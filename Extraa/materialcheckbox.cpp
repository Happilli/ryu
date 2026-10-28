#include "./materialcheckbox.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QLineF>
#include <QList>
#include <QPainter>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kArea = 40.0;
constexpr qreal kGap = 4.0;
constexpr qreal kBox = 18.0;
constexpr qreal kRadius = 2.0;
constexpr qreal kPressRadius = 6.0;
constexpr qreal kPressShrink = 0.12;
constexpr qreal kStroke = 2.0;
constexpr qreal kMarkW = 2.0;
constexpr qreal kFontPx = 16.0;
constexpr qreal kDashHalf = 4.5;
constexpr qreal kPressStiffness = 1.4;
constexpr qreal kMarkStiffness = 0.85;
constexpr qreal kTextPad = 1.0;
const QPointF kCheckA(-4.5, 0.5);
const QPointF kCheckB(-1.5, 3.5);
const QPointF kCheckC(4.5, -3.5);
} // namespace

MaterialCheckbox::MaterialCheckbox(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.55) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

void MaterialCheckbox::componentComplete() {
  SgMaterialItem::componentComplete();
  retarget(true);
  update();
}

QFont MaterialCheckbox::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
}

qreal MaterialCheckbox::textWidth() const {
  if (m_text.isEmpty())
    return 0.0;
  return QFontMetricsF(labelFont()).horizontalAdvance(m_text);
}

void MaterialCheckbox::updateImplicit() {
  const qreal tw = textWidth();
  setImplicitWidth(std::ceil(kArea + (tw > 0.0 ? kGap + tw : 0.0)));
  setImplicitHeight(kArea);
}

void MaterialCheckbox::retarget(bool immediate) {
  m_fill.go(m_state == Unchecked ? 0.0 : 1.0, immediate);
  m_mark.go(m_state == Checked ? 1.0 : 0.0, immediate);
  m_dash.go(m_state == PartiallyChecked ? 1.0 : 0.0, immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialCheckbox::advance(qreal dt) {
  bool active = step(m_fill, dt);
  active |= step(m_mark, dt, 0.001, kMarkStiffness);
  active |= step(m_dash, dt, 0.001, kMarkStiffness);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  return active;
}

bool MaterialCheckbox::applyState(CheckState s, bool user) {
  if (m_state == s)
    return false;
  const bool wasChecked = m_state == Checked;
  m_state = s;
  emit checkStateChanged();
  if (wasChecked != (s == Checked))
    emit checkedChanged();
  retarget();
  update();
  if (user)
    emit toggled();
  return true;
}

void MaterialCheckbox::setChecked(bool v) {
  applyState(v ? Checked : Unchecked, false);
}

void MaterialCheckbox::setCheckState(CheckState v) { applyState(v, false); }

void MaterialCheckbox::setError(bool v) {
  assign(m_error, v, &MaterialCheckbox::errorChanged);
}

void MaterialCheckbox::setText(const QString &v) {
  if (assign(m_text, v, &MaterialCheckbox::textChanged))
    updateImplicit();
}

void MaterialCheckbox::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialCheckbox::fontChanged))
    updateImplicit();
}

void MaterialCheckbox::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialCheckbox::fontWeightChanged))
    updateImplicit();
}

void MaterialCheckbox::setColor(const QColor &v) {
  assign(m_color, v, &MaterialCheckbox::colorChanged);
}

void MaterialCheckbox::setUncheckedColor(const QColor &v) {
  assign(m_uncheckedColor, v, &MaterialCheckbox::uncheckedColorChanged);
}

void MaterialCheckbox::setCheckmarkColor(const QColor &v) {
  assign(m_checkmarkColor, v, &MaterialCheckbox::checkmarkColorChanged);
}

void MaterialCheckbox::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialCheckbox::textColorChanged);
}

void MaterialCheckbox::setErrorColor(const QColor &v) {
  assign(m_errorColor, v, &MaterialCheckbox::errorColorChanged);
}

void MaterialCheckbox::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialCheckbox::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialCheckbox::activate() {
  applyState(m_state == Checked ? Unchecked : Checked, true);
  emit clicked();
}

void MaterialCheckbox::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialCheckbox::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool inside = contains(e->position());
  setPressed(false);
  if (inside)
    activate();
  e->accept();
}

void MaterialCheckbox::mouseUngrabEvent() { setPressed(false); }

void MaterialCheckbox::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  if (isEnabled()) {
    setHovered(true);
    setHand(true);
  }
}

void MaterialCheckbox::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHovered(false);
  setHand(false);
}

void MaterialCheckbox::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || e->key() != Qt::Key_Space) {
    e->ignore();
    return;
  }
  if (!e->isAutoRepeat()) {
    m_keyFocus = true;
    activate();
  }
  e->accept();
}

void MaterialCheckbox::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

QSGNode *MaterialCheckbox::updatePaintNode(QSGNode *old,
                                           UpdatePaintNodeData *d) {
  if (!old) {
    m_textNode = nullptr;
    m_textLayer.shown.clear();
  }
  QSGNode *root = SgMaterialItem::updatePaintNode(old, d);
  syncLayer(root, m_textNode, m_textLayer);
  return root;
}

void MaterialCheckbox::syncLayer(QSGNode *root, QSGSimpleTextureNode *&node,
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

void MaterialCheckbox::build(Ryu::Mesh &m) {
  m_textLayer.on = false;

  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  const qreal dpr = m.dpr();
  const QPointF c(kArea / 2.0, h / 2.0);
  const qreal fill = std::clamp(m_fill.x, 0.0, 1.0);
  const qreal mark = std::clamp(m_mark.x, 0.0, 1.0);
  const qreal dash = std::clamp(m_dash.x, 0.0, 1.0);
  const qreal press = std::max(0.0, m_press.x);

  const QColor accent = m_error ? m_errorColor : m_color;
  const QColor outline = m_error ? m_errorColor : m_uncheckedColor;
  const QColor layerBase =
      Ryu::mix(m_error ? m_errorColor : m_textColor, accent, fill);

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  if (layer > 0.0)
    m.fillCircle(c, kArea / 2.0, Ryu::withAlpha(layerBase, layer));

  const qreal side = kBox * (1.0 - kPressShrink * std::min(press, 1.4));
  const qreal radius =
      kRadius + (kPressRadius - kRadius) * std::min(press, 1.0);
  const QRectF box(c.x() - side / 2.0, c.y() - side / 2.0, side, side);

  if (fill > 0.001)
    m.fillRoundedRect(box, radius, radius, radius, radius,
                      Ryu::withAlpha(accent, accent.alphaF() * fill));

  m.strokeRoundedRect(box, radius, radius, radius, radius,
                      m.snapStroke(kStroke), Ryu::mix(outline, accent, fill));

  const qreal k = side / kBox;

  if (mark > 0.01) {
    const qreal l1 = QLineF(kCheckA, kCheckB).length();
    const qreal l2 = QLineF(kCheckB, kCheckC).length();
    const qreal d = mark * (l1 + l2);

    auto map = [&](const QPointF &p) { return c + p * k; };

    QList<QPointF> pts;
    pts.append(map(kCheckA));
    if (d <= l1) {
      pts.append(map(kCheckA + (kCheckB - kCheckA) * (d / l1)));
    } else {
      pts.append(map(kCheckB));
      pts.append(map(kCheckB + (kCheckC - kCheckB) * ((d - l1) / l2)));
    }
    m.polyline(pts, kMarkW * k, m_checkmarkColor);
  }

  if (dash > 0.01) {
    const qreal half = kDashHalf * dash * k;
    m.line(QPointF(c.x() - half, c.y()), QPointF(c.x() + half, c.y()),
           kMarkW * k, m_checkmarkColor);
  }

  if (m_text.isEmpty())
    return;

  const qreal fade = isEnabled() ? 1.0 : Ryu::kDisabledOpacity;
  const QColor tc = Ryu::withAlpha(m_textColor, m_textColor.alphaF() * fade);
  const QFont lf = labelFont();
  const QFontMetricsF fm(lf);
  const qreal iw = std::ceil(textWidth()) + 2.0 * kTextPad;
  const qreal ih = std::ceil(fm.height()) + 2.0 * kTextPad;

  const QString key =
      QStringLiteral("t|%1|%2|%3|%4|%5")
          .arg(m_text, m_font.toString(), tc.name(QColor::HexArgb))
          .arg(m_fontWeight)
          .arg(dpr);
  if (m_textLayer.key != key) {
    m_textLayer.key = key;
    const int pw = std::max(1, int(std::ceil(iw * dpr)));
    const int ph = std::max(1, int(std::ceil(ih * dpr)));
    QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    Ryu::drawLabel(&p, lf, m_text, kTextPad, ih / 2.0, tc);
    p.end();
    m_textLayer.img = img;
  }

  m_textLayer.rect =
      QRectF(m.snap(kArea + kGap - kTextPad), m.snap(c.y() - ih / 2.0), iw, ih);
  m_textLayer.on = true;
}
