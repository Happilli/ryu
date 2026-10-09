#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>
#include <vector>

extern "C" {
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
}

class Cleave : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVariantList magnitudes READ magnitudes NOTIFY dataChanged)
  Q_PROPERTY(QVariantList peaks READ peaks NOTIFY dataChanged)
  Q_PROPERTY(
      int bandCount READ bandCount WRITE setBandCount NOTIFY bandCountChanged)
  Q_PROPERTY(
      float peakDecay READ peakDecay WRITE setPeakDecay NOTIFY peakDecayChanged)
  Q_PROPERTY(
      float smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
  Q_PROPERTY(bool active READ active NOTIFY activeChanged)
  Q_PROPERTY(int fps READ fps NOTIFY dataChanged)
  Q_PROPERTY(float silenceThreshold READ silenceThreshold WRITE
                 setSilenceThreshold NOTIFY silenceThresholdChanged)
  Q_PROPERTY(bool suspended READ suspended NOTIFY suspendedChanged)
  Q_PROPERTY(
      bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)
  Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)

public:
  explicit Cleave(QObject *parent = nullptr);
  ~Cleave() override;

  QVariantList magnitudes() const;
  QVariantList peaks() const;

  float silenceThreshold() const { return m_silenceThreshold; }
  void setSilenceThreshold(float v) {
    m_silenceThreshold = qBound(0.0f, v, 1.0f);
    emit silenceThresholdChanged();
  }

  int bandCount() const { return m_bandCount; }
  float peakDecay() const { return m_peakDecay; }
  float smoothing() const { return m_smoothing; }
  bool active() const { return m_active; }
  bool suspended() const { return m_suspended; }
  int fps() const { return m_fps; }
  bool debugMode() const { return m_debugMode; }
  QString source() const { return m_source; }

  void setBandCount(int n);
  void setPeakDecay(float v) {
    m_peakDecay = qBound(0.001f, v, 0.5f);
    emit peakDecayChanged();
  }
  void setSmoothing(float v) {
    m_smoothing = qBound(0.0f, v, 0.99f);
    emit smoothingChanged();
  }
  void setDebugMode(bool v) {
    m_debugMode = v;
    emit debugModeChanged();
  }
  void setSource(const QString &v) {
    m_source = v;
    emit sourceChanged();
  }

  Q_INVOKABLE bool startCapture();
  Q_INVOKABLE void stopCapture();
  Q_INVOKABLE void resumeCapture();
  Q_INVOKABLE void reset();

signals:
  void dataChanged();
  void bandCountChanged();
  void peakDecayChanged();
  void smoothingChanged();
  void activeChanged();
  void silenceThresholdChanged();
  void suspendedChanged();
  void debugModeChanged();
  void sourceChanged();
  void error(const QString &msg);

private slots:
  void onIdleTimeout();

private:
  void processPCM(const float *samples, uint32_t n_samples);
  void runFFT();
  void teardownStream();

  static void onProcess(void *userdata);
  static void onStreamStateChanged(void *userdata, enum pw_stream_state old,
                                   enum pw_stream_state state,
                                   const char *error);
  static void onParamChanged(void *userdata, uint32_t id,
                             const struct spa_pod *param);

  static constexpr int kFFTSize = 2048;
  static constexpr int kHalf = kFFTSize / 2;
  static constexpr int kFrameBytes = kFFTSize * sizeof(float);
  static constexpr int kBufCapSamples = kFFTSize * 32;
  static constexpr int kIdleMs = 3000;

  int m_bandCount = 32;
  float m_peakDecay = 0.02f;
  float m_smoothing = 0.75f;
  float m_silenceThreshold = 0.0001f;
  bool m_debugMode = false;
  QString m_source;

  bool m_active = false;
  bool m_silent = false;
  bool m_suspended = false;

  int m_fps = 0;
  int m_frameCount = 0;
  qint64 m_fpsTimer = 0;

  QVector<float> m_mag;
  QVector<float> m_pk;

  std::vector<float> m_pcmBuf;

  float m_re[kFFTSize];
  float m_im[kFFTSize];
  float m_win[kFFTSize];

  struct pw_thread_loop *m_loop = nullptr;
  struct pw_stream *m_stream = nullptr;
  struct pw_stream_events m_streamEvents = {};

  QTimer *m_idleTimer = nullptr;
};
