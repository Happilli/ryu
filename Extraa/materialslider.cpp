#include "./materialslider.hpp"
#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
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
constexpr qreal kPressStiffness = 1.4;
constexpr qreal kMaxBubbleScale = 1.15;

QColor faded(QColor c, qreal k) {
  c.setAlphaF(c.alphaF() * k);
  return c;
}
} // namespace

MaterialSlider::MaterialSlider(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.6) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setActiveFocusOnTab(true);
  updateImplicit();
}

MaterialSlider::~MaterialSlider() {
  if (m_cursorOverridden)
    QGuiApplication::restoreOverrideCursor();
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

qreal MaterialSlider::outerRadius() const {
  return m_outerRadius >= 0.0 ? m_outerRadius : trackHeight() / 2.0;
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

void MaterialSlider::setOuterRadius(qreal v) {
  setOverride(m_outerRadius, v, &MaterialSlider::outerRadiusChanged);
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

  if (v && !m_cursorOverridden) {
    QGuiApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
    m_cursorOverridden = true;
  } else if (!v && m_cursorOverridden) {
    QGuiApplication::restoreOverrideCursor();
    m_cursorOverridden = false;
  }

  m_press.go(v ? 1.0 : 0.0);
  animate();
  update();
}

bool MaterialSlider::advance(qreal dt) {
  return step(m_press, dt, 0.001, kPressStiffness);
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
    m_iconPath.clear();
}

void MaterialSlider::setIconText(const QString &v) {
  assign(m_iconText, v, &MaterialSlider::iconTextChanged);
}

void MaterialSlider::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSlider::iconFontChanged);
}

void MaterialSlider::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialSlider::iconColorChanged);
}

void MaterialSlider::setIconSize(qreal v) {
  assign(m_iconSize, std::max(0.0, v), &MaterialSlider::iconSizeChanged);
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

QSGNode *MaterialSlider::updatePaintNode(QSGNode *old, UpdatePaintNodeData *d) {
  if (!old) {
    m_iconNode = nullptr;
    m_textNode = nullptr;
    m_iconLayer.shown.clear();
    m_textLayer.shown.clear();
  }
  QSGNode *root = SgMaterialItem::updatePaintNode(old, d);
  syncLayer(root, m_iconNode, m_iconLayer);
  syncLayer(root, m_textNode, m_textLayer);
  return root;
}

void MaterialSlider::syncLayer(QSGNode *root, QSGSimpleTextureNode *&node,
                               Layer &l) {
  if (!l.on || l.img.isNull() || !window()) {
    if (node) {
      root->removeChildNode(node);
      delete node;
      node = nullptr;
    }
    return;
  }

  if (!node) {
    node = new QSGSimpleTextureNode;
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);
    root->appendChildNode(node);
    l.shown.clear();
  }

  if (l.shown != l.key) {
    node->setTexture(window()->createTextureFromImage(l.img));
    l.shown = l.key;
  }
  node->setRect(l.rect);
}
void MaterialSlider::build(Ryu::Mesh &m) {
  m_iconLayer.on = false;
  m_textLayer.on = false;

  const qreal len = length();
  const qreal range = m_to - m_from;
  if (len <= 0.0 || range <= 0.0)
    return;

  const bool hz = horizontal();
  const qreal cc = crossCenter();
  const qreal dpr = m.dpr();
  const qreal fade = isEnabled() ? 1.0 : Ryu::kDisabledOpacity;
  const qreal H = height();

  const qreal trackH = trackHeight();
  const qreal hh = trackH / 2.0;
  const qreal handleH = handleHeight();
  const qreal baseW = handleWidth();
  const qreal press = std::max(0.0, m_press.x);
  const qreal hw = std::max(0.5, baseW + (baseW * 0.5 - baseW) * press);
  const qreal gap = handleGap();
  const qreal inner = std::min(innerRadius(), hh);
  const qreal outer = std::min(outerRadius(), hh);
  const qreal dotR = stopSize() / 2.0;
  const qreal span = len - 2.0 * kEdgePad;
  const qreal t = (m_value - m_from) / range;
  const qreal hx = kEdgePad + t * span;
  const qreal activeEnd = hx - hw / 2.0 - gap;
  const qreal inactiveStart = hx + hw / 2.0 + gap;

  auto P = [&](qreal along, qreal cross) {
    return hz ? QPointF(along, cc + cross) : QPointF(cc + cross, H - along);
  };
  auto R = [&](qreal a, qreal b, qreal c0, qreal c1) {
    return QRectF(P(a, c0), P(b, c1)).normalized();
  };

  auto pillHalf = [&](qreal x) {
    if (outer <= 0.0)
      return hh;
    qreal d = 0.0;
    if (x < outer)
      d = outer - x;
    else if (x > len - outer)
      d = x - (len - outer);
    else
      return hh;
    return hh - outer + std::sqrt(std::max(0.0, outer * outer - d * d));
  };

  auto segment = [&](qreal a, qreal b, qreal rl, qreal rr) {
    QList<QPointF> out;
    a = std::max(a, 0.0);
    b = std::min(b, len);
    const qreal w = b - a;
    if (w < 0.5)
      return out;

    rl = std::clamp(rl, 0.0, hh);
    rr = std::clamp(rr, 0.0, hh);
    if (rl + rr > w) {
      const qreal k = w / (rl + rr);
      rl *= k;
      rr *= k;
    }

    auto add = [&](qreal x, qreal y) {
      const qreal lim = pillHalf(x);
      const QPointF p = P(x, std::clamp(y, -lim, lim));
      if (!out.isEmpty() && (out.last() - p).manhattanLength() < 1e-4)
        return;
      out.append(p);
    };

    auto arc = [&](qreal cx, qreal cy, qreal r, qreal a0) {
      if (r < 1e-4) {
        add(cx, cy);
        return;
      }
      const int n = int(std::clamp(r * dpr / 3.0, 4.0, 24.0));
      for (int i = 0; i <= n; ++i) {
        const qreal ang = a0 + (M_PI / 2.0) * i / n;
        add(cx + r * std::cos(ang), cy + r * std::sin(ang));
      }
    };

    auto edge = [&](qreal x0, qreal x1, qreal y) {
      const qreal dx = x1 - x0;
      const int n = std::clamp(int(std::ceil(std::abs(dx) / 1.5)), 1, 400);
      for (int i = 0; i <= n; ++i)
        add(x0 + dx * i / n, y);
    };

    edge(a + rl, b - rr, -hh);
    arc(b - rr, -hh + rr, rr, -M_PI / 2.0);
    arc(b - rr, hh - rr, rr, 0.0);
    edge(b - rr, a + rl, hh);
    arc(a + rl, hh - rl, rl, M_PI / 2.0);
    arc(a + rl, -hh + rl, rl, M_PI);

    if (out.size() > 1 && (out.first() - out.last()).manhattanLength() < 1e-4)
      out.removeLast();
    return out;
  };

  const bool hasActive = activeEnd > 0.0;
  const bool hasInactive = inactiveStart < len;

  const QList<QPointF> activePoly =
      hasActive ? segment(0.0, activeEnd, outer, inner) : QList<QPointF>();
  const QList<QPointF> inactivePoly =
      hasInactive ? segment(inactiveStart, len, inner, outer)
                  : QList<QPointF>();

  if (activePoly.size() >= 3)
    m.fillPolygon(activePoly, m_color);
  if (inactivePoly.size() >= 3)
    m.fillPolygon(inactivePoly, m_trackColor);

  if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0) {
    const qreal bw = m.snapStroke(m_borderWidth);
    if (activePoly.size() >= 3)
      m.strokePolygon(activePoly, bw, m_borderColor);
    if (inactivePoly.size() >= 3)
      m.strokePolygon(inactivePoly, bw, m_borderColor);
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

    if (fits) {
      const QColor ic = faded(m_iconColor, fade);
      const QPointF c = P(iconCx, 0.0);
      const QRectF ir(c.x() - m_iconSize / 2.0, c.y() - m_iconSize / 2.0,
                      m_iconSize, m_iconSize);

      if (hasImage) {
        if (m_iconPath.isEmpty())
          m_iconPath = Ryu::resolveUrl(qmlContext(this), m_iconSource);
        const QString key = QStringLiteral("i|%1|%2|%3|%4")
                                .arg(m_iconPath, ic.name(QColor::HexArgb))
                                .arg(m_iconSize)
                                .arg(dpr);
        if (m_iconLayer.key != key) {
          m_iconLayer.key = key;
          m_iconLayer.img = m_iconCache.get(m_iconPath, m_iconSize, dpr, ic);
        }
        m_iconLayer.rect = ir;
        m_iconLayer.on = true;
      } else {
        const QString key =
            QStringLiteral("g|%1|%2|%3|%4|%5")
                .arg(m_iconText, m_iconFont.family(), ic.name(QColor::HexArgb))
                .arg(m_iconSize)
                .arg(dpr);
        if (m_iconLayer.key != key) {
          m_iconLayer.key = key;
          const int px = std::max(1, int(std::ceil(m_iconSize * dpr)));
          QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
          img.setDevicePixelRatio(dpr);
          img.fill(Qt::transparent);
          QPainter p(&img);
          p.setRenderHint(QPainter::Antialiasing, true);
          p.setRenderHint(QPainter::TextAntialiasing, true);
          Ryu::drawGlyph(&p, m_iconFont, m_iconText,
                         QPointF(m_iconSize / 2.0, m_iconSize / 2.0),
                         m_iconSize, ic);
          p.end();
          m_iconLayer.img = img;
        }
        m_iconLayer.rect = ir;
        m_iconLayer.on = true;
      }
    }
  }

  if (m_showStops && dotR > 0.0) {
    auto dot = [&](qreal x) {
      if (x <= activeEnd - dotR)
        m.fillCircle(P(x, 0.0), dotR, m_trackColor);
      else if (x >= inactiveStart + dotR)
        m.fillCircle(P(x, 0.0), dotR, m_color);
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

  const QRectF hr =
      R(hx - hw / 2.0, hx + hw / 2.0, -handleH / 2.0, handleH / 2.0);
  m.fillRoundedRect(hr, hw / 2.0, hw / 2.0, hw / 2.0, hw / 2.0, m_handleColor);

  if (m_showIndicator && m_press.x > 0.01) {
    const qreal s = std::clamp(m_press.x, 0.0, kMaxBubbleScale);
    const qreal half = kBubbleW / 2.0;
    const qreal bx = len >= kBubbleW ? std::clamp(hx, half, len - half) : hx;
    const qreal by = -(handleH / 2.0 + kBubbleGap + kBubbleH / 2.0);
    const QPointF bc = P(bx, by);
    const QSizeF full =
        hz ? QSizeF(kBubbleW, kBubbleH) : QSizeF(kBubbleH, kBubbleW);
    const QRectF br(bc.x() - full.width() * s / 2.0,
                    bc.y() - full.height() * s / 2.0, full.width() * s,
                    full.height() * s);
    const qreal rad = kBubbleRadius * s;
    m.fillRoundedRect(br, rad, rad, rad, rad, m_indicatorColor);

    const QColor tc = faded(m_indicatorTextColor, fade);
    const QString txt = valueText();
    const QString key = QStringLiteral("t|%1|%2|%3|%4|%5")
                            .arg(txt, tc.name(QColor::HexArgb))
                            .arg(dpr)
                            .arg(hz ? 1 : 0)
                            .arg(full.width());
    if (m_textLayer.key != key) {
      m_textLayer.key = key;
      const int pw = std::max(1, int(std::ceil(full.width() * dpr)));
      const int ph = std::max(1, int(std::ceil(full.height() * dpr)));
      QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
      img.setDevicePixelRatio(dpr);
      img.fill(Qt::transparent);
      QPainter p(&img);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      p.setFont(Ryu::styledFont(QFont(), 14.0, QFont::Medium));
      p.setPen(tc);
      p.drawText(QRectF(0.0, 0.0, full.width(), full.height()), Qt::AlignCenter,
                 txt);
      p.end();
      m_textLayer.img = img;
    }
    m_textLayer.rect = br;
    m_textLayer.on = true;
  }
}
