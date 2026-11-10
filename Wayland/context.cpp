#include "./context.hpp"
#include "qwayland-ext-foreign-toplevel-list-v1.h"
#include "qwayland-ext-image-capture-source-v1.h"
#include "qwayland-ext-image-copy-capture-v1.h"
#include "qwayland-wlr-screencopy-unstable-v1.h"
#include "wayland-wlr-screencopy-unstable-v1-client-protocol.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QScreen>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <cstring>
#include <functional>
#include <private/qwaylandscreen_p.h>

namespace ryu::wl {
namespace {

class ToplevelRegistry;

class ToplevelHandle : public QtWayland::ext_foreign_toplevel_handle_v1 {
public:
  ToplevelHandle(ToplevelRegistry *owner,
                 struct ::ext_foreign_toplevel_handle_v1 *object)
      : QtWayland::ext_foreign_toplevel_handle_v1(object), m_owner(owner) {}

  ~ToplevelHandle() override {
    if (object()) {
      destroy();
    }
  }

  QString id;

protected:
  void
  ext_foreign_toplevel_handle_v1_identifier(const QString &identifier) override;
  void ext_foreign_toplevel_handle_v1_closed() override;

private:
  ToplevelRegistry *m_owner;
};

class ToplevelRegistry
    : public QWaylandClientExtensionTemplate<ToplevelRegistry>,
      public QtWayland::ext_foreign_toplevel_list_v1 {
public:
  ToplevelRegistry() : QWaylandClientExtensionTemplate(1) { initialize(); }

  static ToplevelRegistry *instance() {
    static ToplevelRegistry *registry = new ToplevelRegistry;
    return registry;
  }

  struct ::ext_foreign_toplevel_handle_v1 *find(const QString &id) const {
    for (ToplevelHandle *handle : m_handles) {
      if (handle->id == id) {
        return handle->object();
      }
    }
    return nullptr;
  }

  void wait(QObject *owner, const QString &id, std::function<void()> fire) {
    m_waiters.append({owner, id, std::move(fire)});
  }

  void identified(const QString &id) {
    QList<Waiter> ready;
    for (int i = 0; i < m_waiters.size();) {
      if (m_waiters.at(i).id == id) {
        ready.append(m_waiters.takeAt(i));
      } else {
        ++i;
      }
    }
    for (const Waiter &waiter : std::as_const(ready)) {
      if (waiter.owner) {
        waiter.fire();
      }
    }
  }

  void closed(ToplevelHandle *handle) {
    m_handles.removeOne(handle);
    if (handle->object()) {
      handle->destroy();
    }
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), [handle]() { delete handle; },
        Qt::QueuedConnection);
  }

protected:
  void ext_foreign_toplevel_list_v1_toplevel(
      struct ::ext_foreign_toplevel_handle_v1 *toplevel) override {
    m_handles.append(new ToplevelHandle(this, toplevel));
  }

  void ext_foreign_toplevel_list_v1_finished() override {}

private:
  struct Waiter {
    QPointer<QObject> owner;
    QString id;
    std::function<void()> fire;
  };

  QList<ToplevelHandle *> m_handles;
  QList<Waiter> m_waiters;
};

void ToplevelHandle::ext_foreign_toplevel_handle_v1_identifier(
    const QString &identifier) {
  id = identifier;
  m_owner->identified(identifier);
}

void ToplevelHandle::ext_foreign_toplevel_handle_v1_closed() {
  m_owner->closed(this);
}

class ToplevelSourceManager
    : public QWaylandClientExtensionTemplate<ToplevelSourceManager>,
      public QtWayland::ext_foreign_toplevel_image_capture_source_manager_v1 {
public:
  ToplevelSourceManager() : QWaylandClientExtensionTemplate(1) { initialize(); }

  static ToplevelSourceManager *instance() {
    static ToplevelSourceManager *manager = new ToplevelSourceManager;
    return manager;
  }
};

class CaptureManager : public QWaylandClientExtensionTemplate<CaptureManager>,
                       public QtWayland::ext_image_copy_capture_manager_v1 {
public:
  CaptureManager() : QWaylandClientExtensionTemplate(1) { initialize(); }

  static CaptureManager *instance() {
    static CaptureManager *manager = new CaptureManager;
    return manager;
  }
};

class ExtFrame : public QtWayland::ext_image_copy_capture_frame_v1 {
public:
  explicit ExtFrame(struct ::ext_image_copy_capture_frame_v1 *object)
      : QtWayland::ext_image_copy_capture_frame_v1(object) {}

  ~ExtFrame() override {
    if (object()) {
      destroy();
    }
  }

  std::function<void()> onReady;
  std::function<void(uint32_t)> onFailed;
  uint32_t transform = 0;

protected:
  void ext_image_copy_capture_frame_v1_transform(uint32_t value) override {
    transform = value;
  }

  void ext_image_copy_capture_frame_v1_ready() override {
    if (onReady) {
      onReady();
    }
  }

  void ext_image_copy_capture_frame_v1_failed(uint32_t reason) override {
    if (onFailed) {
      onFailed(reason);
    }
  }
};

class ExtContext : public ScreencopyContext,
                   public QtWayland::ext_image_copy_capture_session_v1 {
public:
  explicit ExtContext(QString identifier) : m_id(std::move(identifier)) {}

  ~ExtContext() override {
    delete m_frame;
    m_frame = nullptr;
    if (object()) {
      destroy();
    }
  }

  void captureFrame() override {
    if (m_frame) {
      return;
    }
    m_wanted = true;
    if (!object()) {
      open();
      return;
    }
    if (m_have) {
      startFrame();
    }
  }

protected:
  void ext_image_copy_capture_session_v1_buffer_size(uint32_t width,
                                                     uint32_t height) override {
    fresh();
    m_request.width = width;
    m_request.height = height;
  }

  void ext_image_copy_capture_session_v1_shm_format(uint32_t format) override {
    fresh();
    m_request.shm.append(format);
  }

  void
  ext_image_copy_capture_session_v1_dmabuf_device(wl_array *device) override {
    fresh();
    if (device->size == sizeof(dev_t)) {
      std::memcpy(&m_request.device, device->data, sizeof(dev_t));
    }
  }

  void ext_image_copy_capture_session_v1_dmabuf_format(
      uint32_t format, wl_array *modifiers) override {
    fresh();
    BufferRequest::DmaFormat entry(format);
    const auto *mods = static_cast<const uint64_t *>(modifiers->data);
    const size_t count = modifiers->size / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i) {
      entry.pushMod(mods[i]);
    }
    m_request.dma.append(entry);
  }

  void ext_image_copy_capture_session_v1_done() override {
    m_fresh = true;
    m_have = true;
    m_doneSince = true;
    if (m_wanted && !m_frame) {
      startFrame();
    }
  }

  void ext_image_copy_capture_session_v1_stopped() override {
    closeSession();
    emit stopped();
  }

private:
  void fresh() {
    if (!m_fresh) {
      return;
    }
    m_request.reset();
    m_fresh = false;
    m_have = false;
  }

  void open() {
    ToplevelRegistry *registry = ToplevelRegistry::instance();
    ToplevelSourceManager *sources = ToplevelSourceManager::instance();
    CaptureManager *capture = CaptureManager::instance();
    if (!registry->isActive() || !sources->isActive() || !capture->isActive()) {
      qWarning("ryu wayland: ext image capture unavailable");
      emit stopped();
      return;
    }
    struct ::ext_foreign_toplevel_handle_v1 *handle = registry->find(m_id);
    if (!handle) {
      if (!m_waiting) {
        m_waiting = true;
        registry->wait(this, m_id, [this]() {
          m_waiting = false;
          open();
        });
      }
      return;
    }
    QtWayland::ext_image_capture_source_v1 source(
        sources->create_source(handle));
    init(capture->create_session(source.object(), 0));
    source.destroy();
  }

  void closeSession() {
    delete m_frame;
    m_frame = nullptr;
    if (object()) {
      destroy();
    }
    m_have = false;
  }

  void retire(ExtFrame *frame) {
    if (frame->object()) {
      frame->destroy();
    }
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), [frame]() { delete frame; },
        Qt::QueuedConnection);
  }

  void startFrame() {
    m_wanted = false;
    Buffer *back = m_swapchain.createBackbuffer(m_request, allowDma);
    if (!back || !back->buffer()) {
      qWarning("ryu wayland: backbuffer creation failed");
      closeSession();
      emit stopped();
      return;
    }
    m_doneSince = false;
    m_frame = new ExtFrame(create_frame());
    m_frame->onReady = [this]() { frameReady(); };
    m_frame->onFailed = [this](uint32_t reason) { frameFailed(reason); };
    m_frame->attach_buffer(back->buffer());
    m_frame->damage_buffer(0, 0, int32_t(m_request.width),
                           int32_t(m_request.height));
    m_frame->capture();
  }

  void frameReady() {
    ExtFrame *frame = m_frame;
    m_frame = nullptr;
    const uint32_t transform = frame->transform & 7u;
    retire(frame);
    if (Buffer *back = m_swapchain.backbuffer()) {
      back->transform = BufferTransform(uint8_t(transform));
    }
    m_swapchain.swap();
    emit frameCaptured();
  }

  void frameFailed(uint32_t reason) {
    ExtFrame *frame = m_frame;
    m_frame = nullptr;
    retire(frame);
    if (reason == 1) {
      m_wanted = true;
      if (m_doneSince) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
              if (m_wanted && !m_frame && m_have) {
                startFrame();
              }
            },
            Qt::QueuedConnection);
      }
      return;
    }
    qWarning("ryu wayland: toplevel capture failed (%u)", reason);
    emit stopped();
  }

  QString m_id;
  BufferRequest m_request;
  ExtFrame *m_frame = nullptr;
  bool m_fresh = true;
  bool m_have = false;
  bool m_wanted = false;
  bool m_waiting = false;
  bool m_doneSince = false;
};
class WlrManager : public QWaylandClientExtensionTemplate<WlrManager>,
                   public QtWayland::zwlr_screencopy_manager_v1 {
public:
  WlrManager() : QWaylandClientExtensionTemplate(3) { initialize(); }

  static WlrManager *instance() {
    static WlrManager *manager = new WlrManager;
    return manager;
  }
};

class WlrContext : public ScreencopyContext,
                   public QtWayland::zwlr_screencopy_frame_v1 {
public:
  WlrContext(WlrManager *manager, QScreen *screen, wl_output *output)
      : m_manager(manager), m_output(output) {
    connect(screen, &QObject::destroyed, this, [this]() {
      if (object()) {
        destroy();
      }
      emit stopped();
    });
  }

  ~WlrContext() override {
    if (object()) {
      destroy();
    }
  }

  void captureFrame() override {
    if (object()) {
      return;
    }
    m_request.reset();
    m_yInvert = false;
    init(m_manager->capture_output(0, m_output));
  }

protected:
  void zwlr_screencopy_frame_v1_buffer(uint32_t format, uint32_t width,
                                       uint32_t height, uint32_t) override {
    m_request.width = width;
    m_request.height = height;
    m_request.shm.append(format);
  }

  void zwlr_screencopy_frame_v1_linux_dmabuf(uint32_t format, uint32_t width,
                                             uint32_t height) override {
    m_request.width = width;
    m_request.height = height;
    m_request.dma.append(BufferRequest::DmaFormat(format));
  }

  void zwlr_screencopy_frame_v1_flags(uint32_t flags) override {
    m_yInvert = flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT;
  }

  void zwlr_screencopy_frame_v1_buffer_done() override {
    Buffer *back = m_swapchain.createBackbuffer(m_request, allowDma);
    if (!back || !back->buffer()) {
      qWarning("ryu wayland: backbuffer creation failed");
      destroy();
      emit stopped();
      return;
    }
    if (m_copied) {
      copy_with_damage(back->buffer());
    } else {
      copy(back->buffer());
    }
  }

  void zwlr_screencopy_frame_v1_ready(uint32_t, uint32_t, uint32_t) override {
    destroy();
    m_copied = true;
    if (Buffer *back = m_swapchain.backbuffer()) {
      back->transform =
          m_yInvert ? BufferTransform::Flipped180 : BufferTransform::Normal0;
    }
    m_swapchain.swap();
    emit frameCaptured();
  }

  void zwlr_screencopy_frame_v1_failed() override {
    qWarning("ryu wayland: output capture failed");
    destroy();
    emit stopped();
  }

private:
  WlrManager *m_manager;
  wl_output *m_output;
  BufferRequest m_request;
  bool m_copied = false;
  bool m_yInvert = false;
};

QScreen *screenNamed(const QString &name) {
  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    if (screen->name() == name) {
      return screen;
    }
  }
  return nullptr;
}

} // namespace

ScreencopyContext *makeContext(QObject *source, bool hardware) {
  if (!source) {
    return nullptr;
  }
  const QMetaObject *meta = source->metaObject();
  ScreencopyContext *context = nullptr;
  if (meta->indexOfProperty("foreignId") >= 0) {
    const QString id = source->property("foreignId").toString();
    if (id.isEmpty()) {
      return nullptr;
    }
    context = new ExtContext(id);
  } else if (meta->indexOfProperty("name") >= 0) {
    QScreen *screen = screenNamed(source->property("name").toString());
    if (!screen) {
      return nullptr;
    }
    auto *waylandScreen =
        dynamic_cast<QtWaylandClient::QWaylandScreen *>(screen->handle());
    WlrManager *manager = WlrManager::instance();
    if (!waylandScreen || !manager->isActive()) {
      return nullptr;
    }
    context = new WlrContext(manager, screen, waylandScreen->output());
  }
  if (context) {
    context->allowDma = hardware;
  }
  return context;
}
} // namespace ryu::wl
