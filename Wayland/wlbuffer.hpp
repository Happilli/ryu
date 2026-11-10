#pragma once

#include <QList>
#include <QMatrix4x4>
#include <QObject>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGNode>
#include <QSGTexture>
#include <QSize>
#include <cstdint>
#include <memory>
#include <sys/types.h>

struct wl_buffer;

namespace ryu::wl {

constexpr uint64_t kModInvalid = 0x00ffffffffffffffULL;

struct BufferTransform {
  enum Transform : uint8_t {
    Normal0 = 0,
    Normal90 = 1,
    Normal180 = 2,
    Normal270 = 3,
    Flipped0 = 4,
    Flipped90 = 5,
    Flipped180 = 6,
    Flipped270 = 7,
  } transform = Normal0;

  BufferTransform() = default;
  BufferTransform(uint8_t value) : transform(static_cast<Transform>(value)) {}

  int degrees() const { return 90 * (transform & 0b11111011); }
  bool flip() const { return transform & 0b00000100; }
  bool flipSize() const { return transform & 0b00000001; }

  void apply(QMatrix4x4 &matrix) const {
    matrix.rotate(flip() ? 180 : 0, 0, 1, 0);
    matrix.rotate(float(degrees()), 0, 0, 1);
  }
};

struct BufferRequest {
  struct DmaFormat {
    DmaFormat() = default;
    explicit DmaFormat(uint32_t value) : format(value) {}
    void pushMod(uint64_t mod) {
      if (mod == kModInvalid) {
        implicit = true;
      } else {
        modifiers.append(mod);
      }
    }
    uint32_t format = 0;
    QList<uint64_t> modifiers;
    bool implicit = false;
  };

  uint32_t width = 0;
  uint32_t height = 0;
  QList<uint32_t> shm;
  dev_t device = 0;
  QList<DmaFormat> dma;

  void reset() { *this = BufferRequest(); }
};

class BufferTexture {
public:
  BufferTexture() = default;
  virtual ~BufferTexture() = default;
  BufferTexture(const BufferTexture &) = delete;
  BufferTexture &operator=(const BufferTexture &) = delete;

  virtual QSGTexture *texture() const = 0;
  virtual void sync(const class Buffer *, QQuickWindow *) {}
};

class Buffer {
public:
  virtual ~Buffer() = default;
  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;

  virtual wl_buffer *buffer() const = 0;
  virtual QSize size() const = 0;
  virtual bool compatible(const BufferRequest &request) const = 0;
  virtual bool isDma() const = 0;
  virtual BufferTexture *createTexture(QQuickWindow *window) const = 0;

  BufferTransform transform;
  const quint64 serial;

protected:
  Buffer();
};

class Swapchain {
public:
  Buffer *createBackbuffer(const BufferRequest &request, bool allowDma,
                           bool *fresh = nullptr);
  void swap() { m_second = !m_second; }
  Buffer *backbuffer() const { return m_second ? m_a.get() : m_b.get(); }
  Buffer *frontbuffer() const { return m_second ? m_b.get() : m_a.get(); }

private:
  std::unique_ptr<Buffer> m_a;
  std::unique_ptr<Buffer> m_b;
  bool m_second = false;

  friend class DisplayNode;
};

class DisplayNode : public QSGTransformNode {
public:
  explicit DisplayNode(QQuickWindow *window);
  bool sync(const Swapchain &swapchain);
  void setRect(const QRectF &rect);
  void setFiltering(QSGTexture::Filtering filtering);

private:
  struct Slot {
    quint64 serial = 0;
    Buffer *buffer = nullptr;
    std::unique_ptr<BufferTexture> texture;
  };

  QQuickWindow *m_window;
  QSGImageNode *m_image;
  Slot m_a;
  Slot m_b;
  bool m_second = false;
  bool m_valid = false;
};

class BufferManager : public QObject {
  Q_OBJECT

public:
  ~BufferManager() override;
  static BufferManager *instance();

  void initWindow(QQuickWindow *window);
  bool isReady() const;
  Buffer *createBuffer(const BufferRequest &request, bool allowDma);

signals:
  void ready();

private:
  BufferManager();
  void check();

  struct Private;
  Private *p;
};

} // namespace ryu::wl
