#include "./materialradiobutton.hpp"
#include "../Core/paintutil.hpp"
#include <QCursor>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kArea = 40.0;
constexpr qreal kGap = 4.0;
constexpr qreal kRing = 20.0;
constexpr qreal kStroke = 2.0;
constexpr qreal kDot = 10.0;
constexpr qreal kDotPress = 1.5;
constexpr qreal kPressShrink = 0.06;
constexpr qreal kPreview = 0.3;
constexpr qreal kFontPx = 16.0;
constexpr qreal kPressStiffness = 1.4;
constexpr qreal kSelStiffness = 0.8;
} // namespace

MaterialRadioButton::MaterialRadioButton(QQuickItem *parent)
    : MaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}
MaterialRadioButton::~MaterialRadioButton() { setHand(false); }

void MaterialRadioButton::setHand(bool on) {
  if (on == m_handOn)
    return;
  m_handOn = on;
  if (on)
    QGuiApplication::setOverrideCursor(QCursor(Qt::PointingHandCursor));
  else
    QGuiApplication::restoreOverrideCursor();
}

void MaterialRadioButton::componentComplete() {
  MaterialItem::componentComplete();
  if (m_checked)
    uncheckPeers();
  retarget(true);
  update();
}
bool MaterialRadioButton::overCircle(const QPointF &pt) const {
  return std::hypot(pt.x() - kArea / 2.0, pt.y() - height() / 2.0) <=
         kArea / 2.0;
}
QFont MaterialRadioButton::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
}

qreal MaterialRadioButton::textWidth() const {
  if (m_text.isEmpty())
    return 0.0;
  return QFontMetricsF(labelFont()).horizontalAdvance(m_text);
}

qreal MaterialRadioButton::targetSelection() const {
  if (m_checked)
    return 1.0;
  return m_pressed ? kPreview : 0.0;
}

void MaterialRadioButton::updateImplicit() {
  const qreal tw = textWidth();
  setImplicitWidth(std::ceil(kArea + (tw > 0.0 ? kGap + tw : 0.0)));
  setImplicitHeight(kArea);
}

void MaterialRadioButton::retarget(bool immediate) {
  m_sel.go(targetSelection(), immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialRadioButton::advance(qreal dt) {
  bool active = step(m_sel, dt, 0.001, kSelStiffness);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  return active;
}

QList<MaterialRadioButton *> MaterialRadioButton::peers() const {
  QList<MaterialRadioButton *> out;
  QQuickItem *par = parentItem();
  if (!par) {
    out.append(const_cast<MaterialRadioButton *>(this));
    return out;
  }
  const auto kids = par->childItems();
  for (QQuickItem *it : kids)
    if (auto *r = qobject_cast<MaterialRadioButton *>(it))
      if (r->m_group == m_group)
        out.append(r);
  return out;
}

void MaterialRadioButton::uncheckPeers() {
  const auto list = peers();
  for (MaterialRadioButton *r : list)
    if (r != this && r->m_checked)
      r->applyChecked(false, false);
}

bool MaterialRadioButton::applyChecked(bool v, bool user) {
  if (m_checked == v)
    return false;
  m_checked = v;
  if (v && isComponentComplete())
    uncheckPeers();
  emit checkedChanged();
  retarget();
  update();
  if (user)
    emit toggled();
  return true;
}

void MaterialRadioButton::setChecked(bool v) { applyChecked(v, false); }

void MaterialRadioButton::setGroup(const QString &v) {
  if (assign(m_group, v, &MaterialRadioButton::groupChanged) && m_checked &&
      isComponentComplete())
    uncheckPeers();
}

void MaterialRadioButton::setError(bool v) {
  assign(m_error, v, &MaterialRadioButton::errorChanged);
}

void MaterialRadioButton::setText(const QString &v) {
  if (assign(m_text, v, &MaterialRadioButton::textChanged))
    updateImplicit();
}

void MaterialRadioButton::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialRadioButton::fontChanged))
    updateImplicit();
}

void MaterialRadioButton::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialRadioButton::fontWeightChanged))
    updateImplicit();
}

void MaterialRadioButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialRadioButton::colorChanged);
}

void MaterialRadioButton::setUncheckedColor(const QColor &v) {
  assign(m_uncheckedColor, v, &MaterialRadioButton::uncheckedColorChanged);
}

void MaterialRadioButton::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialRadioButton::textColorChanged);
}

void MaterialRadioButton::setErrorColor(const QColor &v) {
  assign(m_errorColor, v, &MaterialRadioButton::errorColorChanged);
}

void MaterialRadioButton::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialRadioButton::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialRadioButton::activate() {
  applyChecked(true, true);
  emit clicked();
}

void MaterialRadioButton::moveFocus(int dir) {
  QList<MaterialRadioButton *> list;
  const auto all = peers();
  for (MaterialRadioButton *r : all)
    if (r->isEnabled() && r->isVisible())
      list.append(r);
  const int n = list.size();
  const int idx = list.indexOf(this);
  if (n < 2 || idx < 0)
    return;
  MaterialRadioButton *next = list[(idx + dir + n) % n];
  next->m_keyFocus = true;
  next->forceActiveFocus(Qt::TabFocusReason);
  next->activate();
}
void MaterialRadioButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled() || !overCircle(e->position())) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialRadioButton::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool inside = overCircle(e->position());
  setPressed(false);
  if (inside)
    activate();
  e->accept();
}

void MaterialRadioButton::mouseUngrabEvent() { setPressed(false); }

void MaterialRadioButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialRadioButton::hoverMoveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverMoveEvent(e);
  if (!isEnabled())
    return;
  const bool over = overCircle(e->position());
  setHovered(over);
  setHand(over);
}

void MaterialRadioButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setHovered(false);
  setHand(false);
}

void MaterialRadioButton::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Space:
    if (!e->isAutoRepeat()) {
      m_keyFocus = true;
      activate();
    }
    break;
  case Qt::Key_Left:
  case Qt::Key_Up:
    moveFocus(-1);
    break;
  case Qt::Key_Right:
  case Qt::Key_Down:
    moveFocus(1);
    break;
  default:
    e->ignore();
    return;
  }
  update();
  e->accept();
}

void MaterialRadioButton::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialRadioButton::paint(QPainter *p) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  preparePainter(p);

  const QPointF c(kArea / 2.0, h / 2.0);
  const qreal sel = std::max(0.0, m_sel.x);
  const qreal selC = std::min(sel, 1.0);
  const qreal press = std::max(0.0, m_press.x);

  const QColor accent = m_error ? m_errorColor : m_color;
  const QColor outline = m_error ? m_errorColor : m_uncheckedColor;
  const QColor layerBase =
      Ryu::mix(m_error ? m_errorColor : m_textColor, accent, selC);

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  p->setPen(Qt::NoPen);
  if (layer > 0.0) {
    p->setBrush(Ryu::withAlpha(layerBase, layer));
    p->drawEllipse(c, kArea / 2.0, kArea / 2.0);
  }

  const qreal ringR =
      kRing / 2.0 * (1.0 - kPressShrink * std::min(press, 1.4)) - kStroke / 2.0;
  p->setBrush(Qt::NoBrush);
  p->setPen(QPen(Ryu::mix(outline, accent, selC), kStroke));
  p->drawEllipse(c, ringR, ringR);

  const qreal dotR = (kDot / 2.0 + kDotPress * std::min(press, 1.4)) * sel;
  if (dotR > 0.05) {
    p->setPen(Qt::NoPen);
    p->setBrush(accent);
    p->drawEllipse(c, dotR, dotR);
  }

  p->setPen(Qt::NoPen);
  Ryu::drawLabel(p, labelFont(), m_text, kArea + kGap, c.y(), m_textColor);
}
