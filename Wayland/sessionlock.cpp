#include "./sessionlock.hpp"
#include "qwayland-ext-session-lock-v1.h"
#include "wayland-ext-session-lock-v1-client-protocol.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QMetaObject>
#include <QQmlContext>
#include <QSGImageNode>
#include <QSGTexture>
#include <QScreen>
#include <QThreadPool>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <cstring>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>
#include <private/qwaylandscreen_p.h>
#include <private/qwaylandshellintegration_p.h>
#include <private/qwaylandshellsurface_p.h>
#include <private/qwaylandshmbackingstore_p.h>
#include <private/qwaylandwindow_p.h>
#include <utility>

namespace ryu::wl {

static QElapsedTimer &lockClock() {
  static QElapsedTimer clock;
  return clock;
}

static void startClock() { lockClock().restart(); }

static qint64 stamp() {
  return lockClock().isValid() ? lockClock().elapsed() : -1;
}

static bool debugOn() {
  static const bool on =
      qEnvironmentVariable("RYU_LOCK_DEBUG", QStringLiteral("1")) !=
      QLatin1String("0");
  return on;
}

#define LOCKLOG(fmt, ...)                                                      \
  do {                                                                         \
    if (ryu::wl::debugOn()) {                                                  \
      qInfo("lock +%lldms " fmt,                                               \
            (long long)ryu::wl::stamp() __VA_OPT__(, ) __VA_ARGS__);           \
    }                                                                          \
  } while (0)

static QString nameOf(const QObject *object) {
  return object ? object->property("name").toString() : QString();
}

static QHash<QString, QImage> &backdrops() {
  static QHash<QString, QImage> images;
  return images;
}

QImage backdropFor(const QString &name) { return backdrops().value(name); }

static QColor &defaultColor() {
  static QColor color = Qt::black;
  return color;
}

static bool validScreen(QScreen *screen) {
  auto *waylandScreen =
      dynamic_cast<QtWaylandClient::QWaylandScreen *>(screen->handle());
  return waylandScreen && !waylandScreen->isPlaceholder() &&
         waylandScreen->output() != nullptr;
}

static void flushDisplay() {
  auto *integration = QtWaylandClient::QWaylandIntegration::instance();
  if (integration && integration->display()) {
    wl_display_flush(integration->display()->wl_display());
  }
}

static QImage decodeBackdrop(const QString &path, const QSize &target) {
  QImage image(path);
  if (image.isNull()) {
    return image;
  }
  image.convertTo(QImage::Format_RGB32);
  if (target.isEmpty() || image.size() == target) {
    return image;
  }
  image = image.scaled(target, Qt::KeepAspectRatioByExpanding,
                       Qt::SmoothTransformation);
  if (image.size() != target) {
    image = image.copy((image.width() - target.width()) / 2,
                       (image.height() - target.height()) / 2, target.width(),
                       target.height());
  }
  return image;
}

class LockObject : public QtWayland::ext_session_lock_v1 {
public:
  explicit LockObject(WlSessionLock *owner) : m_owner(owner) {}

protected:
  void ext_session_lock_v1_locked() override {
    LOCKLOG("compositor event: locked");
    if (m_owner) {
      m_owner->handleLocked();
    }
  }

  void ext_session_lock_v1_finished() override {
    LOCKLOG("compositor event: finished");
    if (m_owner) {
      QMetaObject::invokeMethod(m_owner.data(), &WlSessionLock::handleFinished,
                                Qt::QueuedConnection);
    }
  }

private:
  QPointer<WlSessionLock> m_owner;
};

class LockIntegration;

class LockSurfaceRole : public QtWaylandClient::QWaylandShellSurface,
                        public QtWayland::ext_session_lock_surface_v1 {
public:
  LockSurfaceRole(LockIntegration *shell,
                  QtWaylandClient::QWaylandWindow *window);
  ~LockSurfaceRole() override;

  bool isExposed() const override { return m_configured && m_released; }
  bool commitSurfaceRole() const override { return false; }

  void applyConfigure() override {
    window()->resizeFromApplyConfigure(m_pending);
  }

protected:
  void ext_session_lock_surface_v1_configure(uint32_t serial, uint32_t width,
                                             uint32_t height) override {
    ack_configure(serial);
    m_pending = QSize(int(width), int(height));
    LOCKLOG(
        "configure %s serial=%u %ux%u first=%d",
        qPrintable(nameOf(m_owner.data() ? m_owner->screenSource() : nullptr)),
        serial, width, height, int(!m_configured));
    window()->resizeFromApplyConfigure(m_pending);
    if (m_configured) {
      return;
    }
    m_configured = true;
    paintPlaceholder();
    if (m_owner) {
      m_owner->setReady();
    }
    release();
  }

private:
  void paintPlaceholder();
  void dropPlaceholder();
  void release();

  QPointer<WlSessionLockSurface> m_owner;
  QtWaylandClient::QWaylandShmBuffer *m_shm = nullptr;
  QSize m_pending;
  bool m_configured = false;
  bool m_released = false;
};

class LockIntegration
    : public QtWaylandClient::QWaylandShellIntegrationTemplate<LockIntegration>,
      public QtWayland::ext_session_lock_manager_v1 {
public:
  LockIntegration()
      : QtWaylandClient::QWaylandShellIntegrationTemplate<LockIntegration>(1) {
    QWaylandClientExtension::initialize();
  }

  static LockIntegration *instance() {
    static LockIntegration *integration = new LockIntegration;
    return integration;
  }

  QtWaylandClient::QWaylandShellSurface *
  createShellSurface(QtWaylandClient::QWaylandWindow *window) override {
    return new LockSurfaceRole(this, window);
  }

  QtWayland::ext_session_lock_v1 *activeLock() const { return m_lock; }
  void setActiveLock(QtWayland::ext_session_lock_v1 *lock) { m_lock = lock; }

private:
  QtWayland::ext_session_lock_v1 *m_lock = nullptr;
};

LockSurfaceRole::LockSurfaceRole(LockIntegration *shell,
                                 QtWaylandClient::QWaylandWindow *window)
    : QtWaylandClient::QWaylandShellSurface(window),
      m_owner(qobject_cast<WlSessionLockSurface *>(window->window())) {
  QtWayland::ext_session_lock_v1 *lock = shell->activeLock();
  if (!m_owner || !lock) {
    qWarning("ryu wayland: lock surface created without an active lock");
    return;
  }
  ::wl_output *output =
      window->waylandScreen() ? window->waylandScreen()->output() : nullptr;
  if (!output) {
    qWarning("ryu wayland: lock surface has no output");
    return;
  }
  init(lock->get_lock_surface(window->wlSurface(), output));
  LOCKLOG("role created %s", qPrintable(nameOf(m_owner->screenSource())));
}

LockSurfaceRole::~LockSurfaceRole() {
  delete m_shm;
  m_shm = nullptr;
  if (object()) {
    destroy();
  }
}

void LockSurfaceRole::paintPlaceholder() {
  QThread::msleep(12);
  if (!m_owner || m_pending.isEmpty()) {
    return;
  }
  QElapsedTimer timer;
  timer.start();

  const QString name = nameOf(m_owner->screenSource());
  const int scale = qMax(1, qRound(m_owner->devicePixelRatio()));
  const QSize px(m_pending.width() * scale, m_pending.height() * scale);

  QImage src = backdropFor(name);
  const bool hit = !src.isNull() && src.size() == px;
  if (!src.isNull() && !hit) {
    src =
        src.scaled(px, Qt::KeepAspectRatioByExpanding, Qt::FastTransformation);
  }

  m_shm = new QtWaylandClient::QWaylandShmBuffer(window()->display(), px,
                                                 QImage::Format_RGB32, scale);
  QImage *dst = m_shm->image();
  if (!hit)
    dst->fill(m_owner->color());

  if (!src.isNull()) {
    const int ox = (src.width() - px.width()) / 2;
    const int oy = (src.height() - px.height()) / 2;
    for (int y = 0; y < px.height(); ++y) {
      std::memcpy(dst->scanLine(y), src.constScanLine(y + oy) + size_t(ox) * 4,
                  size_t(px.width()) * 4);
    }
  }

  ::wl_surface *surface = window()->wlSurface();
  if (scale > 1) {
    wl_surface_set_buffer_scale(surface, scale);
  }
  wl_surface_attach(surface, m_shm->buffer(), 0, 0);
  wl_surface_damage(surface, 0, 0, m_pending.width(), m_pending.height());
  wl_surface_commit(surface);
  wl_display_flush(window()->display()->wl_display());

  LOCKLOG("placeholder committed %s px=%dx%d scale=%d image=%s exact=%d "
          "took=%lldms",
          qPrintable(name), px.width(), px.height(), scale,
          src.isNull() ? "none" : "yes", int(hit), (long long)timer.elapsed());

  QObject::connect(
      m_owner.data(), &WlSessionLockSurface::paintedChanged, this,
      [this]() { dropPlaceholder(); }, Qt::SingleShotConnection);
}

void LockSurfaceRole::dropPlaceholder() {
  if (!m_shm) {
    return;
  }
  delete m_shm;
  m_shm = nullptr;
  LOCKLOG("placeholder freed %s",
          qPrintable(nameOf(m_owner ? m_owner->screenSource() : nullptr)));
}

void LockSurfaceRole::release() {
  if (m_released) {
    return;
  }
  m_released = true;
  if (m_owner) {
    m_owner->markRevealed();
  }
  LOCKLOG("release %s",
          qPrintable(nameOf(m_owner ? m_owner->screenSource() : nullptr)));
  window()->updateExposure();
}

} // namespace ryu::wl

LockBackdrop::LockBackdrop(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

void LockBackdrop::setScreenName(const QString &name) {
  if (m_screenName == name) {
    return;
  }
  m_screenName = name;
  m_image = ryu::wl::backdropFor(name);
  m_textureDirty = true;
  LOCKLOG("backdrop bound %s image=%dx%d", qPrintable(name), m_image.width(),
          m_image.height());
  emit screenNameChanged();
  emit hasImageChanged();
  update();
}

void LockBackdrop::geometryChange(const QRectF &next, const QRectF &previous) {
  QQuickItem::geometryChange(next, previous);
  update();
}

QSGNode *LockBackdrop::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
  if (m_image.isNull() || !window() || width() <= 0 || height() <= 0) {
    delete old;
    m_textureDirty = true;
    return nullptr;
  }

  auto *node = static_cast<QSGImageNode *>(old);
  if (!node) {
    node = window()->createImageNode();
    node->setOwnsTexture(true);
    m_textureDirty = true;
  }

  if (m_textureDirty) {
    node->setTexture(window()->createTextureFromImage(
        m_image, QQuickWindow::TextureIsOpaque));
    m_textureDirty = false;
    LOCKLOG("backdrop texture uploaded %s", qPrintable(m_screenName));
  }

  const QSizeF img = m_image.size();
  const qreal s = qMax(width() / img.width(), height() / img.height());
  const QSizeF visible(width() / s, height() / s);
  node->setFiltering(QSGTexture::Linear);
  node->setRect(boundingRect());
  node->setSourceRect(QRectF((img.width() - visible.width()) / 2,
                             (img.height() - visible.height()) / 2,
                             visible.width(), visible.height()));
  return node;
}

WlSessionLockSurface::WlSessionLockSurface(QWindow *parent)
    : QQuickWindow(parent) {
  qInfo() << rendererInterface()->graphicsApi();
  setColor(Qt::black);
  connect(this, &QQuickWindow::frameSwapped, this, [this]() {
    if (m_frames < 4) {
      ++m_frames;
      LOCKLOG("frame #%d %s %dx%d dpr=%.2f revealed=%d", m_frames,
              qPrintable(ryu::wl::nameOf(m_screen.data())), width(), height(),
              devicePixelRatio(), int(m_revealed));
    }
    if (!m_revealed || m_painted || width() <= 0) {
      return;
    }
    m_painted = true;
    LOCKLOG("painted %s", qPrintable(ryu::wl::nameOf(m_screen.data())));
    emit paintedChanged();
  });
}

WlSessionLockSurface::~WlSessionLockSurface() { destroy(); }

void WlSessionLockSurface::classBegin() { setColor(ryu::wl::defaultColor()); }

void WlSessionLockSurface::setDefaultColor(const QColor &color) {
  ryu::wl::defaultColor() = color;
}

void WlSessionLockSurface::log(const QString &message) const {
  LOCKLOG("qml %s: %s", qPrintable(ryu::wl::nameOf(m_screen.data())),
          qPrintable(message));
}

QQmlListProperty<QObject> WlSessionLockSurface::contents() {
  return QQmlListProperty<QObject>(
      this, nullptr, &WlSessionLockSurface::appendContent,
      &WlSessionLockSurface::contentCount, &WlSessionLockSurface::contentAt,
      &WlSessionLockSurface::clearContents);
}

void WlSessionLockSurface::appendContent(QQmlListProperty<QObject> *list,
                                         QObject *object) {
  auto *self = static_cast<WlSessionLockSurface *>(list->object);
  self->m_contents.append(object);
  if (auto *item = qobject_cast<QQuickItem *>(object)) {
    item->setParentItem(self->contentItem());
  } else {
    object->setParent(self);
  }
}

qsizetype WlSessionLockSurface::contentCount(QQmlListProperty<QObject> *list) {
  return static_cast<WlSessionLockSurface *>(list->object)->m_contents.size();
}

QObject *WlSessionLockSurface::contentAt(QQmlListProperty<QObject> *list,
                                         qsizetype index) {
  return static_cast<WlSessionLockSurface *>(list->object)
      ->m_contents.at(index);
}

void WlSessionLockSurface::clearContents(QQmlListProperty<QObject> *list) {
  static_cast<WlSessionLockSurface *>(list->object)->m_contents.clear();
}

void WlSessionLockSurface::setScreenSource(QObject *screen) {
  if (m_screen.data() == screen) {
    return;
  }
  m_screen = screen;
  emit screenSourceChanged();
}

void WlSessionLockSurface::setReady() {
  if (m_ready) {
    return;
  }
  m_ready = true;
  emit readyChanged();
}

void WlSessionLockSurface::markRevealed() { m_revealed = true; }

QScreen *WlSessionLockSurface::resolveScreen() const {
  QObject *source = m_screen.data();
  if (!source) {
    return nullptr;
  }
  if (auto *screen = qobject_cast<QScreen *>(source)) {
    return screen;
  }
  const QString name = source->property("name").toString();
  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    if (screen->name() == name) {
      return screen;
    }
  }
  return nullptr;
}

void WlSessionLockSurface::present() {
  if (m_presented) {
    return;
  }
  ryu::wl::LockIntegration *integration = ryu::wl::LockIntegration::instance();
  if (!integration->isActive()) {
    qWarning("ryu wayland: ext_session_lock_manager_v1 is not available");
    return;
  }
  QScreen *target = resolveScreen();
  if (!target) {
    qWarning("ryu wayland: lock surface output not found");
    return;
  }
  QWindow::setScreen(target);
  create();
  QWindow::setScreen(target);
  auto *platform = dynamic_cast<QtWaylandClient::QWaylandWindow *>(handle());
  if (!platform) {
    qWarning("ryu wayland: not running on the wayland platform plugin");
    return;
  }
  platform->setShellIntegration(integration);
  m_presented = true;
  LOCKLOG("present %s", qPrintable(target->name()));
  setVisible(true);
}

WlSessionLock::WlSessionLock(QObject *parent) : QObject(parent) {}

WlSessionLock::~WlSessionLock() {
  ++m_generation;
  disconnect(qGuiApp, nullptr, this, nullptr);
  ryu::wl::LockIntegration::instance()->setActiveLock(nullptr);
  if (m_obj && !m_secure) {
    if (m_obj->object()) {
      m_obj->destroy();
    }
    delete m_obj;
  }
  m_obj = nullptr;
  for (const QPointer<WlSessionLockSurface> &window :
       std::as_const(m_windows)) {
    if (window) {
      window->destroy();
      delete window.data();
    }
  }
  m_windows.clear();
  ryu::wl::backdrops().clear();
}

void WlSessionLock::setColor(const QColor &color) {
  if (m_color == color) {
    return;
  }
  m_color = color;
  emit colorChanged();
}

void WlSessionLock::setBackgrounds(const QVariantMap &backgrounds) {
  if (m_backgrounds == backgrounds) {
    return;
  }
  m_backgrounds = backgrounds;
  emit backgroundsChanged();
}

void WlSessionLock::setSurface(QQmlComponent *surface) {
  if (m_surface.data() == surface) {
    return;
  }
  m_surface = surface;
  emit surfaceChanged();
}

void WlSessionLock::setLocked(bool locked) {
  if (locked) {
    if (m_state != Idle) {
      return;
    }
    ryu::wl::startClock();
    LOCKLOG("lock requested");
    ++m_generation;
    m_state = Preparing;
    m_locked = true;
    emit lockedChanged();
    if (!prepare()) {
      beginLock();
    }
    return;
  }
  if (m_state == Idle) {
    return;
  }
  LOCKLOG("unlock requested secure=%d", int(m_secure));
  ++m_generation;
  teardown(true);
  m_locked = false;
  emit lockedChanged();
}

bool WlSessionLock::prepare() {
  struct Job {
    QString name;
    QString path;
    QSize target;
  };

  QList<Job> jobs;
  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    if (!ryu::wl::validScreen(screen)) {
      continue;
    }
    const QString path = m_backgrounds.value(screen->name()).toString();
    if (path.isEmpty()) {
      continue;
    }
    const int scale = qMax(1, qRound(screen->devicePixelRatio()));
    const QSize logical = screen->geometry().size();
    jobs.append({screen->name(), path,
                 QSize(logical.width() * scale, logical.height() * scale)});
  }

  ryu::wl::backdrops().clear();
  m_loading = int(jobs.size());
  if (jobs.isEmpty()) {
    LOCKLOG("no backgrounds to decode");
    return false;
  }

  LOCKLOG("decoding %d backgrounds on worker threads", m_loading);
  const quint64 generation = m_generation;
  QPointer<WlSessionLock> guard(this);
  for (const Job &job : std::as_const(jobs)) {
    QThreadPool::globalInstance()->start([guard, job, generation]() {
      QElapsedTimer timer;
      timer.start();
      const QImage image = ryu::wl::decodeBackdrop(job.path, job.target);
      const qint64 ms = timer.elapsed();
      if (!guard) {
        return;
      }
      QMetaObject::invokeMethod(
          guard.data(),
          [guard, job, generation, image, ms]() {
            if (guard) {
              guard->handleLoaded(generation, job.name, image, ms);
            }
          },
          Qt::QueuedConnection);
    });
  }
  return true;
}

void WlSessionLock::handleLoaded(quint64 generation, const QString &name,
                                 const QImage &image, qint64 ms) {
  if (generation != m_generation || m_state != Preparing) {
    return;
  }
  if (image.isNull()) {
    LOCKLOG("decode FAILED %s", qPrintable(name));
  } else {
    ryu::wl::backdrops().insert(name, image);
    LOCKLOG("decoded %s %dx%d in %lldms", qPrintable(name), image.width(),
            image.height(), (long long)ms);
  }
  if (--m_loading <= 0) {
    beginLock();
  }
}

void WlSessionLock::beginLock() {
  if (m_state != Preparing) {
    return;
  }
  if (!engage()) {
    fail();
  }
}

void WlSessionLock::fail() {
  LOCKLOG("lock failed");
  teardown(false);
  m_locked = false;
  emit lockedChanged();
  emit finished();
}

WlSessionLockSurface *WlSessionLock::buildSurface(QScreen *screen) {
  WlSessionLockSurface::setDefaultColor(m_color);
  QObject *object = m_surface->createWithInitialProperties(
      {{QStringLiteral("screen"), QVariant::fromValue<QObject *>(screen)}},
      qmlContext(this));
  if (!object) {
    qWarning("ryu wayland: lock surface component failed: %s",
             qPrintable(m_surface->errorString()));
    return nullptr;
  }
  auto *window = qobject_cast<WlSessionLockSurface *>(object);
  if (!window) {
    qWarning("ryu wayland: lock surface component root must be "
             "WlSessionLockSurface");
    delete object;
    return nullptr;
  }
  return window;
}

WlSessionLockSurface *WlSessionLock::windowFor(QScreen *screen) const {
  for (const QPointer<WlSessionLockSurface> &window : m_windows) {
    if (window && window->screenSource() == screen) {
      return window.data();
    }
  }
  return nullptr;
}

void WlSessionLock::discardWindows() {
  const auto windows = std::exchange(m_windows, {});
  for (const QPointer<WlSessionLockSurface> &window : windows) {
    if (window) {
      window->destroy();
      window->deleteLater();
    }
  }
}

bool WlSessionLock::engage() {
  ryu::wl::LockIntegration *integration = ryu::wl::LockIntegration::instance();
  if (!integration->isActive()) {
    qWarning("ryu wayland: ext_session_lock_manager_v1 is not available");
    return false;
  }
  if (!m_surface) {
    qWarning("ryu wayland: session lock has no surface component");
    return false;
  }

  m_state = Locking;
  LOCKLOG("engaging, creating surfaces");

  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    if (!ryu::wl::validScreen(screen)) {
      continue;
    }
    if (auto *window = buildSurface(screen)) {
      m_windows.append(window);
    }
  }

  if (m_windows.isEmpty()) {
    qWarning("ryu wayland: no usable outputs for session lock");
    return false;
  }

  m_obj = new ryu::wl::LockObject(this);
  m_obj->init(integration->lock());
  integration->setActiveLock(m_obj);
  LOCKLOG("lock object created, %lld surfaces", (long long)m_windows.size());

  connect(qGuiApp, &QGuiApplication::screenAdded, this,
          [this](QScreen *screen) { addScreen(screen); });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this,
          [this](QScreen *screen) { dropSurface(screen); });

  for (const QPointer<WlSessionLockSurface> &window :
       std::as_const(m_windows)) {
    if (window) {
      window->present();
    }
  }
  return true;
}

void WlSessionLock::teardown(bool unlock) {
  disconnect(qGuiApp, nullptr, this, nullptr);
  ryu::wl::LockIntegration::instance()->setActiveLock(nullptr);
  if (m_obj) {
    if (m_obj->object()) {
      if (unlock && m_secure) {
        LOCKLOG("unlock_and_destroy");
        m_obj->unlock_and_destroy();
      } else {
        LOCKLOG("destroy lock object");
        m_obj->destroy();
      }
    }
    delete m_obj;
    m_obj = nullptr;
  }
  ryu::wl::flushDisplay();
  discardWindows();
  ryu::wl::backdrops().clear();
  m_loading = 0;
  m_state = Idle;
  if (m_secure) {
    m_secure = false;
    emit secureChanged();
  }
  LOCKLOG("teardown complete, images and windows released");
}

void WlSessionLock::addScreen(QScreen *screen) {
  if (!m_obj || !m_surface || !ryu::wl::validScreen(screen) ||
      windowFor(screen)) {
    return;
  }
  LOCKLOG("screen added during lock %s", qPrintable(screen->name()));
  auto *window = buildSurface(screen);
  if (!window) {
    return;
  }
  m_windows.append(window);
  window->present();
}

void WlSessionLock::dropSurface(QScreen *screen) {
  LOCKLOG("screen removed during lock %s", qPrintable(screen->name()));
  m_windows.removeIf([screen](const QPointer<WlSessionLockSurface> &window) {
    if (!window) {
      return true;
    }
    if (window->screenSource() != screen) {
      return false;
    }
    window->destroy();
    window->deleteLater();
    return true;
  });
}

void WlSessionLock::handleLocked() {
  if (m_secure) {
    return;
  }
  m_secure = true;
  emit secureChanged();
}

void WlSessionLock::handleFinished() {
  teardown(false);
  if (m_locked) {
    m_locked = false;
    emit lockedChanged();
  }
  emit finished();
}
