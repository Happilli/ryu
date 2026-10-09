#include "./materialslider.hpp"
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQmlFile>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kTrackH[] = {16.0, 24.0, 40.0, 56.0, 96.0};
constexpr qreal kHandleH[] = {44.0, 44.0, 52.0, 68.0, 108.0};
constexpr qreal kHandleW = 4.0;
constexpr qreal kHandleGap = 6.0;
constexpr qreal kInnerRadius = 2.0;
constexpr qreal kStopSize = 4.0;
constexpr qreal kEdgePad = 2.0;
constexpr qreal kBubbleW = 48.0;
constexpr qreal kBubbleH = 44.0;
constexpr qreal kBubbleGap = 4.0;
constexpr qreal kBubbleRadius = 16.0;
constexpr qreal kIndicatorSpace = 52.0;
constexpr qreal kDefaultLength = 200.0;
constexpr qreal kIconMinPad = 4.0;
constexpr qreal kIconMaxPad = 16.0;

QPainterPath sidedRect(qreal x0, qreal x1, qreal hh, qreal rl, qreal rr) {
  const qreal maxR = std::min(hh, (x1 - x0) / 2.0);
  rl = std::clamp(rl, 0.0, maxR);
  rr = std::clamp(rr, 0.0, maxR);

  QPainterPath p;
  p.moveTo(x0 + rl, -hh);
  p.lineTo(x1 - rr, -hh);
  if (rr > 0.0)
    p.arcTo(x1 - 2 * rr, -hh, 2 * rr, 2 * rr, 90, -90);
  p.lineTo(x1, hh - rr);
  if (rr > 0.0)
    p.arcTo(x1 - 2 * rr, hh - 2 * rr, 2 * rr, 2 * rr, 0, -90);
  p.lineTo(x0 + rl, hh);
  if (rl > 0.0)
    p.arcTo(x0, hh - 2 * rl, 2 * rl, 2 * rl, 270, -90);
  p.lineTo(x0, -hh + rl);
  if (rl > 0.0)
    p.arcTo(x0, -hh, 2 * rl, 2 * rl, 180, -90);
  p.closeSubpath();
  return p;
}
} // namespace

MaterialSlider::MaterialSlider(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setActiveFocusOnTab(true);

  m_pressAnim.setDuration(150);
  m_pressAnim.setEasingCurve(QEasingCurve::OutCubic);
  connect(&m_pressAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant &v) {
            m_press = v.toReal();
            update();
          });

  updateImplicit();
}

qreal MaterialSlider::trackHeight() const {
  return m_trackH >= 0.0 ? m_trackH : kTrackH[m_sizeStyle];
}

qreal MaterialSlider::handleWidth() const {
  return m_handleW >= 0.0 ? m_handleW : kHandleW;
}

qreal MaterialSlider::handleHeight() const {
  return m_handleH >= 0.0 ? m_handleH : kHandleH[m_sizeStyle];
}

qreal MaterialSlider::handleGap() const {
  return m_handleGap >= 0.0 ? m_handleGap : kHandleGap;
}

qreal MaterialSlider::innerRadius() const {
  return m_innerRadius >= 0.0 ? m_innerRadius : kInnerRadius;
}

qreal MaterialSlider::stopSize() const {
  return m_stopSize >= 0.0 ? m_stopSize : kStopSize;
}

void MaterialSlider::setOverride(qreal &field, qreal v,
                                 void (MaterialSlider::*sig)()) {
  if (v < 0.0)
    v = -1.0;
  if (field == v)
    return;
  field = v;
  (this->*sig)();
  updateImplicit();
  update();
}

void MaterialSlider::setTrackHeight(qreal v) {
  setOverride(m_trackH, v, &MaterialSlider::trackHeightChanged);
}

void MaterialSlider::setHandleWidth(qreal v) {
  setOverride(m_handleW, v, &MaterialSlider::handleWidthChanged);
}

void MaterialSlider::setHandleHeight(qreal v) {
  setOverride(m_handleH, v, &MaterialSlider::handleHeightChanged);
}

void MaterialSlider::setHandleGap(qreal v) {
  setOverride(m_handleGap, v, &MaterialSlider::handleGapChanged);
}

void MaterialSlider::setInnerRadius(qreal v) {
  setOverride(m_innerRadius, v, &MaterialSlider::innerRadiusChanged);
}

void MaterialSlider::setStopSize(qreal v) {
  setOverride(m_stopSize, v, &MaterialSlider::stopSizeChanged);
}

void MaterialSlider::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v), &MaterialSlider::borderWidthChanged);
}

qreal MaterialSlider::indicatorSpace() const {
  return m_showIndicator ? kIndicatorSpace : 0.0;
}

qreal MaterialSlider::length() const {
  return horizontal() ? width() : height();
}

qreal MaterialSlider::outerRadius() const {
  return m_outerRadius >= 0.0 ? m_outerRadius : trackHeight() / 2.0;
}

void MaterialSlider::setOuterRadius(qreal v) {
  setOverride(m_outerRadius, v, &MaterialSlider::outerRadiusChanged);
}

qreal MaterialSlider::crossCenter() const {
  const qreal cross = horizontal() ? height() : width();
  const qreal ind = indicatorSpace();
  return ind + (cross - ind) / 2.0;
}

void MaterialSlider::updateImplicit() {
  const qreal cross =
      std::max(handleHeight(), trackHeight()) + indicatorSpace();
  if (horizontal()) {
    setImplicitWidth(kDefaultLength);
    setImplicitHeight(cross);
  } else {
    setImplicitWidth(cross);
    setImplicitHeight(kDefaultLength);
  }
}

int MaterialSlider::resolvedDecimals() const {
  if (m_decimals >= 0)
    return m_decimals;
  if (m_step > 0.0) {
    int d = 0;
    qreal s = m_step;
    while (d < 4 && std::abs(s - std::round(s)) > 1e-6) {
      s *= 10.0;
      ++d;
    }
    return d;
  }
  const qreal r = m_to - m_from;
  return r >= 10.0 ? 0 : (r >= 1.0 ? 1 : 2);
}

QString MaterialSlider::valueText() const {
  return QString::number(m_value, 'f', resolvedDecimals());
}

qreal MaterialSlider::snap(qreal v) const {
  v = std::clamp(v, m_from, m_to);
  if (m_step > 0.0)
    v = std::clamp(m_from + std::round((v - m_from) / m_step) * m_step, m_from,
                   m_to);
  return v;
}

bool MaterialSlider::commit(qreal v) {
  v = snap(v);
  if (std::abs(v - m_value) < 1e-12)
    return false;
  m_value = v;
  emit valueChanged();
  emit moved();
  update();
  return true;
}

void MaterialSlider::reclamp() {
  if (m_to <= m_from)
    return;
  const qreal c = std::clamp(m_value, m_from, m_to);
  if (c != m_value) {
    m_value = c;
    emit valueChanged();
  }
}

void MaterialSlider::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  m_pressAnim.stop();
  m_pressAnim.setStartValue(m_press);
  m_pressAnim.setEndValue(v ? 1.0 : 0.0);
  m_pressAnim.start();
}

void MaterialSlider::setFrom(qreal v) {
  if (assign(m_from, v, &MaterialSlider::fromChanged))
    reclamp();
}

void MaterialSlider::setTo(qreal v) {
  if (assign(m_to, v, &MaterialSlider::toChanged))
    reclamp();
}

void MaterialSlider::setValue(qreal v) {
  if (m_to > m_from)
    v = std::clamp(v, m_from, m_to);
  if (m_value == v)
    return;
  m_value = v;
  emit valueChanged();
  update();
}

void MaterialSlider::setStepSize(qreal v) {
  assign(m_step, std::max(0.0, v), &MaterialSlider::stepSizeChanged);
}

void MaterialSlider::setOrientation(Orientation v) {
  if (assign(m_orientation, v, &MaterialSlider::orientationChanged)) {
    updateImplicit();
    const qreal w = width(), h = height();
    setWidth(h);
    setHeight(w);
  }
}

void MaterialSlider::setSizeStyle(SizeStyle v) {
  if (!assign(m_sizeStyle, v, &MaterialSlider::sizeStyleChanged))
    return;
  updateImplicit();
  if (m_trackH < 0.0)
    emit trackHeightChanged();
  if (m_handleH < 0.0)
    emit handleHeightChanged();
}

void MaterialSlider::setShowStops(bool v) {
  assign(m_showStops, v, &MaterialSlider::showStopsChanged);
}

void MaterialSlider::setShowValueIndicator(bool v) {
  if (assign(m_showIndicator, v, &MaterialSlider::showValueIndicatorChanged))
    updateImplicit();
}

void MaterialSlider::setDecimals(int v) {
  assign(m_decimals, v, &MaterialSlider::decimalsChanged);
}

void MaterialSlider::setIconSource(const QUrl &v) {
  if (assign(m_iconSource, v, &MaterialSlider::iconSourceChanged))
    m_iconDirty = true;
}

void MaterialSlider::setIconText(const QString &v) {
  assign(m_iconText, v, &MaterialSlider::iconTextChanged);
}

void MaterialSlider::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSlider::iconFontChanged);
}

void MaterialSlider::setIconColor(const QColor &v) {
  if (assign(m_iconColor, v, &MaterialSlider::iconColorChanged))
    m_iconDirty = true;
}

void MaterialSlider::setIconSize(qreal v) {
  if (assign(m_iconSize, std::max(0.0, v), &MaterialSlider::iconSizeChanged))
    m_iconDirty = true;
}

void MaterialSlider::setColor(const QColor &v) {
  assign(m_color, v, &MaterialSlider::colorChanged);
}

void MaterialSlider::setTrackColor(const QColor &v) {
  assign(m_trackColor, v, &MaterialSlider::trackColorChanged);
}

void MaterialSlider::setHandleColor(const QColor &v) {
  assign(m_handleColor, v, &MaterialSlider::handleColorChanged);
}

void MaterialSlider::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSlider::borderColorChanged);
}

void MaterialSlider::setIndicatorColor(const QColor &v) {
  assign(m_indicatorColor, v, &MaterialSlider::indicatorColorChanged);
}

void MaterialSlider::setIndicatorTextColor(const QColor &v) {
  assign(m_indicatorTextColor, v, &MaterialSlider::indicatorTextColorChanged);
}

void MaterialSlider::buildIcon(qreal dpr) {
  const int px = qRound(m_iconSize * dpr);
  if (!m_iconDirty && px == m_iconPx)
    return;
  m_iconDirty = false;
  m_iconPx = px;
  m_icon = QImage();
  if (px <= 0 || !m_iconSource.isValid() || m_iconSource.isEmpty())
    return;

  const QString path = QQmlFile::urlToLocalFileOrQrc(m_iconSource);
  QImageReader r(path);
  QSize s = r.size();
  if (s.isValid()) {
    s.scale(px, px, Qt::KeepAspectRatio);
    r.setScaledSize(s);
  }
  const QImage src = r.read();
  if (src.isNull()) {
    qWarning("MaterialSlider: failed to load icon '%s'", qPrintable(path));
    return;
  }

  QImage out(px, px, QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter q(&out);
  q.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QSize d = src.size();
  d.scale(px, px, Qt::KeepAspectRatio);
  q.drawImage(
      QRect((px - d.width()) / 2, (px - d.height()) / 2, d.width(), d.height()),
      src);
  q.setCompositionMode(QPainter::CompositionMode_SourceIn);
  q.fillRect(out.rect(), m_iconColor);
  q.end();
  out.setDevicePixelRatio(dpr);
  m_icon = std::move(out);
}

void MaterialSlider::updateFromPos(const QPointF &pt) {
  const qreal len = length();
  const qreal along = horizontal() ? pt.x() : height() - pt.y();
  const qreal span = std::max(1.0, len - 2.0 * kEdgePad);
  const qreal t = std::clamp((along - kEdgePad) / span, 0.0, 1.0);
  commit(m_from + t * (m_to - m_from));
}

void MaterialSlider::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  setKeepMouseGrab(true);
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  updateFromPos(e->position());
  e->accept();
}

void MaterialSlider::mouseMoveEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  updateFromPos(e->position());
  e->accept();
}

void MaterialSlider::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  updateFromPos(e->position());
  setPressed(false);
  setKeepMouseGrab(false);
  e->accept();
}

void MaterialSlider::mouseUngrabEvent() {
  setPressed(false);
  setKeepMouseGrab(false);
}

void MaterialSlider::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  const qreal step = m_step > 0.0 ? m_step : (m_to - m_from) / 20.0;
  switch (e->key()) {
  case Qt::Key_Left:
  case Qt::Key_Down:
    commit(m_value - step);
    break;
  case Qt::Key_Right:
  case Qt::Key_Up:
    commit(m_value + step);
    break;
  case Qt::Key_Home:
    commit(m_from);
    break;
  case Qt::Key_End:
    commit(m_to);
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialSlider::paint(QPainter *p) {
  const qreal len = length();
  const qreal range = m_to - m_from;
  if (len <= 0.0 || range <= 0.0)
    return;

  p->setRenderHint(QPainter::Antialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const qreal cc = crossCenter();
  if (horizontal()) {
    p->translate(0.0, cc);
  } else {
    p->translate(cc, height());
    p->rotate(-90.0);
  }

  const qreal trackH = trackHeight();
  const qreal hh = trackH / 2.0;
  const qreal handleH = handleHeight();
  const qreal baseW = handleWidth();
  const qreal hw = baseW + (baseW * 0.5 - baseW) * m_press;
  const qreal gap = handleGap();
  const qreal inner = innerRadius();
  const qreal dotR = stopSize() / 2.0;
  const qreal span = len - 2.0 * kEdgePad;
  const qreal t = (m_value - m_from) / range;
  const qreal hx = kEdgePad + t * span;
  const qreal activeEnd = hx - hw / 2.0 - gap;
  const qreal inactiveStart = hx + hw / 2.0 + gap;
  p->setPen(Qt::NoPen);

  const qreal outer = outerRadius();
  const QPainterPath pill = sidedRect(0.0, len, hh, outer, outer);

  const bool hasActive = activeEnd > 0.0;
  const bool hasInactive = inactiveStart < len;
  QPainterPath activePath;
  QPainterPath inactivePath;

  if (hasActive) {
    activePath = sidedRect(0.0, activeEnd, hh, 0.0, inner).intersected(pill);
    p->setBrush(m_color);
    p->drawPath(activePath);
  }
  if (hasInactive) {
    inactivePath =
        sidedRect(inactiveStart, len, hh, inner, 0.0).intersected(pill);
    p->setBrush(m_trackColor);
    p->drawPath(inactivePath);
  }

  if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0) {
    auto stroke = [&](const QPainterPath &seg) {
      p->save();
      p->setClipPath(seg);
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(m_borderColor, m_borderWidth * 2.0, Qt::SolidLine,
                     Qt::FlatCap, Qt::RoundJoin));
      p->drawPath(seg);
      p->restore();
    };
    if (hasActive)
      stroke(activePath);
    if (hasInactive)
      stroke(inactivePath);
    p->setPen(Qt::NoPen);
  }

  const bool hasImage = m_iconSource.isValid() && !m_iconSource.isEmpty() &&
                        m_iconSize > 0.0 && trackH >= m_iconSize;
  const bool hasGlyph =
      !m_iconText.isEmpty() && m_iconSize > 0.0 && trackH >= m_iconSize;
  if (hasImage || hasGlyph) {
    const qreal pad =
        std::clamp((trackH - m_iconSize) / 2.0, kIconMinPad, kIconMaxPad);
    const qreal iconCx = pad + m_iconSize / 2.0;
    const bool fits = activeEnd >= iconCx + m_iconSize / 2.0 + kIconMinPad;

    if (hasImage && fits) {
      buildIcon(window() ? window()->devicePixelRatio() : 1.0);
      if (!m_icon.isNull()) {
        p->save();
        p->translate(iconCx, 0.0);
        if (!horizontal())
          p->rotate(90.0);
        p->drawImage(QPointF(-m_iconSize / 2.0, -m_iconSize / 2.0), m_icon);
        p->restore();
      }
    }

    if (hasGlyph && fits) {
      QFont f = m_iconFont;
      f.setPixelSize(qMax(1, qRound(m_iconSize)));
      f.setHintingPreference(QFont::PreferNoHinting);

      QPainterPath gp;
      gp.addText(QPointF(0.0, 0.0), f, m_iconText);
      const QRectF br = gp.boundingRect();

      if (!br.isEmpty()) {
        const qreal k =
            std::min(1.0, m_iconSize / std::max(br.width(), br.height()));

        p->save();
        p->translate(iconCx, 0.0);
        if (!horizontal())
          p->rotate(90.0);
        p->scale(k, k);
        p->translate(-br.center());
        p->setPen(Qt::NoPen);
        p->setBrush(m_iconColor);
        p->drawPath(gp);
        p->restore();
        p->setPen(Qt::NoPen);
      }
    }
  }

  if (m_showStops && dotR > 0.0) {
    auto dot = [&](qreal x) {
      if (x <= activeEnd - dotR) {
        p->setBrush(m_trackColor);
        p->drawEllipse(QPointF(x, 0.0), dotR, dotR);
      } else if (x >= inactiveStart + dotR) {
        p->setBrush(m_color);
        p->drawEllipse(QPointF(x, 0.0), dotR, dotR);
      }
    };

    const qreal inset = std::min(hh, 10.0);
    if (m_step > 0.0 && range / m_step <= 100.5) {
      const int n = qRound(range / m_step);
      dot(inset);
      for (int i = 1; i < n; ++i)
        dot(kEdgePad + (i * m_step / range) * span);
    }
    dot(len - inset);
  }

  p->setBrush(m_handleColor);
  p->drawRoundedRect(QRectF(hx - hw / 2.0, -handleH / 2.0, hw, handleH),
                     hw / 2.0, hw / 2.0);

  if (m_showIndicator && m_press > 0.01) {
    const qreal half = kBubbleW / 2.0;
    const qreal bx = len >= kBubbleW ? std::clamp(hx, half, len - half) : hx;
    const qreal by = -(handleH / 2.0 + kBubbleGap + kBubbleH / 2.0);

    p->save();
    p->translate(bx, by);
    p->scale(m_press, m_press);
    p->setPen(Qt::NoPen);
    p->setBrush(m_indicatorColor);
    const QRectF r(-kBubbleW / 2.0, -kBubbleH / 2.0, kBubbleW, kBubbleH);
    p->drawRoundedRect(r, kBubbleRadius, kBubbleRadius);
    if (!horizontal())
      p->rotate(90.0);
    QFont f = p->font();
    f.setPixelSize(14);
    f.setWeight(QFont::Medium);
    p->setFont(f);
    p->setPen(m_indicatorTextColor);
    p->drawText(r, Qt::AlignCenter, valueText());
    p->restore();
  }
}
