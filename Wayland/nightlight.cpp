#include "./nightlight.hpp"

#include "qwayland-wlr-gamma-control-unstable-v1.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QScreen>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <private/qwaylandscreen_p.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace ryu::wl {

namespace {

class GammaManager : public QWaylandClientExtensionTemplate<GammaManager>,
                     public QtWayland::zwlr_gamma_control_manager_v1 {
public:
  GammaManager() : QWaylandClientExtensionTemplate(1) { initialize(); }

  static GammaManager *instance() {
    static GammaManager *manager = new GammaManager;
    return manager;
  }
};

struct Rgb {
  double r;
  double g;
  double b;
};

Rgb helland(double kelvin) {
  const double t = std::clamp(kelvin, 1000.0, 40000.0) / 100.0;
  double r = 255.0;
  double g = 255.0;
  double b = 255.0;
  if (t > 66.0) {
    r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
    g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
  } else {
    g = 99.4708025861 * std::log(t) - 161.1195681661;
  }
  if (t < 66.0) {
    b = t <= 19.0 ? 0.0 : 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
  }
  return {std::clamp(r, 0.0, 255.0) / 255.0, std::clamp(g, 0.0, 255.0) / 255.0,
          std::clamp(b, 0.0, 255.0) / 255.0};
}

Rgb whitepoint(double kelvin) {
  const Rgb c = helland(kelvin);
  const Rgb ref = helland(6500.0);
  return {std::min(1.0, c.r / ref.r), std::min(1.0, c.g / ref.g),
          std::min(1.0, c.b / ref.b)};
}

} // namespace

class GammaOutput : public QtWayland::zwlr_gamma_control_v1 {
public:
  explicit GammaOutput(struct ::zwlr_gamma_control_v1 *object)
      : QtWayland::zwlr_gamma_control_v1(object) {}

  ~GammaOutput() override {
    if (object()) {
      destroy();
    }
  }

  uint32_t size = 0;
  bool failed = false;
  std::function<void(GammaOutput *)> onSize;
  std::function<void()> onFailed;

protected:
  void zwlr_gamma_control_v1_gamma_size(uint32_t value) override {
    size = value;
    if (onSize) {
      onSize(this);
    }
  }

  void zwlr_gamma_control_v1_failed() override {
    failed = true;
    if (onFailed) {
      onFailed();
    }
  }
};

} // namespace ryu::wl

using ryu::wl::GammaManager;
using ryu::wl::GammaOutput;

NightLight::NightLight(QObject *parent) : QObject(parent) {
  connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *s) {
    if (m_active) {
      attach(s);
    }
  });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this,
          [this](QScreen *s) { detach(s); });
}

NightLight::~NightLight() { release(); }

void NightLight::setActive(bool on) {
  if (m_active == on) {
    return;
  }
  m_active = on;
  if (on) {
    engage();
  } else {
    release();
  }
  emit activeChanged();
}

void NightLight::setTemperature(qreal kelvin) {
  if (qFuzzyCompare(m_temperature + 1.0, kelvin + 1.0)) {
    return;
  }
  m_temperature = kelvin;
  emit temperatureChanged();
  schedule();
}

void NightLight::setGamma(qreal value) {
  if (qFuzzyCompare(m_gamma + 1.0, value + 1.0)) {
    return;
  }
  m_gamma = value;
  emit gammaChanged();
  schedule();
}

void NightLight::engage() {
  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    attach(screen);
  }
}

void NightLight::release() {
  qDeleteAll(m_outputs);
  m_outputs.clear();
}

void NightLight::attach(QScreen *screen) {
  if (m_outputs.contains(screen)) {
    return;
  }
  GammaManager *manager = GammaManager::instance();
  if (!manager->isActive()) {
    if (!m_warned) {
      m_warned = true;
      qWarning("ryu nightlight: zwlr_gamma_control_manager_v1 unavailable");
    }
    return;
  }
  auto *native =
      dynamic_cast<QtWaylandClient::QWaylandScreen *>(screen->handle());
  if (!native || !native->output()) {
    return;
  }
  auto *output = new GammaOutput(manager->get_gamma_control(native->output()));
  const QString name = screen->name();
  output->onSize = [this](GammaOutput *o) { push(o); };
  output->onFailed = [name]() {
    qWarning("ryu nightlight: gamma control refused on %s (another client may "
             "hold it)",
             qPrintable(name));
  };
  m_outputs.insert(screen, output);
}

void NightLight::detach(QScreen *screen) { delete m_outputs.take(screen); }

void NightLight::schedule() {
  if (m_scheduled || !m_active) {
    return;
  }
  m_scheduled = true;
  QMetaObject::invokeMethod(this, &NightLight::flush, Qt::QueuedConnection);
}

void NightLight::flush() {
  m_scheduled = false;
  if (!m_active) {
    return;
  }
  for (GammaOutput *output : std::as_const(m_outputs)) {
    push(output);
  }
}

void NightLight::push(GammaOutput *output) {
  if (!m_active || output->failed || output->size < 2) {
    return;
  }
  const size_t size = output->size;
  const ryu::wl::Rgb white = ryu::wl::whitepoint(m_temperature);
  const double level = std::clamp(double(m_gamma), 0.0, 1.0);

  std::vector<uint16_t> ramp(size * 3);
  const double factors[3] = {white.r * level, white.g * level, white.b * level};
  for (size_t channel = 0; channel < 3; ++channel) {
    for (size_t i = 0; i < size; ++i) {
      const double v = double(i) / double(size - 1) * factors[channel];
      ramp[channel * size + i] =
          uint16_t(std::lround(std::clamp(v, 0.0, 1.0) * 65535.0));
    }
  }

  const int fd = memfd_create("ryu-gamma", MFD_CLOEXEC);
  if (fd < 0) {
    qWarning("ryu nightlight: memfd_create failed");
    return;
  }
  const size_t bytes = ramp.size() * sizeof(uint16_t);
  if (pwrite(fd, ramp.data(), bytes, 0) != ssize_t(bytes)) {
    qWarning("ryu nightlight: writing ramp failed");
    close(fd);
    return;
  }
  output->set_gamma(fd);
  close(fd);
}
