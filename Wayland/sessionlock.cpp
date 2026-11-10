#include "./sessionlock.hpp"
#include "qwayland-ext-session-lock-v1.h"
#include "wayland-ext-session-lock-v1-client-protocol.h"
#include <QGuiApplication>
#include <QHash>
#include <QMetaObject>
#include <QQmlContext>
#include <QScreen>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <algorithm>
#include <cstdint>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandscreen_p.h>
#include <private/qwaylandshellintegration_p.h>
#include <private/qwaylandshellsurface_p.h>
#include <private/qwaylandshm_p.h>
#include <private/qwaylandwindow_p.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace ryu::wl {

class LockIntegration;

struct Prefilled {
  QSize logical;
  int scale = 1;
  QRgb color = 0;
  ::wl_buffer *buffer = nullptr;
};

static QHash<QString, Prefilled> &prefillCache() {
  static QHash<QString, Prefilled> cache;
  return cache;
}

static ::wl_buffer *makeBuffer(::wl_shm *shm, const QSize &logical, int scale,
                               QRgb color) {
  const int bw = logical.width() * scale;
  const int bh = logical.height() * scale;
  const int stride = bw * 4;
  const size_t size = size_t(stride) * size_t(bh);

  const int fd = memfd_create("ryu-lock", MFD_CLOEXEC);
  if (fd < 0) {
    qWarning("ryu wayland: memfd_create failed");
    return nullptr;
  }
  if (ftruncate(fd, off_t(size)) != 0) {
    qWarning("ryu wayland: ftruncate failed");
    close(fd);
    return nullptr;
  }
  void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    qWarning("ryu wayland: mmap failed");
    close(fd);
    return nullptr;
  }
  std::fill_n(static_cast<uint32_t *>(data), size / 4, uint32_t(color));
  munmap(data, size);

  ::wl_shm_pool *pool = wl_shm_create_pool(shm, fd, int(size));
  ::wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, bw, bh, stride,
                                                  WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
  close(fd);
  return buffer;
}

static void prefill(QScreen *screen, QRgb color) {
  auto *waylandScreen =
      dynamic_cast<QtWaylandClient::QWaylandScreen *>(screen->handle());
  if (!waylandScreen) {
    return;
  }
  const QSize logical = screen->geometry().size();
  const int scale = qMax(1, qRound(screen->devicePixelRatio()));
  Prefilled &entry = prefillCache()[screen->name()];
  if (entry.buffer && entry.logical == logical && entry.scale == scale &&
      entry.color == color) {
    return;
  }
  if (entry.buffer) {
    wl_buffer_destroy(entry.buffer);
    entry.buffer = nullptr;
  }
  entry.logical = logical;
  entry.scale = scale;
  entry.color = color;
  entry.buffer = makeBuffer(waylandScreen->display()->shm()->object(), logical,
                            scale, color);
}

class LockObject : public QtWayland::ext_session_lock_v1 {
public:
  explicit LockObject(WlSessionLock *owner) : m_owner(owner) {}

protected:
  void ext_session_lock_v1_locked() override {
    if (m_owner) {
      m_owner->handleLocked();
    }
  }

  void ext_session_lock_v1_finished() override {
    if (m_owner) {
      QMetaObject::invokeMethod(m_owner.data(), &WlSessionLock::handleFinished,
                                Qt::QueuedConnection);
    }
  }

private:
  QPointer<WlSessionLock> m_owner;
};

class LockSurfaceRole : public QtWaylandClient::QWaylandShellSurface,
                        public QtWayland::ext_session_lock_surface_v1 {
public:
  LockSurfaceRole(LockIntegration *shell,
                  QtWaylandClient::QWaylandWindow *window);
  ~LockSurfaceRole() override;

  bool isExposed() const override { return m_configured; }
  bool commitSurfaceRole() const override { return false; }

  void applyConfigure() override {
    window()->resizeFromApplyConfigure(m_pending);
    paintFirst();
    window()->updateExposure();
    if (m_owner) {
      m_owner->setReady();
    }
  }

protected:
  void ext_session_lock_surface_v1_configure(uint32_t serial, uint32_t width,
                                             uint32_t height) override {
    ack_configure(serial);
    m_pending = QSize(int(width), int(height));
    m_configured = true;
    if (!m_applied) {
      m_applied = true;
      applyConfigure();
    } else {
      window()->applyConfigureWhenPossible();
    }
  }

private:
  void paintFirst();

  QPointer<WlSessionLockSurface> m_owner;
  QSize m_pending;
  ::wl_buffer *m_ownedBuffer = nullptr;
  bool m_configured = false;
  bool m_painted = false;
  bool m_applied = false;
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
}

LockSurfaceRole::~LockSurfaceRole() {
  if (object()) {
    destroy();
  }
  if (m_ownedBuffer) {
    wl_buffer_destroy(m_ownedBuffer);
  }
}

void LockSurfaceRole::paintFirst() {
  if (m_painted || !m_owner || m_pending.isEmpty()) {
    return;
  }
  const int scale = qMax(1, qRound(m_owner->devicePixelRatio()));
  const QScreen *screen = m_owner->QWindow::screen();

  ::wl_buffer *buffer = nullptr;
  if (screen) {
    const auto it = prefillCache().constFind(screen->name());
    if (it != prefillCache().constEnd() && it->buffer &&
        it->logical == m_pending && it->scale == scale) {
      buffer = it->buffer;
    }
  }
  if (!buffer) {
    m_ownedBuffer = makeBuffer(window()->display()->shm()->object(), m_pending,
                               scale, m_owner->color().rgb());
    buffer = m_ownedBuffer;
  }
  if (!buffer) {
    return;
  }

  ::wl_surface *surface = window()->wlSurface();
  if (scale > 1) {
    wl_surface_set_buffer_scale(surface, scale);
  }
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_damage(surface, 0, 0, m_pending.width(), m_pending.height());
  wl_surface_commit(surface);
  m_painted = true;
}

} // namespace ryu::wl

WlSessionLockSurface::WlSessionLockSurface(QWindow *parent)
    : QQuickWindow(parent) {}

WlSessionLockSurface::~WlSessionLockSurface() { destroy(); }

void WlSessionLockSurface::componentComplete() { present(); }

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
  if (QWindow::screen() != target) {
    QWindow::setScreen(target);
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

WlSessionLock::WlSessionLock(QObject *parent) : QObject(parent) {}

WlSessionLock::~WlSessionLock() {
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
}

void WlSessionLock::setColor(const QColor &color) {
  if (m_color == color) {
    return;
  }
  m_color = color;
  emit colorChanged();
}

void WlSessionLock::setSurface(QQmlComponent *surface) {
  if (m_surface.data() == surface) {
    return;
  }
  m_surface = surface;
  emit surfaceChanged();
}

void WlSessionLock::setLocked(bool locked) {
  if (m_locked == locked) {
    return;
  }
  if (locked) {
    if (!engage()) {
      emit finished();
      return;
    }
    m_locked = true;
  } else {
    teardown(true);
    m_locked = false;
  }
  emit lockedChanged();
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

  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    ryu::wl::prefill(screen, m_color.rgb());
  }

  m_obj = new ryu::wl::LockObject(this);
  m_obj->init(integration->lock());
  integration->setActiveLock(m_obj);

  connect(qGuiApp, &QGuiApplication::screenAdded, this,
          [this](QScreen *screen) { createSurface(screen); });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this,
          [this](QScreen *screen) { dropSurface(screen); });

  for (QScreen *screen : screens) {
    createSurface(screen);
  }
  return true;
}

void WlSessionLock::teardown(bool unlock) {
  disconnect(qGuiApp, nullptr, this, nullptr);
  ryu::wl::LockIntegration::instance()->setActiveLock(nullptr);

  if (m_obj) {
    if (m_obj->object()) {
      if (unlock && m_secure) {
        m_obj->unlock_and_destroy();
      } else {
        m_obj->destroy();
      }
    }
    delete m_obj;
    m_obj = nullptr;
  }

  const auto windows = std::exchange(m_windows, {});
  for (const QPointer<WlSessionLockSurface> &window : windows) {
    if (window) {
      window->destroy();
      window->deleteLater();
    }
  }

  if (m_secure) {
    m_secure = false;
    emit secureChanged();
  }
}

void WlSessionLock::createSurface(QScreen *screen) {
  if (!m_obj || !m_surface) {
    return;
  }
  QObject *object = m_surface->createWithInitialProperties(
      {{QStringLiteral("screen"), QVariant::fromValue<QObject *>(screen)}},
      qmlContext(this));
  if (!object) {
    qWarning("ryu wayland: lock surface component failed: %s",
             qPrintable(m_surface->errorString()));
    return;
  }
  auto *window = qobject_cast<WlSessionLockSurface *>(object);
  if (!window) {
    qWarning("ryu wayland: lock surface component root must be "
             "WlSessionLockSurface");
    delete object;
    return;
  }
  m_windows.append(window);
}

void WlSessionLock::dropSurface(QScreen *screen) {
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
