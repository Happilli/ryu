#include "./layershell.hpp"
#include "qwayland-wlr-layer-shell-unstable-v1.h"
#include "wayland-wlr-layer-shell-unstable-v1-client-protocol.h"
#include <QGuiApplication>
#include <QMetaObject>
#include <QScreen>
#include <QSurfaceFormat>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandscreen_p.h>
#include <private/qwaylandshellintegration_p.h>
#include <private/qwaylandshellsurface_p.h>
#include <private/qwaylandwindow_p.h>
#include <qpa/qwindowsysteminterface.h>
#include <utility>

namespace ryu::wl {

class LayerIntegration;

class LayerSurface : public QtWaylandClient::QWaylandShellSurface,
                     public QtWayland::zwlr_layer_surface_v1 {
public:
  LayerSurface(LayerIntegration *shell,
               QtWaylandClient::QWaylandWindow *window);
  ~LayerSurface() override;

  bool isExposed() const override { return m_configured; }

  void applyConfigure() override {
    window()->resizeFromApplyConfigure(m_pending);
    window()->updateExposure();
    if (m_owner) {
      m_owner->pushInput();
    }
  }

  bool configured() const { return m_configured; }

  void setLayer(uint32_t layer) {
    set_layer(layer);
    commit();
  }
  void setAnchors(uint32_t anchors) {
    set_anchor(anchors);
    commit();
  }
  void setMargins(int top, int right, int bottom, int left) {
    set_margin(top, right, bottom, left);
    commit();
  }
  void setExclusive(int zone) {
    set_exclusive_zone(zone);
    commit();
  }
  void setKeyboard(uint32_t mode) {
    set_keyboard_interactivity(mode);
    commit();
  }
  void syncSize() {
    const QSize wanted = wantedSize();
    if (wanted == m_sent) {
      return;
    }
    m_sent = wanted;
    set_size(uint32_t(wanted.width()), uint32_t(wanted.height()));
    commit();
  }
  void setInput(const QRegion &region) {
    ::wl_region *wlRegion = window()->display()->createRegion(region);
    wl_surface_set_input_region(window()->wlSurface(), wlRegion);
    wl_region_destroy(wlRegion);
    commit();
  }

protected:
  void zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width,
                                       uint32_t height) override {
    ack_configure(serial);
    m_pending = QSize(int(width), int(height));
    m_configured = true;
    window()->applyConfigureWhenPossible();
  }
  void zwlr_layer_surface_v1_closed() override {
    if (m_owner) {
      QMetaObject::invokeMethod(
          m_owner.data(), [owner = m_owner]() { owner->close(); },
          Qt::QueuedConnection);
    }
  }

private:
  QSize wantedSize() const {
    const WlrAnchors *a = m_owner->anchors();
    const int w = (a->left() && a->right()) ? 0 : qMax(1, m_owner->width());
    const int h = (a->top() && a->bottom()) ? 0 : qMax(1, m_owner->height());
    return QSize(w, h);
  }
  void commit() { wl_surface_commit(window()->wlSurface()); }

  QPointer<WlrLayershell> m_owner;
  QSize m_pending;
  QSize m_sent;
  bool m_configured = false;
};

class LayerIntegration
    : public QtWaylandClient::QWaylandShellIntegrationTemplate<
          LayerIntegration>,
      public QtWayland::zwlr_layer_shell_v1 {
public:
  LayerIntegration()
      : QtWaylandClient::QWaylandShellIntegrationTemplate<LayerIntegration>(5) {
    QWaylandClientExtension::initialize();
  }

  static LayerIntegration *instance() {
    static LayerIntegration *integration = new LayerIntegration;
    return integration;
  }

  QtWaylandClient::QWaylandShellSurface *
  createShellSurface(QtWaylandClient::QWaylandWindow *window) override {
    return new LayerSurface(this, window);
  }
};

LayerSurface::LayerSurface(LayerIntegration *shell,
                           QtWaylandClient::QWaylandWindow *window)
    : QtWaylandClient::QWaylandShellSurface(window),
      m_owner(qobject_cast<WlrLayershell *>(window->window())) {
  if (!m_owner) {
    qWarning("ryu wayland: layer surface created for a foreign window");
    return;
  }
  ::wl_output *output =
      window->waylandScreen() ? window->waylandScreen()->output() : nullptr;
  init(shell->get_layer_surface(window->wlSurface(), output,
                                uint32_t(m_owner->layerValue()),
                                m_owner->ns()));
  m_sent = wantedSize();
  set_size(uint32_t(m_sent.width()), uint32_t(m_sent.height()));
  set_anchor(m_owner->anchors()->mask());
  set_exclusive_zone(m_owner->exclusiveZone());
  set_margin(m_owner->margins()->top(), m_owner->margins()->right(),
             m_owner->margins()->bottom(), m_owner->margins()->left());
  set_keyboard_interactivity(uint32_t(m_owner->keyboardFocusValue()));
  m_owner->attachSurface(this);
  commit();
}

LayerSurface::~LayerSurface() {
  if (m_owner) {
    m_owner->detachSurface(this);
  }
  if (object()) {
    destroy();
  }
}

} // namespace ryu::wl

Region::Region(QObject *parent) : QObject(parent) {}

void Region::setItem(QQuickItem *item) {
  if (m_item.data() == item) {
    return;
  }
  m_item = item;
  emit changed();
}

QQmlListProperty<Region> Region::regions() {
  return QQmlListProperty<Region>(this, nullptr, &Region::appendRegion,
                                  &Region::regionCount, &Region::regionAt,
                                  &Region::clearRegions);
}

void Region::appendRegion(QQmlListProperty<Region> *list, Region *region) {
  auto *self = static_cast<Region *>(list->object);
  self->m_regions.append(region);
  connect(region, &Region::changed, self, &Region::changed);
  emit self->changed();
}

qsizetype Region::regionCount(QQmlListProperty<Region> *list) {
  return static_cast<Region *>(list->object)->m_regions.size();
}

Region *Region::regionAt(QQmlListProperty<Region> *list, qsizetype index) {
  return static_cast<Region *>(list->object)->m_regions.at(index);
}

void Region::clearRegions(QQmlListProperty<Region> *list) {
  auto *self = static_cast<Region *>(list->object);
  for (Region *region : std::as_const(self->m_regions)) {
    disconnect(region, nullptr, self, nullptr);
  }
  self->m_regions.clear();
  emit self->changed();
}

QRegion Region::build() const {
  QRegion out;
  QQuickItem *item = m_item.data();
  if (item && item->isVisible()) {
    const QRect rect =
        item->mapRectToScene(QRectF(0, 0, item->width(), item->height()))
            .toAlignedRect();
    if (!rect.isEmpty()) {
      out += rect;
    }
  }
  for (const Region *region : m_regions) {
    out += region->build();
  }
  return out;
}

WlrLayershell::WlrLayershell(QWindow *parent) : QQuickWindow(parent) {
  QSurfaceFormat surface = format();
  surface.setAlphaBufferSize(8);
  setFormat(surface);
  setColor(Qt::transparent);

  m_anchors = new WlrAnchors(this);
  m_margins = new WlrMargins(this);

  connect(m_anchors, &WlrAnchors::changed, this, [this]() {
    if (m_surface) {
      m_surface->setAnchors(m_anchors->mask());
      m_surface->syncSize();
    }
  });
  connect(m_margins, &WlrMargins::changed, this, [this]() {
    if (m_surface) {
      m_surface->setMargins(m_margins->top(), m_margins->right(),
                            m_margins->bottom(), m_margins->left());
    }
  });
  connect(this, &QWindow::widthChanged, this, [this]() {
    if (m_surface) {
      m_surface->syncSize();
    }
  });
  connect(this, &QWindow::heightChanged, this, [this]() {
    if (m_surface) {
      m_surface->syncSize();
    }
  });
  connect(this, &QQuickWindow::afterAnimating, this,
          &WlrLayershell::schedulePush);
}

WlrLayershell::~WlrLayershell() { destroy(); }

void WlrLayershell::componentComplete() {
  m_complete = true;
  present();
}

QQmlListProperty<QObject> WlrLayershell::contents() {
  return QQmlListProperty<QObject>(this, nullptr, &WlrLayershell::appendContent,
                                   &WlrLayershell::contentCount,
                                   &WlrLayershell::contentAt,
                                   &WlrLayershell::clearContents);
}

void WlrLayershell::appendContent(QQmlListProperty<QObject> *list,
                                  QObject *object) {
  auto *self = static_cast<WlrLayershell *>(list->object);
  self->m_contents.append(object);
  if (auto *item = qobject_cast<QQuickItem *>(object)) {
    item->setParentItem(self->contentItem());
  } else {
    object->setParent(self);
  }
}

qsizetype WlrLayershell::contentCount(QQmlListProperty<QObject> *list) {
  return static_cast<WlrLayershell *>(list->object)->m_contents.size();
}

QObject *WlrLayershell::contentAt(QQmlListProperty<QObject> *list,
                                  qsizetype index) {
  return static_cast<WlrLayershell *>(list->object)->m_contents.at(index);
}

void WlrLayershell::clearContents(QQmlListProperty<QObject> *list) {
  static_cast<WlrLayershell *>(list->object)->m_contents.clear();
}

void WlrLayershell::setNs(const QString &ns) {
  if (m_ns == ns) {
    return;
  }
  m_ns = ns;
  emit nsChanged();
  recreate();
}

void WlrLayershell::setScreenSource(QObject *screen) {
  if (m_screen.data() == screen) {
    return;
  }
  m_screen = screen;
  emit screenSourceChanged();
  recreate();
}

void WlrLayershell::setLayerValue(WlrLayer::Enum layer) {
  if (m_layer == layer) {
    return;
  }
  m_layer = layer;
  emit layerChanged();
  if (m_surface) {
    m_surface->setLayer(uint32_t(layer));
  }
}

void WlrLayershell::setKeyboardFocusValue(WlrKeyboardFocus::Enum focus) {
  if (m_keyboard == focus) {
    return;
  }
  m_keyboard = focus;
  emit keyboardFocusChanged();
  if (m_surface) {
    m_surface->setKeyboard(uint32_t(focus));
  }
}

void WlrLayershell::setExclusiveZone(int zone) {
  if (m_exclusive == zone) {
    return;
  }
  m_exclusive = zone;
  emit exclusiveZoneChanged();
  if (m_surface) {
    m_surface->setExclusive(zone);
  }
}

void WlrLayershell::setMaskRegion(Region *region) {
  if (m_mask.data() == region) {
    return;
  }
  if (m_mask) {
    disconnect(m_mask.data(), nullptr, this, nullptr);
  }
  m_mask = region;
  if (region) {
    connect(region, &Region::changed, this, [this]() { update(); });
  }
  m_inputValid = false;
  emit maskChanged();
  update();
}

void WlrLayershell::attachSurface(ryu::wl::LayerSurface *surface) {
  m_surface = surface;
  m_inputValid = false;
}

void WlrLayershell::detachSurface(ryu::wl::LayerSurface *surface) {
  if (m_surface == surface) {
    m_surface = nullptr;
  }
}

void WlrLayershell::schedulePush() {
  if (m_pushQueued) {
    return;
  }
  m_pushQueued = true;
  QMetaObject::invokeMethod(
      this,
      [this]() {
        m_pushQueued = false;
        pushInput();
      },
      Qt::QueuedConnection);
}

void WlrLayershell::pushInput() {
  if (!m_surface || !m_mask || !m_surface->configured()) {
    return;
  }
  const QRegion region = m_mask->build();
  if (m_inputValid && region == m_input) {
    return;
  }
  m_input = region;
  m_inputValid = true;
  m_surface->setInput(region);
}

QScreen *WlrLayershell::resolveScreen() const {
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

void WlrLayershell::present() {
  ryu::wl::LayerIntegration *integration =
      ryu::wl::LayerIntegration::instance();
  if (!integration->isActive()) {
    qWarning("ryu wayland: zwlr_layer_shell_v1 is not available");
    return;
  }

  if (m_screen) {
    QScreen *target = resolveScreen();
    if (!target) {
      qWarning("ryu wayland: layer shell output not found");
      return;
    }
    if (QWindow::screen() != target) {
      QWindow::setScreen(target);
    }
  }

  create();
  auto *platform = dynamic_cast<QtWaylandClient::QWaylandWindow *>(handle());
  if (!platform) {
    qWarning("ryu wayland: not running on the wayland platform plugin");
    return;
  }

  platform->setShellIntegration(integration);
  setVisible(true);
}
void WlrLayershell::recreate() {
  if (!m_complete || !isVisible()) {
    return;
  }
  destroy();
  present();
}
