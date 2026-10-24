#include "./wavyprogress.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kTwoPi = 2.0 * M_PI;
constexpr qreal kCycle = 1.8;
constexpr qreal kSpin = 1.4;
constexpr qreal kMaxDt = 1.0 / 30.0;
constexpr qreal kSuperSample = 2.0;
constexpr int kMaxTexSide = 4096;
constexpr qreal kSegPerWave = 16.0;
constexpr qreal kArcStepsPerWave = 48.0;

qreal easeStep(qreal x) {
  x = std::clamp(x, 0.0, 1.0);
  return x * x * (3.0 - 2.0 * x);
}
} // namespace

WavyProgress::WavyProgress(QQuickItem *parent) : RyuItem(parent) {
  setSmooth(true);
  setMipmap(true);
}

void WavyProgress::propertyAssigned() { syncFrames(window()); }

void WavyProgress::componentComplete() {
  QQuickPaintedItem::componentComplete();
  updateTexture(window());
  syncFrames(window());
}

void WavyProgress::itemChange(ItemChange change, const ItemChangeData &data) {
  QQuickPaintedItem::itemChange(change, data);
  if (change == ItemSceneChange) {
    if (m_conn) {
      disconnect(m_conn);
      m_conn = {};
    }
    syncFrames(data.window);
    updateTexture(data.window);
  } else if (change == ItemVisibleHasChanged) {
    syncFrames(window());
  }
}

void WavyProgress::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickPaintedItem::geometryChange(n, o);
  if (n.size() != o.size())
    updateTexture(window());
}

void WavyProgress::updateTexture(QQuickWindow *w) {
  if (width() <= 0.0 || height() <= 0.0) {
    setTextureSize(QSize());
    return;
  }
  const qreal dpr = w ? w->effectiveDevicePixelRatio() : 1.0;
  const qreal k = dpr * kSuperSample;
  setTextureSize(QSize(std::min(kMaxTexSide, int(std::ceil(width() * k))),
                       std::min(kMaxTexSide, int(std::ceil(height() * k)))));
}

void WavyProgress::syncFrames(QQuickWindow *w) {
  const bool need = w && isComponentComplete() && isVisible() &&
                    (m_indeterminate || (m_amp > 0.0 && m_waveSpeed != 0.0));
  if (need) {
    if (!m_conn) {
      m_clock.start();
      m_conn = connect(w, &QQuickWindow::afterAnimating, this,
                       &WavyProgress::onFrame);
      update();
    }
  } else if (m_conn) {
    disconnect(m_conn);
    m_conn = {};
  }
}

void WavyProgress::onFrame() {
  const qreal dt = std::min(qreal(m_clock.restart()) / 1000.0, kMaxDt);
  m_phase = std::fmod(m_phase + dt * m_waveSpeed * kTwoPi, kTwoPi);
  m_time = std::fmod(m_time + dt, kCycle * kSpin * 100.0);
  update();
}

void WavyProgress::setStyle(Style v) {
  assign(m_style, v, &WavyProgress::styleChanged);
}
void WavyProgress::setProgress(qreal v) {
  assign(m_progress, std::clamp(v, 0.0, 1.0), &WavyProgress::progressChanged);
}
void WavyProgress::setIndeterminate(bool v) {
  assign(m_indeterminate, v, &WavyProgress::indeterminateChanged);
}
void WavyProgress::setColor(const QColor &v) {
  assign(m_color, v, &WavyProgress::colorChanged);
}
void WavyProgress::setTrackColor(const QColor &v) {
  assign(m_trackColor, v, &WavyProgress::trackColorChanged);
}
void WavyProgress::setAmplitude(qreal v) {
  assign(m_amp, std::max(0.0, v), &WavyProgress::amplitudeChanged);
}
void WavyProgress::setWavelength(qreal v) {
  assign(m_wavelength, std::max(4.0, v), &WavyProgress::wavelengthChanged);
}
void WavyProgress::setWaveSpeed(qreal v) {
  assign(m_waveSpeed, v, &WavyProgress::waveSpeedChanged);
}
void WavyProgress::setStrokeWidth(qreal v) {
  assign(m_stroke, std::max(0.5, v), &WavyProgress::strokeWidthChanged);
}
void WavyProgress::setGap(qreal v) {
  assign(m_gap, std::max(0.0, v), &WavyProgress::gapChanged);
}

void WavyProgress::segment(qreal &a, qreal &b) const {
  if (!m_indeterminate) {
    a = 0.0;
    b = m_progress;
    return;
  }
  const qreal u = std::fmod(m_time, kCycle) / kCycle;
  b = easeStep(u / 0.7);
  a = easeStep((u - 0.3) / 0.7);
}

void WavyProgress::paint(QPainter *p) {
  preparePainter(p);
  qreal a, b;
  segment(a, b);
  if (m_style == Linear)
    paintLinear(p, a, b);
  else
    paintCircular(p, a, b);
}

void WavyProgress::paintLinear(QPainter *p, qreal a, qreal b) {
  const qreal pad = m_stroke / 2.0;
  const qreal x0 = pad, x1 = width() - pad, len = x1 - x0;
  if (len <= 0.0)
    return;
  const qreal cy = height() / 2.0;
  const qreal amp = std::min(m_amp, std::max(0.0, (height() - m_stroke) / 2.0));
  const qreal xa = x0 + len * a, xb = x0 + len * b;
  const qreal g = m_gap + m_stroke;

  QPen pen(m_trackColor, m_stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  p->setBrush(Qt::NoBrush);
  p->setPen(pen);
  if (xa - g > x0)
    p->drawLine(QPointF(x0, cy), QPointF(xa - g, cy));
  if (xb + g < x1)
    p->drawLine(QPointF(xb + g, cy), QPointF(x1, cy));

  if (xb - xa < 0.01) {
    if (b > 0.0 || m_indeterminate) {
      pen.setColor(m_color);
      p->setPen(pen);
      p->drawPoint(QPointF(xa, cy));
    }
    return;
  }

  const qreal k = kTwoPi / m_wavelength;
  const qreal ph = m_phase;
  auto yAt = [&](qreal x) { return cy + amp * std::sin(k * x - ph); };
  auto dyAt = [&](qreal x) { return amp * k * std::cos(k * x - ph); };

  const int segs =
      std::max(1, int(std::ceil((xb - xa) / (m_wavelength / kSegPerWave))));
  const qreal dx = (xb - xa) / segs;

  QPainterPath path;
  path.moveTo(xa, yAt(xa));
  for (int i = 0; i < segs; ++i) {
    const qreal sx0 = xa + i * dx;
    const qreal sx1 = (i == segs - 1) ? xb : sx0 + dx;
    const qreal h = (sx1 - sx0) / 3.0;
    path.cubicTo(QPointF(sx0 + h, yAt(sx0) + dyAt(sx0) * h),
                 QPointF(sx1 - h, yAt(sx1) - dyAt(sx1) * h),
                 QPointF(sx1, yAt(sx1)));
  }
  pen.setColor(m_color);
  p->setPen(pen);
  p->drawPath(path);
}

void WavyProgress::paintCircular(QPainter *p, qreal a, qreal b) {
  const qreal amp = m_amp;
  const qreal r = std::min(width(), height()) / 2.0 - m_stroke / 2.0 - amp;
  if (r <= 0.0)
    return;
  const QPointF c(width() / 2.0, height() / 2.0);
  const qreal rot = m_indeterminate ? kTwoPi * (m_time / kSpin) : 0.0;

  auto pt = [&](qreal th, qreal rad) {
    const qreal t = th + rot;
    return QPointF(c.x() + rad * std::sin(t), c.y() - rad * std::cos(t));
  };

  const int n = std::max(1, int(std::round(kTwoPi * r / m_wavelength)));
  const qreal ga = (m_gap + m_stroke) / r;
  const qreal ta = kTwoPi * a, tb = kTwoPi * b;

  QPen pen(m_trackColor, m_stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  p->setBrush(Qt::NoBrush);
  p->setPen(pen);

  const qreal t0 = tb + ga, t1 = kTwoPi + ta - ga;
  if (t1 > t0) {
    const QRectF box(c.x() - r, c.y() - r, 2.0 * r, 2.0 * r);
    const qreal deg = 180.0 / M_PI;
    const qreal startDeg = 90.0 - (t0 + rot) * deg;
    QPainterPath track;
    track.arcMoveTo(box, startDeg);
    track.arcTo(box, startDeg, -(t1 - t0) * deg);
    p->drawPath(track);
  }

  pen.setColor(m_color);
  p->setPen(pen);
  if (tb - ta < 0.001) {
    p->drawPoint(pt(ta, r));
    return;
  }
  QPainterPath arc;
  const int steps =
      std::max(32, int((tb - ta) / kTwoPi * n * kArcStepsPerWave));
  for (int i = 0; i <= steps; ++i) {
    const qreal th = ta + (tb - ta) * i / steps;
    const QPointF q = pt(th, r + amp * std::sin(n * th - m_phase));
    i == 0 ? arc.moveTo(q) : arc.lineTo(q);
  }
  p->drawPath(arc);
}
