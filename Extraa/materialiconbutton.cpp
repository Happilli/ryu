#include "./materialiconbutton.hpp"
#include "../Core/sizes.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQmlEngine>
#include <QQuickWindow>
#include <algorithm>

namespace {
constexpr qreal kWidth[3][5] = {{32.0, 40.0, 56.0, 96.0, 136.0},
                                {28.0, 32.0, 48.0, 64.0, 104.0},
                                {40.0, 52.0, 72.0, 128.0, 184.0}};
constexpr qreal kBorder[] = {1.0, 1.0, 1.0, 2.0, 3.0};
constexpr qreal kPressShrink = 0.06;
} // namespace

MaterialIconButton::MaterialIconButton(QQuickItem *parent)
    : MaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

QColor MaterialIconButton::containerColor() const {
  if (m_checked && m_checkedColor.isValid())
    return m_checkedColor;
  if (!m_checked && m_color.isValid())
    return m_color;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#6750A4") : QColor("#E6E0E9");
  case Tonal:
    return m_checked ? QColor("#625B71") : QColor("#E8DEF8");
  case Outlined:
    return m_checked ? QColor("#322F35") : QColor(Qt::transparent);
  case Standard:
    return QColor(Qt::transparent);
  }
  return QColor(Qt::transparent);
}

QColor MaterialIconButton::contentColor() const {
  if (m_checked && m_checkedIconColor.isValid())
    return m_checkedIconColor;
  if (!m_checked && m_iconColor.isValid())
    return m_iconColor;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#FFFFFF") : QColor("#6750A4");
  case Tonal:
    return m_checked ? QColor("#FFFFFF") : QColor("#1D192B");
  case Outlined:
    return m_checked ? QColor("#F5EFF7") : QColor("#49454F");
  case Standard:
    return m_checked ? QColor("#6750A4") : QColor("#49454F");
  }
  return QColor("#49454F");
}

qreal MaterialIconButton::resolvedIconSize() const {
  return m_iconSize >= 0.0 ? m_iconSize : Ryu::kBtnIcon[m_sizeStyle];
}

qreal MaterialIconButton::resolvedBorderWidth() const {
  return m_borderWidth >= 0.0 ? m_borderWidth : kBorder[m_sizeStyle];
}

qreal MaterialIconButton::targetRound() const {
  if (m_pressed)
    return 0.0;
  qreal base = m_shape == Round ? 1.0 : 0.0;
  if (m_checkable && m_checked)
    base = 1.0 - base;
  return base;
}

void MaterialIconButton::updateImplicit() {
  setImplicitWidth(kWidth[m_widthStyle][m_sizeStyle]);
  setImplicitHeight(Ryu::kBtnHeight[m_sizeStyle]);
}

void MaterialIconButton::retarget(bool immediate) {
  m_round.go(targetRound(), immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialIconButton::advance(qreal dt) {
  bool active = step(m_round, dt);
  active |= step(m_press, dt, 0.001, 1.4);
  return active;
}

void MaterialIconButton::setType(Type v) {
  assign(m_type, v, &MaterialIconButton::typeChanged);
}

void MaterialIconButton::setSizeStyle(SizeStyle v) {
  if (assign(m_sizeStyle, v, &MaterialIconButton::sizeStyleChanged))
    updateImplicit();
}

void MaterialIconButton::setWidthStyle(WidthStyle v) {
  if (assign(m_widthStyle, v, &MaterialIconButton::widthStyleChanged))
    updateImplicit();
}

void MaterialIconButton::setShape(Shape v) {
  if (assign(m_shape, v, &MaterialIconButton::shapeChanged))
    retarget();
}

void MaterialIconButton::setCheckable(bool v) {
  if (assign(m_checkable, v, &MaterialIconButton::checkableChanged))
    retarget();
}

void MaterialIconButton::setChecked(bool v) {
  if (assign(m_checked, v, &MaterialIconButton::checkedChanged))
    retarget();
}

void MaterialIconButton::setIconSource(const QUrl &v) {
  if (assign(m_iconSource, v, &MaterialIconButton::iconSourceChanged))
    m_iconPath.clear();
}

void MaterialIconButton::setIconText(const QString &v) {
  assign(m_iconText, v, &MaterialIconButton::iconTextChanged);
}

void MaterialIconButton::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialIconButton::iconFontChanged);
}

void MaterialIconButton::setIconSize(qreal v) {
  if (v < 0.0)
    v = -1.0;
  assign(m_iconSize, v, &MaterialIconButton::iconSizeChanged);
}

void MaterialIconButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialIconButton::colorChanged);
}

void MaterialIconButton::setCheckedColor(const QColor &v) {
  assign(m_checkedColor, v, &MaterialIconButton::checkedColorChanged);
}

void MaterialIconButton::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialIconButton::iconColorChanged);
}

void MaterialIconButton::setCheckedIconColor(const QColor &v) {
  assign(m_checkedIconColor, v, &MaterialIconButton::checkedIconColorChanged);
}

void MaterialIconButton::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialIconButton::borderColorChanged);
}

void MaterialIconButton::setBorderWidth(qreal v) {
  if (v < 0.0)
    v = -1.0;
  assign(m_borderWidth, v, &MaterialIconButton::borderWidthChanged);
}

void MaterialIconButton::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialIconButton::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialIconButton::activate() {
  if (m_checkable) {
    m_checked = !m_checked;
    emit checkedChanged();
    emit toggled();
    retarget();
  }
  emit clicked();
  update();
}

void MaterialIconButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialIconButton::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool inside = contains(e->position());
  setPressed(false);
  if (inside)
    activate();
  e->accept();
}

void MaterialIconButton::mouseUngrabEvent() { setPressed(false); }

void MaterialIconButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  if (isEnabled())
    setHovered(true);
}

void MaterialIconButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setHovered(false);
}

void MaterialIconButton::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (!e->isAutoRepeat()) {
      m_keyFocus = true;
      activate();
    }
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialIconButton::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialIconButton::paint(QPainter *p) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  preparePainter(p);

  const qreal scale = 1.0 - kPressShrink * m_press.x;
  p->translate(w / 2.0, h / 2.0);
  p->scale(scale, scale);
  p->translate(-w / 2.0, -h / 2.0);

  const qreal full = std::min(w, h) / 2.0;
  const qreal sq = std::min(Ryu::kBtnSquareR[m_sizeStyle], full);
  const qreal radius = std::max(0.0, sq + (full - sq) * m_round.x);
  const QRectF rect(0.0, 0.0, w, h);

  QPainterPath path;
  path.addRoundedRect(rect, radius, radius);

  const QColor bg = containerColor();
  const QColor fg = contentColor();

  p->setPen(Qt::NoPen);
  if (bg.alpha() > 0) {
    p->setBrush(bg);
    p->drawPath(path);
  }

  const qreal bw = resolvedBorderWidth();
  if (m_type == Outlined && !m_checked && bw > 0.0 &&
      m_borderColor.alpha() > 0) {
    const qreal half = bw / 2.0;
    const qreal br = std::max(0.0, radius - half);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(m_borderColor, bw));
    p->drawRoundedRect(rect.adjusted(half, half, -half, -half), br, br);
    p->setPen(Qt::NoPen);
  }

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  if (layer > 0.0) {
    p->setBrush(Ryu::withAlpha(fg, layer));
    p->drawPath(path);
  }

  const qreal iconSz = resolvedIconSize();
  if (iconSz <= 0.0)
    return;

  if (m_iconSource.isValid() && !m_iconSource.isEmpty()) {
    if (m_iconPath.isEmpty())
      m_iconPath = Ryu::resolveUrl(qmlContext(this), m_iconSource);
    const QImage &img = m_iconCache.get(
        m_iconPath, iconSz, window() ? window()->devicePixelRatio() : 1.0, fg);
    if (!img.isNull())
      p->drawImage(QPointF(w / 2.0 - iconSz / 2.0, h / 2.0 - iconSz / 2.0),
                   img);
  }

  if (!m_iconText.isEmpty())
    Ryu::drawGlyph(p, m_iconFont, m_iconText, QPointF(w / 2.0, h / 2.0), iconSz,
                   fg);
}
