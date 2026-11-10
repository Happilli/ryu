#define EGL_NO_X11
#define MESA_EGL_NO_X11_HEADERS
#include "./wlbuffer.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPair>
#include <QSGRendererInterface>
#include <QtWaylandClient/qwaylandclientextension.h>
#include <atomic>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <functional>
#include <gbm.h>
#include <QtGui/qopenglcontext_platform.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>
#include <private/qwaylandshm_p.h>
#include <private/qwaylandshmbackingstore_p.h>
#include <qsgtexture_platform.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <vector>
#include <wayland-client-protocol.h>
#include <xf86drm.h>

#include "qwayland-linux-dmabuf-v1.h"
#include "wayland-linux-dmabuf-v1-client-protocol.h"

namespace ryu::wl {

namespace {

std::atomic<quint64> g_serial{0};

struct Modifiers {
  QList<uint64_t> list;
  bool implicit = false;
};

using FormatList = QList<QPair<uint32_t, Modifiers>>;

struct Tranche {
  dev_t device = 0;
  uint32_t flags = 0;
  FormatList formats;
};

struct GbmDevice {
  dev_t handle = 0;
  std::string node;
  gbm_device *device = nullptr;
  GbmDevice() = default;
  GbmDevice(const GbmDevice &) = delete;
  GbmDevice &operator=(const GbmDevice &) = delete;
  ~GbmDevice() {
    if (device) {
      const int fd = gbm_device_get_fd(device);
      gbm_device_destroy(device);
      close(fd);
    }
  }
};

bool formatHasAlpha(uint32_t format) {
  switch (format) {
  case DRM_FORMAT_ARGB8888:
  case DRM_FORMAT_ABGR8888:
  case DRM_FORMAT_ARGB2101010:
  case DRM_FORMAT_ABGR2101010:
  case DRM_FORMAT_ABGR16161616F:
    return true;
  default:
    return false;
  }
}

void sortFormats(FormatList &formats) {
  int next = 0;
  for (const uint32_t wanted :
       {uint32_t(DRM_FORMAT_ARGB8888), uint32_t(DRM_FORMAT_XRGB8888)}) {
    for (int i = next; i < formats.size(); ++i) {
      if (formats.at(i).first == wanted) {
        formats.swapItemsAt(i, next);
        ++next;
        break;
      }
    }
  }
}

template <typename T> T eglProc(const char *name) {
  auto proc = reinterpret_cast<T>(eglGetProcAddress(name));
  if (!proc) {
    qWarning("ryu wayland: missing EGL proc %s", name);
  }
  return proc;
}

class DmaTexture : public BufferTexture {
public:
  DmaTexture(EGLDisplay display, EGLImage image, unsigned int texture,
             QSGTexture *qsg)
      : m_display(display), m_image(image), m_texture(texture), m_qsg(qsg) {}

  ~DmaTexture() override {
    delete m_qsg;
    if (QOpenGLContext *ctx = QOpenGLContext::currentContext()) {
      if (m_texture) {
        ctx->functions()->glDeleteTextures(1, &m_texture);
      }
    }
    if (m_image) {
      eglDestroyImage(m_display, m_image);
    }
  }

  QSGTexture *texture() const override { return m_qsg; }

private:
  EGLDisplay m_display;
  EGLImage m_image;
  unsigned int m_texture;
  QSGTexture *m_qsg;
};

class DmaBuffer : public Buffer {
public:
  struct Plane {
    int fd = -1;
    uint32_t offset = 0;
    uint32_t stride = 0;
  };

  DmaBuffer() = default;

  ~DmaBuffer() override {
    if (wlBuffer) {
      wl_buffer_destroy(wlBuffer);
    }
    if (bo) {
      gbm_bo_destroy(bo);
    }
    for (const Plane &plane : planes) {
      if (plane.fd >= 0) {
        close(plane.fd);
      }
    }
  }

  wl_buffer *buffer() const override { return wlBuffer; }
  QSize size() const override { return QSize(int(width), int(height)); }
  bool isDma() const override { return true; }

  bool compatible(const BufferRequest &request) const override {
    if (request.width != width || request.height != height) {
      return false;
    }
    for (const auto &entry : request.dma) {
      if (entry.format != format) {
        continue;
      }
      if (entry.modifiers.isEmpty() ||
          (entry.implicit && usedImplicitModifier) ||
          entry.modifiers.contains(modifier)) {
        return true;
      }
    }
    return false;
  }

  BufferTexture *createTexture(QQuickWindow *window) const override {
    static auto target = reinterpret_cast<void (*)(unsigned int, void *)>(
        eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!target) {
      qWarning("ryu wayland: glEGLImageTargetTexture2DOES missing");
      return nullptr;
    }

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
      qWarning("ryu wayland: no current GL context for dmabuf import");
      return nullptr;
    }
    auto *egl = context->nativeInterface<QNativeInterface::QEGLContext>();
    if (!egl) {
      qWarning("ryu wayland: GL context is not EGL");
      return nullptr;
    }
    EGLDisplay display = egl->display();

    static const EGLAttrib fdNames[4] = {
        EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT};
    static const EGLAttrib offsetNames[4] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT};
    static const EGLAttrib pitchNames[4] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT};
    static const EGLAttrib modLoNames[4] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT};
    static const EGLAttrib modHiNames[4] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};

    if (planes.size() > 4) {
      qWarning("ryu wayland: dmabuf has more than 4 planes");
      return nullptr;
    }

    std::vector<EGLAttrib> attribs = {
        EGL_WIDTH,         EGLAttrib(width),         EGL_HEIGHT,
        EGLAttrib(height), EGL_LINUX_DRM_FOURCC_EXT, EGLAttrib(format),
    };

    for (int i = 0; i < planes.size(); ++i) {
      const Plane &plane = planes.at(i);
      attribs.insert(attribs.end(), {fdNames[i], EGLAttrib(plane.fd),
                                     offsetNames[i], EGLAttrib(plane.offset),
                                     pitchNames[i], EGLAttrib(plane.stride)});
      if (modifier != kModInvalid) {
        attribs.insert(attribs.end(),
                       {modLoNames[i], EGLAttrib(modifier & 0xffffffffu),
                        modHiNames[i], EGLAttrib(modifier >> 32)});
      }
    }
    attribs.push_back(EGL_NONE);

    EGLImage image =
        eglCreateImage(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                       attribs.data());
    if (image == EGL_NO_IMAGE) {
      qWarning("ryu wayland: eglCreateImage failed (0x%x)", eglGetError());
      return nullptr;
    }

    QOpenGLFunctions *f = context->functions();
    unsigned int texture = 0;
    window->beginExternalCommands();
    f->glGenTextures(1, &texture);
    f->glBindTexture(GL_TEXTURE_2D, texture);
    target(GL_TEXTURE_2D, image);
    f->glBindTexture(GL_TEXTURE_2D, 0);
    window->endExternalCommands();

    QSGTexture *qsg = QNativeInterface::QSGOpenGLTexture::fromNative(
        texture, window, QSize(int(width), int(height)),
        formatHasAlpha(format) ? QQuickWindow::TextureHasAlphaChannel
                               : QQuickWindow::TextureIsOpaque);
    return new DmaTexture(display, image, texture, qsg);
  }

  std::shared_ptr<GbmDevice> device;
  gbm_bo *bo = nullptr;
  wl_buffer *wlBuffer = nullptr;
  QList<Plane> planes;
  uint32_t format = 0;
  uint64_t modifier = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  bool usedImplicitModifier = false;
};

class ShmTexture : public BufferTexture {
public:
  QSGTexture *texture() const override { return m_texture.get(); }

  void sync(const Buffer *, QQuickWindow *window) override {
    m_texture.reset(window->createTextureFromImage(*m_shm->image()));
  }

  std::shared_ptr<QtWaylandClient::QWaylandShmBuffer> m_shm;
  std::unique_ptr<QSGTexture> m_texture;
};

class ShmBuffer : public Buffer {
public:
  ShmBuffer(QtWaylandClient::QWaylandShmBuffer *shm, uint32_t fmt)
      : m_shm(shm), m_format(fmt) {}

  wl_buffer *buffer() const override { return m_shm->buffer(); }
  QSize size() const override { return m_shm->size(); }
  bool isDma() const override { return false; }

  bool compatible(const BufferRequest &request) const override {
    if (QSize(int(request.width), int(request.height)) != size()) {
      return false;
    }
    return request.shm.contains(m_format);
  }

  BufferTexture *createTexture(QQuickWindow *window) const override {
    auto *texture = new ShmTexture;
    texture->m_shm = m_shm;
    texture->sync(this, window);
    return texture;
  }

private:
  std::shared_ptr<QtWaylandClient::QWaylandShmBuffer> m_shm;
  uint32_t m_format;
};

class Feedback : public QtWayland::zwp_linux_dmabuf_feedback_v1 {
public:
  Feedback(::zwp_linux_dmabuf_feedback_v1 *object,
           std::function<void(QList<Tranche>)> done)
      : QtWayland::zwp_linux_dmabuf_feedback_v1(object),
        m_done(std::move(done)) {}

  ~Feedback() override {
    if (m_table) {
      munmap(m_table, m_tableSize);
    }
    if (object()) {
      destroy();
    }
  }

protected:
  void zwp_linux_dmabuf_feedback_v1_main_device(wl_array *) override {}

  void zwp_linux_dmabuf_feedback_v1_format_table(int32_t fd,
                                                 uint32_t size) override {
    if (m_table) {
      munmap(m_table, m_tableSize);
      m_table = nullptr;
    }
    m_tableSize = size;
    void *map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    m_table = map == MAP_FAILED ? nullptr : map;
    close(fd);
  }

  void zwp_linux_dmabuf_feedback_v1_tranche_target_device(
      wl_array *device) override {
    Tranche tranche;
    if (device->size == sizeof(dev_t)) {
      tranche.device = *static_cast<dev_t *>(device->data);
    }
    m_tranches.append(tranche);
  }

  void zwp_linux_dmabuf_feedback_v1_tranche_flags(uint32_t flags) override {
    if (!m_tranches.isEmpty()) {
      m_tranches.last().flags = flags;
    }
  }

  void
  zwp_linux_dmabuf_feedback_v1_tranche_formats(wl_array *indices) override {
    struct Entry {
      uint32_t format;
      uint32_t padding;
      uint64_t modifier;
    };
    if (!m_table || m_tranches.isEmpty()) {
      return;
    }

    Tranche &tranche = m_tranches.last();
    const auto *table = static_cast<const Entry *>(m_table);
    const size_t tableCount = m_tableSize / sizeof(Entry);
    const auto *index = static_cast<const uint16_t *>(indices->data);
    const size_t count = indices->size / sizeof(uint16_t);

    for (size_t i = 0; i < count; ++i) {
      if (index[i] >= tableCount) {
        continue;
      }
      const Entry &entry = table[index[i]];
      Modifiers *mods = nullptr;
      for (auto &pair : tranche.formats) {
        if (pair.first == entry.format) {
          mods = &pair.second;
          break;
        }
      }
      if (!mods) {
        tranche.formats.append(qMakePair(entry.format, Modifiers()));
        mods = &tranche.formats.last().second;
      }
      if (entry.modifier == kModInvalid) {
        mods->implicit = true;
      } else {
        mods->list.append(entry.modifier);
      }
    }
  }

  void zwp_linux_dmabuf_feedback_v1_tranche_done() override {}

  void zwp_linux_dmabuf_feedback_v1_done() override {
    if (m_table) {
      munmap(m_table, m_tableSize);
      m_table = nullptr;
    }
    QList<Tranche> tranches;
    tranches.swap(m_tranches);
    m_done(std::move(tranches));
  }

private:
  std::function<void(QList<Tranche>)> m_done;
  QList<Tranche> m_tranches;
  void *m_table = nullptr;
  uint32_t m_tableSize = 0;
};

class DmabufManager : public QWaylandClientExtensionTemplate<DmabufManager>,
                      public QtWayland::zwp_linux_dmabuf_v1 {
public:
  DmabufManager() : QWaylandClientExtensionTemplate(5) {
    initialize();
    if (isActive() && QtWayland::zwp_linux_dmabuf_v1::version() >= 4) {
      m_feedback = std::make_unique<Feedback>(
          get_default_feedback(), [this](QList<Tranche> tranches) {
            m_tranches = std::move(tranches);
            if (m_onFeedback) {
              m_onFeedback();
            }
          });
      m_usable = true;
    }
  }

  bool usable() const { return m_usable; }
  void setFeedbackCallback(std::function<void()> fn) {
    m_onFeedback = std::move(fn);
  }

  bool initRenderFormats(QQuickWindow *window) {
    QSGRendererInterface *ri = window->rendererInterface();
    if (ri->graphicsApi() != QSGRendererInterface::OpenGL) {
      qWarning("ryu wayland: scene graph is not OpenGL, using shm");
      return false;
    }

    auto *context = static_cast<QOpenGLContext *>(
        ri->getResource(window, QSGRendererInterface::OpenGLContextResource));
    if (!context) {
      qWarning("ryu wayland: no GL context for render formats");
      return false;
    }
    auto *egl = context->nativeInterface<QNativeInterface::QEGLContext>();
    if (!egl) {
      qWarning("ryu wayland: GL context is not EGL");
      return false;
    }
    EGLDisplay display = egl->display();

    static auto queryDisplayAttrib =
        eglProc<PFNEGLQUERYDISPLAYATTRIBEXTPROC>("eglQueryDisplayAttribEXT");
    static auto queryDeviceString =
        eglProc<PFNEGLQUERYDEVICESTRINGEXTPROC>("eglQueryDeviceStringEXT");
    static auto queryFormats =
        eglProc<PFNEGLQUERYDMABUFFORMATSEXTPROC>("eglQueryDmaBufFormatsEXT");
    static auto queryModifiers = eglProc<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(
        "eglQueryDmaBufModifiersEXT");

    if (!queryDisplayAttrib || !queryDeviceString || !queryFormats ||
        !queryModifiers) {
      return false;
    }

    EGLAttrib deviceAttrib = 0;
    if (!queryDisplayAttrib(display, EGL_DEVICE_EXT, &deviceAttrib)) {
      qWarning("ryu wayland: EGL device attrib missing");
      return false;
    }
    auto device = reinterpret_cast<EGLDeviceEXT>(deviceAttrib);

    if (const char *node =
            queryDeviceString(device, EGL_DRM_RENDER_NODE_FILE_EXT)) {
      m_renderNode = node;
    } else if (const char *primary =
                   queryDeviceString(device, EGL_DRM_DEVICE_FILE_EXT)) {
      m_renderNode = primary;
    } else {
      qWarning("ryu wayland: no DRM node for the EGL device");
      return false;
    }

    EGLint count = 0;
    if (!queryFormats(display, 0, nullptr, &count) || count == 0) {
      qWarning("ryu wayland: EGL reports no dmabuf formats");
      return false;
    }
    std::vector<EGLint> formats(count);
    if (!queryFormats(display, count, formats.data(), &count)) {
      return false;
    }

    FormatList render;
    for (const EGLint format : formats) {
      EGLint modCount = 0;
      if (!queryModifiers(display, format, 0, nullptr, nullptr, &modCount)) {
        return false;
      }
      std::vector<EGLuint64KHR> mods(modCount);
      std::vector<EGLBoolean> external(modCount);
      if (!queryModifiers(display, format, modCount, mods.data(),
                          external.data(), &modCount)) {
        return false;
      }
      Modifiers entry;
      for (EGLint i = 0; i < modCount; ++i) {
        if (external[i] != EGL_TRUE) {
          entry.list.append(mods[i]);
        }
      }
      entry.implicit = true;
      render.append(qMakePair(uint32_t(format), entry));
    }
    m_render = std::move(render);
    return true;
  }

  Buffer *create(const BufferRequest &request) {
    for (const Tranche &tranche : std::as_const(m_tranches)) {
      if (request.device != 0 && tranche.device != request.device) {
        continue;
      }

      FormatList usable;
      for (const auto &wanted : request.dma) {
        for (const auto &render : std::as_const(m_render)) {
          if (render.first != wanted.format) {
            continue;
          }
          const Modifiers &renderMods = render.second;

          if (!wanted.modifiers.isEmpty()) {
            Modifiers mods;
            mods.implicit = wanted.implicit && renderMods.implicit;
            for (const uint64_t mod : wanted.modifiers) {
              if (renderMods.list.contains(mod)) {
                mods.list.append(mod);
              }
            }
            if (mods.implicit || !mods.list.isEmpty()) {
              usable.append(qMakePair(wanted.format, mods));
            }
          } else {
            for (const auto &trancheFormat : tranche.formats) {
              if (trancheFormat.first != wanted.format) {
                continue;
              }
              Modifiers mods;
              mods.implicit =
                  trancheFormat.second.implicit && renderMods.implicit;
              for (const uint64_t mod : trancheFormat.second.list) {
                if (renderMods.list.contains(mod)) {
                  mods.list.append(mod);
                }
              }
              if (mods.implicit || !mods.list.isEmpty()) {
                usable.append(qMakePair(wanted.format, mods));
              }
              break;
            }
          }
          break;
        }
      }

      if (usable.isEmpty()) {
        continue;
      }
      sortFormats(usable);

      std::shared_ptr<GbmDevice> gbm = gbmFor(tranche.device);
      if (!gbm) {
        continue;
      }

      for (const auto &entry : std::as_const(usable)) {
        if (Buffer *buffer = make(gbm, entry.first, entry.second, request.width,
                                  request.height)) {
          return buffer;
        }
      }
    }
    return nullptr;
  }

private:
  std::shared_ptr<GbmDevice> gbmFor(dev_t handle) {
    m_gbm.removeIf(
        [](const std::weak_ptr<GbmDevice> &weak) { return weak.expired(); });

    for (const auto &weak : std::as_const(m_gbm)) {
      if (auto shared = weak.lock(); shared && shared->handle == handle) {
        return shared;
      }
    }

    drmDevice *drm = nullptr;
    if (drmGetDeviceFromDevId(handle, 0, &drm) != 0) {
      return nullptr;
    }

    std::string node;
    if (drm->available_nodes & (1 << DRM_NODE_RENDER)) {
      node = drm->nodes[DRM_NODE_RENDER];
    } else if (drm->available_nodes & (1 << DRM_NODE_PRIMARY)) {
      node = drm->nodes[DRM_NODE_PRIMARY];
    }
    drmFreeDevice(&drm);
    if (node.empty()) {
      return nullptr;
    }

    for (const auto &weak : std::as_const(m_gbm)) {
      if (auto shared = weak.lock(); shared && shared->node == node) {
        return shared;
      }
    }

    const int fd = open(node.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      return nullptr;
    }
    gbm_device *device = gbm_create_device(fd);
    if (!device) {
      close(fd);
      return nullptr;
    }

    auto shared = std::make_shared<GbmDevice>();
    shared->handle = handle;
    shared->node = node;
    shared->device = device;
    m_gbm.append(shared);
    return shared;
  }

  Buffer *make(const std::shared_ptr<GbmDevice> &gbm, uint32_t format,
               const Modifiers &mods, uint32_t width, uint32_t height) {
    auto buffer = std::make_unique<DmaBuffer>();

    if (mods.list.isEmpty()) {
      if (!mods.implicit || gbm->node != m_renderNode) {
        return nullptr;
      }
      buffer->usedImplicitModifier = true;
      buffer->bo = gbm_bo_create(gbm->device, width, height, format,
                                 GBM_BO_USE_RENDERING);
    } else {
      buffer->bo = gbm_bo_create_with_modifiers2(
          gbm->device, width, height, format, mods.list.constData(),
          unsigned(mods.list.size()), GBM_BO_USE_RENDERING);
    }

    if (!buffer->bo) {
      return nullptr;
    }

    buffer->device = gbm;
    buffer->modifier = gbm_bo_get_modifier(buffer->bo);
    const int planeCount = gbm_bo_get_plane_count(buffer->bo);
    buffer->planes.resize(planeCount);

    QtWayland::zwp_linux_buffer_params_v1 params(create_params());

    for (int i = 0; i < planeCount; ++i) {
      DmaBuffer::Plane &plane = buffer->planes[i];
      plane.fd = gbm_bo_get_fd_for_plane(buffer->bo, i);
      if (plane.fd < 0) {
        params.destroy();
        return nullptr;
      }
      plane.stride = gbm_bo_get_stride_for_plane(buffer->bo, i);
      plane.offset = gbm_bo_get_offset(buffer->bo, i);
      params.add(plane.fd, i, plane.offset, plane.stride,
                 uint32_t(buffer->modifier >> 32),
                 uint32_t(buffer->modifier & 0xffffffffu));
    }

    buffer->wlBuffer =
        params.create_immed(int32_t(width), int32_t(height), format, 0);
    params.destroy();

    if (!buffer->wlBuffer) {
      return nullptr;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->format = format;
    return buffer.release();
  }

  std::unique_ptr<Feedback> m_feedback;
  std::function<void()> m_onFeedback;
  QList<Tranche> m_tranches;
  FormatList m_render;
  std::string m_renderNode;
  QList<std::weak_ptr<GbmDevice>> m_gbm;
  bool m_usable = false;
};

Buffer *makeShm(const BufferRequest &request) {
  if (request.shm.isEmpty()) {
    return nullptr;
  }
  auto *integration = QtWaylandClient::QWaylandIntegration::instance();
  if (!integration) {
    return nullptr;
  }
  const uint32_t format = request.shm.first();
  auto *shm = new QtWaylandClient::QWaylandShmBuffer(
      integration->display(), QSize(int(request.width), int(request.height)),
      QtWaylandClient::QWaylandShm::formatFrom(
          static_cast<::wl_shm_format>(format)));
  return new ShmBuffer(shm, format);
}

} // namespace

Buffer::Buffer() : serial(++g_serial) {}

Buffer *Swapchain::createBackbuffer(const BufferRequest &request, bool allowDma,
                                    bool *fresh) {
  auto &slot = m_second ? m_a : m_b;
  if (!slot || !slot->compatible(request) || (!allowDma && slot->isDma())) {
    slot.reset(BufferManager::instance()->createBuffer(request, allowDma));
    if (fresh) {
      *fresh = true;
    }
  }
  return slot.get();
}

DisplayNode::DisplayNode(QQuickWindow *window)
    : m_window(window), m_image(window->createImageNode()) {
  appendChildNode(m_image);
}

bool DisplayNode::sync(const Swapchain &swapchain) {
  Buffer *front = swapchain.frontbuffer();
  if (!front) {
    m_valid = false;
    return false;
  }

  Slot &slot = swapchain.m_second ? m_b : m_a;

  if (m_valid && swapchain.m_second == m_second &&
      slot.serial == front->serial && slot.texture) {
    return true;
  }

  if (slot.texture && slot.serial == front->serial) {
    slot.texture->sync(front, m_window);
  } else {
    slot.texture.reset(front->createTexture(m_window));
    slot.serial = slot.texture ? front->serial : 0;
    if (!slot.texture) {
      m_valid = false;
      return false;
    }
  }

  slot.buffer = front;
  m_second = swapchain.m_second;
  m_valid = true;
  m_image->setTexture(slot.texture->texture());
  return true;
}

void DisplayNode::setRect(const QRectF &rect) {
  const Buffer *buffer = (m_second ? m_b : m_a).buffer;
  if (!buffer) {
    return;
  }

  QMatrix4x4 matrix;
  const QPointF center = rect.center();
  const float cx = float(center.x());
  const float cy = float(center.y());
  matrix.translate(cx, cy);
  buffer->transform.apply(matrix);
  matrix.translate(-cx, -cy);

  const QRectF view = matrix.mapRect(rect);
  QSizeF size = QSizeF(buffer->size());
  size.scale(view.width(), view.height(), Qt::KeepAspectRatio);

  m_image->setRect(view.x() + view.width() / 2 - size.width() / 2,
                   view.y() + view.height() / 2 - size.height() / 2,
                   size.width(), size.height());
  setMatrix(matrix);
}

void DisplayNode::setFiltering(QSGTexture::Filtering filtering) {
  m_image->setFiltering(filtering);
}

struct BufferManager::Private {
  DmabufManager dmabuf;
  std::atomic<bool> dmaReady{false};
  std::atomic<bool> renderReady{false};
  std::atomic<bool> renderFailed{false};
  std::atomic<bool> emitted{false};
  std::atomic<bool> initStarted{false};
};

BufferManager::BufferManager() : p(new Private) {
  if (!p->dmabuf.usable()) {
    p->renderFailed = true;
    p->dmaReady = true;
    return;
  }
  p->dmabuf.setFeedbackCallback([this]() {
    p->dmaReady = true;
    check();
  });
}

BufferManager::~BufferManager() { delete p; }

BufferManager *BufferManager::instance() {
  static BufferManager *manager = new BufferManager;
  return manager;
}

void BufferManager::check() {
  if (p->dmaReady && p->renderReady && !p->emitted.exchange(true)) {
    emit ready();
  }
}

bool BufferManager::isReady() const { return p->dmaReady && p->renderReady; }

void BufferManager::initWindow(QQuickWindow *window) {
  if (!window || p->renderReady) {
    return;
  }
  if (p->initStarted.exchange(true)) {
    return;
  }

  auto run = [this](QQuickWindow *target) {
    if (p->renderFailed || !p->dmabuf.initRenderFormats(target)) {
      qWarning("ryu wayland: dmabuf unavailable, using shm");
      p->renderFailed = true;
    }
    p->renderReady = true;
    check();
  };

  QObject::connect(window, &QObject::destroyed, this, [this]() {
    if (!p->renderReady) {
      p->initStarted = false;
    }
  });

  if (window->isSceneGraphInitialized()) {
    run(window);
    return;
  }

  QObject::connect(
      window, &QQuickWindow::sceneGraphInitialized, this,
      [run, window]() { run(window); },
      Qt::ConnectionType(Qt::DirectConnection | Qt::SingleShotConnection));
}

Buffer *BufferManager::createBuffer(const BufferRequest &request,
                                    bool allowDma) {
  static const bool disabled = qEnvironmentVariableIsSet("RYU_DISABLE_DMABUF");

  if (request.width == 0 || request.height == 0) {
    return nullptr;
  }

  if (allowDma && !disabled && !p->renderFailed) {
    if (Buffer *buffer = p->dmabuf.create(request)) {
      return buffer;
    }
    qWarning("ryu wayland: dmabuf allocation failed, falling back to shm");
  }

  return makeShm(request);
}

} // namespace ryu::wl
