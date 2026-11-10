#include "./screencopy.hpp"

#include <QQuickWindow>
#include <QSGNode>
#include <QTimer>
#include <algorithm>

#include "./context.hpp"
#include "./wlbuffer.hpp"

using namespace ryu::wl;

ScreencopyView::ScreencopyView(QQuickItem *parent) : QQuickItem(parent) {}

ScreencopyView::~ScreencopyView() { delete m_context; }

bool ScreencopyView::wanted() const { return isVisible() && window(); }

void ScreencopyView::componentComplete() {
  QQuickItem::componentComplete();
  if (m_ready) {
    createContext();
    return;
  }
  ensureBuffers();
}

void ScreencopyView::ensureBuffers() {
  if (m_ready) {
    return;
  }
  BufferManager *manager = BufferManager::instance();
  if (manager->isReady()) {
    onBuffersReady();
    return;
  }
  connect(manager, &BufferManager::ready, this, &ScreencopyView::onBuffersReady,
          Qt::UniqueConnection);
  if (window()) {
    manager->initWindow(window());
  }
}

void ScreencopyView::onBuffersReady() {
  m_ready = true;
  createContext();
}

void ScreencopyView::setCaptureSource(QObject *source) {
  if (m_source == source) {
    return;
  }

  destroyContext(false);

  if (m_source) {
    disconnect(m_source, nullptr, this, nullptr);
  }

  m_source = source;

  if (source) {
    connect(source, &QObject::destroyed, this, [this]() {
      destroyContext(true);
      emit captureSourceChanged();
    });
  }

  emit captureSourceChanged();
  update();
  createContext();
}

void ScreencopyView::destroyContext(bool update) {
  const bool had = m_context != nullptr;
  delete m_context;
  m_context = nullptr;

  if (m_hasContent) {
    m_hasContent = false;
    emit hasContentChanged();
  }
  if (m_accelerated) {
    m_accelerated = false;
    emit acceleratedChanged();
  }
  if (m_sourceSize.isValid()) {
    m_sourceSize = QSize();
    emit sourceSizeChanged();
  }

  if (had && update) {
    this->update();
  }
}

void ScreencopyView::scheduleNext() {
  if (m_nextQueued.exchange(true)) {
    return;
  }
  QMetaObject::invokeMethod(
      this,
      [this]() {
        m_nextQueued.store(false);
        advance();
      },
      Qt::QueuedConnection);
}

void ScreencopyView::advance() {
  if (!m_live || !m_context || !wanted()) {
    return;
  }

  const qint64 interval = 1000 / std::max(1, m_fps);
  const qint64 elapsed = m_clock.isValid() ? m_clock.elapsed() : interval;

  if (elapsed >= interval) {
    m_clock.restart();
    m_context->captureFrame();
    return;
  }

  if (m_timerPending) {
    return;
  }

  m_timerPending = true;
  QTimer::singleShot(int(interval - elapsed), this, [this]() {
    m_timerPending = false;
    if (m_live && m_context && wanted()) {
      m_clock.restart();
      m_context->captureFrame();
    }
  });
}

void ScreencopyView::setLive(bool live) {
  if (m_live == live) {
    return;
  }
  m_live = live;
  emit liveChanged();
  if (m_live) {
    advance();
  }
}

void ScreencopyView::setFullResolution(bool full) {
  if (m_full == full) {
    return;
  }
  m_full = full;
  emit fullResolutionChanged();
}

void ScreencopyView::setMaxFps(int fps) {
  fps = std::clamp(fps, 1, 60);
  if (m_fps == fps) {
    return;
  }
  m_fps = fps;
  emit maxFpsChanged();
}

void ScreencopyView::setHardware(bool hardware) {
  if (m_hardware == hardware) {
    return;
  }
  m_hardware = hardware;
  if (m_context) {
    m_context->allowDma = hardware;
  }
  emit hardwareChanged();
}

void ScreencopyView::createContext() {
  destroyContext(false);
  if (!m_source || !m_ready || !isComponentComplete()) {
    return;
  }
  m_context = makeContext(m_source, m_hardware);
  if (!m_context) {
    return;
  }
  m_context->setParent(this);
  connect(
      m_context, &ScreencopyContext::stopped, this,
      [this]() { destroyContext(true); }, Qt::QueuedConnection);
  connect(m_context, &ScreencopyContext::frameCaptured, this,
          &ScreencopyView::onFrameCaptured);
  if (wanted()) {
    m_clock.restart();
    m_context->captureFrame();
  }
}

void ScreencopyView::onFrameCaptured() {
  if (!m_context) {
    return;
  }
  const Buffer *front = m_context->swapchain().frontbuffer();
  if (!front) {
    return;
  }
  setFlag(QQuickItem::ItemHasContents, true);
  QSize size = front->size();
  if (front->transform.flipSize()) {
    size.transpose();
  }
  if (m_sourceSize != size) {
    m_sourceSize = size;
    setImplicitSize(size.width(), size.height());
    emit sourceSizeChanged();
  }
  const bool accelerated = front->isDma();
  if (m_accelerated != accelerated) {
    m_accelerated = accelerated;
    emit acceleratedChanged();
  }
  if (!m_hasContent) {
    m_hasContent = true;
    emit hasContentChanged();
  }
  update();
}

QSGNode *ScreencopyView::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
  if (!m_context || !m_hasContent || !window()) {
    delete old;
    return nullptr;
  }
  auto *node = static_cast<DisplayNode *>(old);
  if (!node) {
    node = new DisplayNode(window());
  }
  if (!node->sync(m_context->swapchain())) {
    qWarning("ryu wayland: node sync failed");
    delete node;
    return nullptr;
  }
  node->setRect(boundingRect());
  node->setFiltering(QSGTexture::Linear);
  if (m_live) {
    scheduleNext();
  }
  return node;
}

void ScreencopyView::itemChange(ItemChange change,
                                const ItemChangeData &value) {
  QQuickItem::itemChange(change, value);
  if (change == ItemSceneChange && value.window && !m_ready) {
    ensureBuffers();
  }
  if (change != ItemSceneChange && change != ItemVisibleHasChanged) {
    return;
  }
  if (!m_ready || !isComponentComplete() || !window()) {
    return;
  }
  if (!m_context) {
    createContext();
  } else if (wanted()) {
    m_clock.restart();
    m_context->captureFrame();
  }
}
