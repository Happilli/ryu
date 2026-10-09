#include "cleave.hpp"
#include <QDateTime>
#include <QMetaObject>
#include <QtMath>
#include <algorithm>
#include <cstring>

static void fftInPlace(float *re, float *im, int n) {
  for (int i = 1, j = 0; i < n; ++i) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
      std::swap(im[i], im[j]);
    }
  }
  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * float(M_PI) / len;
    float wRe = cosf(ang), wIm = sinf(ang);
    for (int i = 0; i < n; i += len) {
      float cRe = 1.0f, cIm = 0.0f;
      for (int j = 0; j < len / 2; ++j) {
        float uRe = re[i + j], uIm = im[i + j];
        float vRe = re[i + j + len / 2], vIm = im[i + j + len / 2];
        float tRe = cRe * vRe - cIm * vIm, tIm = cRe * vIm + cIm * vRe;
        re[i + j] = uRe + tRe;
        im[i + j] = uIm + tIm;
        re[i + j + len / 2] = uRe - tRe;
        im[i + j + len / 2] = uIm - tIm;
        float nr = cRe * wRe - cIm * wIm;
        cIm = cRe * wIm + cIm * wRe;
        cRe = nr;
      }
    }
  }
}

static void staticInitPw() {
  static bool inited = false;
  if (!inited) {
    pw_init(nullptr, nullptr);
    inited = true;
  }
}

Cleave::Cleave(QObject *parent) : QObject(parent) {
  staticInitPw();

  m_mag.resize(m_bandCount, 0.0f);
  m_pk.resize(m_bandCount, 0.0f);
  m_pcmBuf.reserve(kBufCapSamples);
  for (int i = 0; i < kFFTSize; ++i)
    m_win[i] = 0.5f * (1.0f - cosf(2.0f * float(M_PI) * i / (kFFTSize - 1)));
  m_fpsTimer = QDateTime::currentMSecsSinceEpoch();

  m_idleTimer = new QTimer(this);
  m_idleTimer->setSingleShot(true);
  m_idleTimer->setInterval(kIdleMs);
  connect(m_idleTimer, &QTimer::timeout, this, &Cleave::onIdleTimeout);
}

Cleave::~Cleave() { stopCapture(); }

QVariantList Cleave::magnitudes() const {
  QVariantList out;
  out.reserve(m_bandCount);
  for (float v : m_mag)
    out.append(v);
  return out;
}

QVariantList Cleave::peaks() const {
  QVariantList out;
  out.reserve(m_bandCount);
  for (float v : m_pk)
    out.append(v);
  return out;
}

void Cleave::setBandCount(int n) {
  n = qBound(2, n, 512);
  if (n == m_bandCount)
    return;
  m_bandCount = n;
  m_mag.assign(n, 0.0f);
  m_pk.assign(n, 0.0f);
  emit bandCountChanged();
}

void Cleave::teardownStream() {
  if (!m_loop)
    return;

  pw_thread_loop_lock(m_loop);
  if (m_stream) {
    pw_stream_destroy(m_stream);
    m_stream = nullptr;
  }
  pw_thread_loop_unlock(m_loop);

  pw_thread_loop_stop(m_loop);
  pw_thread_loop_destroy(m_loop);
  m_loop = nullptr;
}

bool Cleave::startCapture() {
  teardownStream();
  m_idleTimer->stop();
  m_suspended = false;
  m_silent = false;
  m_pcmBuf.clear();

  m_loop = pw_thread_loop_new("cleave-capture", nullptr);
  if (!m_loop) {
    emit error("failed to create pipewire thread loop");
    return false;
  }

  pw_thread_loop_lock(m_loop);

  m_streamEvents = {};
  m_streamEvents.version = PW_VERSION_STREAM_EVENTS;
  m_streamEvents.process = &Cleave::onProcess;
  m_streamEvents.state_changed = &Cleave::onStreamStateChanged;
  m_streamEvents.param_changed = &Cleave::onParamChanged;

  auto *props =
      pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                        "Capture", PW_KEY_MEDIA_ROLE, "Music", nullptr);

  if (!m_source.isEmpty()) {
    QString target = m_source;
    const QString suffix = QStringLiteral(".monitor");
    bool isMonitor = target.endsWith(suffix);
    if (isMonitor)
      target.chop(suffix.size());

    pw_properties_set(props, PW_KEY_TARGET_OBJECT, target.toUtf8().constData());
    if (isMonitor)
      pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
  } else {
    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
  }

  m_stream = pw_stream_new_simple(pw_thread_loop_get_loop(m_loop), "cleave-viz",
                                  props, &m_streamEvents, this);

  if (!m_stream) {
    pw_thread_loop_unlock(m_loop);
    emit error("failed to create pipewire stream");
    teardownStream();
    return false;
  }

  uint8_t buffer[1024];
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  struct spa_audio_info_raw info = {};
  info.format = SPA_AUDIO_FORMAT_F32;
  info.rate = 48000;
  info.channels = 1;

  const struct spa_pod *params[1];
  params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

  int res = pw_stream_connect(
      m_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                   PW_STREAM_FLAG_RT_PROCESS),
      params, 1);

  pw_thread_loop_unlock(m_loop);

  if (res < 0) {
    emit error("pw_stream_connect failed");
    teardownStream();
    return false;
  }

  if (pw_thread_loop_start(m_loop) < 0) {
    emit error("failed to start pipewire thread loop");
    teardownStream();
    return false;
  }

  if (m_debugMode)
    qDebug("[Cleave] capture started, source=%s", qPrintable(m_source));

  m_active = true;
  emit activeChanged();
  return true;
}

void Cleave::stopCapture() {
  m_idleTimer->stop();
  teardownStream();
  m_pcmBuf.clear();
  m_active = false;
  m_suspended = false;
  m_silent = false;
  if (m_debugMode)
    qDebug("[Cleave] capture stopped");
  emit activeChanged();
}

void Cleave::resumeCapture() {
  if (m_debugMode)
    qDebug("[Cleave] resuming capture");
  bool wasSuspended = m_suspended;
  startCapture();
  if (wasSuspended)
    emit suspendedChanged();
}

void Cleave::reset() {
  m_pcmBuf.clear();
  m_mag.fill(0.0f);
  m_pk.fill(0.0f);
  m_silent = false;
  emit dataChanged();
}

void Cleave::onProcess(void *userdata) {
  auto *self = static_cast<Cleave *>(userdata);
  struct pw_buffer *b = pw_stream_dequeue_buffer(self->m_stream);
  if (!b)
    return;

  struct spa_buffer *buf = b->buffer;
  if (buf->datas[0].data && buf->datas[0].chunk->size > 0) {
    const float *samples = static_cast<const float *>(buf->datas[0].data);
    uint32_t n_samples = buf->datas[0].chunk->size / sizeof(float);

    QVector<float> chunk(samples, samples + n_samples);
    QMetaObject::invokeMethod(
        self,
        [self, chunk = std::move(chunk)]() {
          self->processPCM(chunk.constData(), chunk.size());
        },
        Qt::QueuedConnection);
  }

  pw_stream_queue_buffer(self->m_stream, b);
}

void Cleave::onStreamStateChanged(void *userdata, enum pw_stream_state,
                                  enum pw_stream_state state,
                                  const char *error) {
  auto *self = static_cast<Cleave *>(userdata);
  if (self->m_debugMode)
    qDebug("[Cleave] stream state -> %s", pw_stream_state_as_string(state));

  if (state == PW_STREAM_STATE_ERROR) {
    QMetaObject::invokeMethod(
        self,
        [self, err = QString(error ? error : "unknown")]() {
          emit self->error("pipewire stream error: " + err);
        },
        Qt::QueuedConnection);
  }
}

void Cleave::onParamChanged(void *, uint32_t, const struct spa_pod *) {}

void Cleave::processPCM(const float *samples, uint32_t n_samples) {
  m_pcmBuf.insert(m_pcmBuf.end(), samples, samples + n_samples);
  if (m_pcmBuf.size() > size_t(kBufCapSamples))
    m_pcmBuf.erase(m_pcmBuf.begin(),
                   m_pcmBuf.begin() + (m_pcmBuf.size() - kBufCapSamples));

  bool anySound = false;

  while (m_pcmBuf.size() >= size_t(kFFTSize)) {
    std::memcpy(m_re, m_pcmBuf.data(), kFrameBytes);
    m_pcmBuf.erase(m_pcmBuf.begin(), m_pcmBuf.begin() + kHalf);

    float rms = 0.0f;
    for (int i = 0; i < kFFTSize; ++i)
      rms += m_re[i] * m_re[i];
    rms = sqrtf(rms / kFFTSize);
    if (rms < m_silenceThreshold)
      continue;

    anySound = true;
    runFFT();
  }

  if (anySound) {
    m_silent = false;
    m_suspended = false;
    m_idleTimer->start();
    emit dataChanged();
    return;
  }

  if (m_silent)
    return;

  bool stillActive = false;
  for (int b = 0; b < m_bandCount; ++b) {
    m_mag[b] = qMax(0.0f, m_mag[b] - m_peakDecay);
    m_pk[b] = qMax(0.0f, m_pk[b] - m_peakDecay);
    if (m_mag[b] > 0.0f || m_pk[b] > 0.0f)
      stillActive = true;
  }
  emit dataChanged();
  if (!stillActive)
    m_silent = true;
}

void Cleave::onIdleTimeout() {
  if (m_debugMode)
    qDebug("[Cleave] idle -- suspending stream");
  teardownStream();
  m_pcmBuf.clear();
  std::fill(m_re, m_re + kFFTSize, 0.0f);
  std::fill(m_im, m_im + kFFTSize, 0.0f);
  m_mag.fill(0.0f);
  m_pk.fill(0.0f);
  m_active = false;
  m_suspended = true;
  m_silent = true;
  emit dataChanged();
  emit activeChanged();
  emit suspendedChanged();
}

void Cleave::runFFT() {
  for (int i = 0; i < kFFTSize; ++i) {
    m_re[i] *= m_win[i];
    m_im[i] = 0.0f;
  }
  fftInPlace(m_re, m_im, kFFTSize);

  float mag[kHalf];
  for (int i = 0; i < kHalf; ++i) {
    float re = m_re[i], im = m_im[i];
    mag[i] = sqrtf(re * re + im * im) / kHalf;
  }

  auto sampleBin = [&](float f) {
    int i = qBound(0, int(f), kHalf - 2);
    float frac = f - float(i);
    return mag[i] * (1.0f - frac) + mag[i + 1] * frac;
  };

  const float minBin = 3.0f;
  const float maxBin = 700.0f;
  const float logMin = logf(minBin);
  const float logMax = logf(maxBin);
  const float step = (logMax - logMin) / m_bandCount;

  for (int b = 0; b < m_bandCount; ++b) {
    const float fLo = expf(logMin + b * step);
    const float fHi = expf(logMin + (b + 1) * step);
    const float center = sqrtf(fLo * fHi);

    float raw;
    if (fHi - fLo < 1.0f) {
      raw = sampleBin(center);
    } else {
      int lo = qBound(1, int(fLo + 0.5f), kHalf - 1);
      int hi = qBound(lo, int(fHi + 0.5f), kHalf - 1);
      float sum = 0.0f;
      for (int k = lo; k <= hi; ++k)
        sum += mag[k];
      raw = sum / (hi - lo + 1);
    }

    raw *= sqrtf(center / minBin);

    float next = m_smoothing * m_mag[b] + (1.0f - m_smoothing) * raw;
    m_mag[b] = next;
    if (next >= m_pk[b])
      m_pk[b] = next;
    else
      m_pk[b] = qMax(0.0f, m_pk[b] - m_peakDecay);
  }

  ++m_frameCount;
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (now - m_fpsTimer >= 1000) {
    m_fps = m_frameCount;
    m_frameCount = 0;
    m_fpsTimer = now;
    if (m_debugMode)
      qDebug("[Cleave] fps: %d | band[0] mag=%.4f peak=%.4f", m_fps, m_mag[0],
             m_pk[0]);
  }
}
