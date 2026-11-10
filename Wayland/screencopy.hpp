#pragma once

#include <QElapsedTimer>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSize>
#include <atomic>

namespace ryu::wl {
class ScreencopyContext;
}

class ScreencopyView : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QObject *captureSource READ captureSource WRITE setCaptureSource
                 NOTIFY captureSourceChanged)
  Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
  Q_PROPERTY(bool hasContent READ hasContent NOTIFY hasContentChanged)
  Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY sourceSizeChanged)
  Q_PROPERTY(bool fullResolution READ fullResolution WRITE setFullResolution
                 NOTIFY fullResolutionChanged)
  Q_PROPERTY(int maxFps READ maxFps WRITE setMaxFps NOTIFY maxFpsChanged)
  Q_PROPERTY(
      bool hardware READ hardware WRITE setHardware NOTIFY hardwareChanged)
  Q_PROPERTY(bool accelerated READ accelerated NOTIFY acceleratedChanged)

public:
  explicit ScreencopyView(QQuickItem *parent = nullptr);
  ~ScreencopyView() override;

  QObject *captureSource() const { return m_source.data(); }
  void setCaptureSource(QObject *source);

  bool live() const { return m_live; }
  void setLive(bool live);

  bool hasContent() const { return m_hasContent; }
  QSize sourceSize() const { return m_sourceSize; }

  bool fullResolution() const { return m_full; }
  void setFullResolution(bool full);

  int maxFps() const { return m_fps; }
  void setMaxFps(int fps);

  bool hardware() const { return m_hardware; }
  void setHardware(bool hardware);

  bool accelerated() const { return m_accelerated; }

signals:
  void captureSourceChanged();
  void liveChanged();
  void hasContentChanged();
  void sourceSizeChanged();
  void fullResolutionChanged();
  void maxFpsChanged();
  void hardwareChanged();
  void acceleratedChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override;
  void componentComplete() override;
  void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
  bool wanted() const;
  void ensureBuffers();
  void onBuffersReady();
  void createContext();
  void destroyContext(bool update);
  void onFrameCaptured();
  void scheduleNext();
  void advance();

  QPointer<QObject> m_source;
  ryu::wl::ScreencopyContext *m_context = nullptr;
  QSize m_sourceSize;
  QElapsedTimer m_clock;
  std::atomic<bool> m_nextQueued{false};
  int m_fps = 30;
  bool m_live = false;
  bool m_full = false;
  bool m_hardware = true;
  bool m_accelerated = false;
  bool m_hasContent = false;
  bool m_ready = false;
  bool m_timerPending = false;
};
