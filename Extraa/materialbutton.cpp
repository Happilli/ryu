#include "./materialbutton.hpp"
#include "../Core/paintutil.hpp"
#include "../Core/sizes.hpp"
#include <QFontMetricsF>
#include <QQmlEngine>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPad[] = {12.0, 16.0, 24.0, 48.0, 64.0};
constexpr qreal kBorder[] = {1.0, 1.0, 1.0, 2.0, 3.0};
constexpr qreal kPressShrink = 0.04;
constexpr qreal kPressStiffness = 1.4;
constexpr int kSlotIcon = 0;
constexpr int kSlotText = 1;
} // namespace

MaterialButton::MaterialButton(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

void MaterialButton::componentComplete() {
  SgMaterialItem::componentComplete();
  updateImplicit();
  retarget(true);
  update();
}

QFont MaterialButton::labelFont() const {
  return Ryu::styledFont(m_font, Ryu::kBtnFont[m_sizeStyle], m_fontWeight);
}

bool MaterialButton::hasIcon() const {
  return !m_iconText.isEmpty() ||
         (m_iconSource.isValid() && !m_iconSource.isEmpty());
}

qreal MaterialButton::textWidth() const {
  if (m_label.isEmpty())
    return 0.0;
  return QFontMetricsF(labelFont()).horizontalAdvance(m_label);
}

qreal MaterialButton::resolvedIconSize() const {
  return m_iconSize >= 0.0 ? m_iconSize : Ryu::kBtnIcon[m_sizeStyle];
}

qreal MaterialButton::resolvedPadding() const {
  return m_padX >= 0.0 ? m_padX : kPad[m_sizeStyle];
}

qreal MaterialButton::resolvedIconGap() const {
  return m_iconGap >= 0.0 ? m_iconGap : Ryu::kBtnIconGap[m_sizeStyle];
}

qreal MaterialButton::resolvedBorderWidth() const {
  return m_borderWidth >= 0.0 ? m_borderWidth : kBorder[m_sizeStyle];
}

QColor MaterialButton::containerColor() const {
  if (m_checked && m_checkedColor.isValid())
    return m_checkedColor;
  if (!m_checked && m_color.isValid())
    return m_color;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#6750A4") : QColor("#E6E0E9");
  case Tonal:
    return m_checked ? QColor("#625B71") : QColor("#E8DEF8");
  case Elevated:
    return m_checked ? QColor("#6750A4") : QColor("#F7F2FA");
  case Outlined:
    return m_checked ? QColor("#322F35") : QColor(Qt::transparent);
  case Text:
    return m_checked ? QColor("#E8DEF8") : QColor(Qt::transparent);
  }
  return QColor(Qt::transparent);
}

QColor MaterialButton::foregroundColor() const {
  if (m_checked && m_checkedContentColor.isValid())
    return m_checkedContentColor;
  if (!m_checked && m_contentColor.isValid())
    return m_contentColor;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#FFFFFF") : QColor("#6750A4");
  case Tonal:
    return m_checked ? QColor("#FFFFFF") : QColor("#1D192B");
  case Elevated:
    return m_checked ? QColor("#FFFFFF") : QColor("#6750A4");
  case Outlined:
    return m_checked ? QColor("#F5EFF7") : QColor("#49454F");
  case Text:
    return m_checked ? QColor("#1D192B") : QColor("#6750A4");
  }
  return QColor("#6750A4");
}

qreal MaterialButton::targetRound() const {
  if (m_pressed)
    return 0.0;
  qreal base = m_shape == Round ? 1.0 : 0.0;
  if (m_checkable && m_checked)
    base = 1.0 - base;
  return base;
}

void MaterialButton::updateImplicit() {
  m_textW = textWidth();
  const qreal h = Ryu::kBtnHeight[m_sizeStyle];
  qreal w;
  if (m_label.isEmpty() && hasIcon()) {
    w = h;
  } else {
    w = 2.0 * resolvedPadding() + m_textW;
    if (hasIcon()) {
      w += resolvedIconSize();
      if (m_textW > 0.0)
        w += resolvedIconGap();
    }
  }
  setImplicitWidth(std::ceil(w));
  setImplicitHeight(h);
}

void MaterialButton::retarget(bool immediate) {
  m_round.go(targetRound(), immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialButton::advance(qreal dt) {
  bool active = step(m_round, dt);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  return active;
}

void MaterialButton::setLabel(const QString &v) {
  if (assign(m_label, v, &MaterialButton::labelChanged))
    updateImplicit();
}

void MaterialButton::setIconText(const QString &v) {
  if (assign(m_iconText, v, &MaterialButton::iconTextChanged))
    updateImplicit();
}

void MaterialButton::setIconSource(const QUrl &v) {
  if (assign(m_iconSource, v, &MaterialButton::iconSourceChanged)) {
    m_iconPath.clear();
    m_iconCache.invalidate();
    updateImplicit();
  }
}

void MaterialButton::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialButton::iconFontChanged);
}

void MaterialButton::setIconSize(qreal v) {
  if (v < 0.0)
    v = -1.0;
  if (assign(m_iconSize, v, &MaterialButton::iconSizeChanged))
    updateImplicit();
}

void MaterialButton::setIconPosition(IconPosition v) {
  assign(m_iconPosition, v, &MaterialButton::iconPositionChanged);
}

void MaterialButton::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialButton::fontChanged))
    updateImplicit();
}

void MaterialButton::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialButton::fontWeightChanged))
    updateImplicit();
}

void MaterialButton::setType(Type v) {
  assign(m_type, v, &MaterialButton::typeChanged);
}

void MaterialButton::setSizeStyle(SizeStyle v) {
  if (assign(m_sizeStyle, v, &MaterialButton::sizeStyleChanged))
    updateImplicit();
}

void MaterialButton::setShape(Shape v) {
  if (assign(m_shape, v, &MaterialButton::shapeChanged))
    retarget();
}

void MaterialButton::setCheckable(bool v) {
  if (assign(m_checkable, v, &MaterialButton::checkableChanged))
    retarget();
}

void MaterialButton::setChecked(bool v) {
  if (assign(m_checked, v, &MaterialButton::checkedChanged))
    retarget();
}

void MaterialButton::setHorizontalPadding(qreal v) {
  if (v < 0.0)
    v = -1.0;
  if (assign(m_padX, v, &MaterialButton::horizontalPaddingChanged))
    updateImplicit();
}

void MaterialButton::setIconSpacing(qreal v) {
  if (v < 0.0)
    v = -1.0;
  if (assign(m_iconGap, v, &MaterialButton::iconSpacingChanged))
    updateImplicit();
}

void MaterialButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialButton::colorChanged);
}

void MaterialButton::setCheckedColor(const QColor &v) {
  assign(m_checkedColor, v, &MaterialButton::checkedColorChanged);
}

void MaterialButton::setContentColor(const QColor &v) {
  assign(m_contentColor, v, &MaterialButton::contentColorChanged);
}

void MaterialButton::setCheckedContentColor(const QColor &v) {
  assign(m_checkedContentColor, v, &MaterialButton::checkedContentColorChanged);
}

void MaterialButton::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialButton::borderColorChanged);
}

void MaterialButton::setBorderWidth(qreal v) {
  if (v < 0.0)
    v = -1.0;
  assign(m_borderWidth, v, &MaterialButton::borderWidthChanged);
}

void MaterialButton::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialButton::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialButton::activate() {
  if (m_checkable) {
    m_checked = !m_checked;
    emit checkedChanged();
    emit toggled();
    retarget();
  }
  emit clicked();
  update();
}

void MaterialButton::click() {
  if (!isEnabled())
    return;
  activate();
}

void MaterialButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialButton::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed || m_keyDown)
    return;
  const bool inside = contains(e->position());
  setPressed(false);
  if (inside)
    activate();
  e->accept();
}

void MaterialButton::mouseUngrabEvent() {
  if (!m_keyDown)
    setPressed(false);
}

void MaterialButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  if (isEnabled()) {
    setHovered(true);
    setHand(true);
  }
}

void MaterialButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHovered(false);
  setHand(false);
}

void MaterialButton::keyPressEvent(QKeyEvent *e) {
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
      m_keyDown = true;
      setPressed(true);
    }
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialButton::keyReleaseEvent(QKeyEvent *e) {
  if (!m_keyDown || e->isAutoRepeat()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter:
    m_keyDown = false;
    setPressed(false);
    activate();
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialButton::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  if (m_keyDown) {
    m_keyDown = false;
    setPressed(false);
  }
  update();
}

void MaterialButton::build(Ryu::Mesh &m) {
  const qreal w = width();
  const qreal ih = height();
  if (w <= 0.0 || ih <= 0.0)
    return;

  const int s = m_sizeStyle;
  const qreal h = Ryu::kBtnHeight[s];
  const qreal y = std::round((ih - h) / 2.0);
  const qreal press = std::max(0.0, m_press.x);
  const qreal scale = 1.0 - kPressShrink * std::min(press, 1.4);
  const QPointF c(w / 2.0, y + h / 2.0);
  const QRectF rect(c.x() - w * scale / 2.0, c.y() - h * scale / 2.0, w * scale,
                    h * scale);

  const qreal full = h / 2.0;
  const qreal sq = std::min(Ryu::kBtnSquareR[s], full);
  const qreal t = std::clamp(m_round.x, 0.0, 1.0);
  const qreal radius = std::max(0.0, sq + (full - sq) * t) * scale;

  const QColor bg = containerColor();
  const QColor fg = foregroundColor();

  if (m_type == Elevated && bg.alpha() > 0) {
    m.fillRoundedRect(rect.translated(0.0, 2.0), radius, radius, radius, radius,
                      QColor(0, 0, 0, 22));
    m.fillRoundedRect(rect.translated(0.0, 1.0), radius, radius, radius, radius,
                      QColor(0, 0, 0, 30));
  }

  if (bg.alpha() > 0)
    m.fillRoundedRect(rect, radius, radius, radius, radius, bg);

  const qreal bw = resolvedBorderWidth();
  if (m_type == Outlined && !m_checked && bw > 0.0 && m_borderColor.alpha() > 0)
    m.strokeRoundedRect(rect, radius, radius, radius, radius, m.snapStroke(bw),
                        m_borderColor);

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  if (layer > 0.0)
    m.fillRoundedRect(rect, radius, radius, radius, radius,
                      Ryu::withAlpha(fg, layer));

  const bool icon = hasIcon();
  const qreal iconSz = resolvedIconSize();
  const qreal tw = m_textW;
  const qreal gap = (icon && tw > 0.0) ? resolvedIconGap() : 0.0;
  const qreal iconW = icon ? iconSz : 0.0;
  const qreal contentW = iconW + gap + tw;
  const qreal x0 = c.x() - contentW / 2.0;

  const qreal iconX = m_iconPosition == Leading ? x0 : x0 + tw + gap;
  const qreal textX = m_iconPosition == Leading ? x0 + iconW + gap : x0;

  if (icon && iconSz > 0.0) {
    const QPointF ic(iconX + iconSz / 2.0, c.y());
    if (m_iconSource.isValid() && !m_iconSource.isEmpty()) {
      if (m_iconPath.isEmpty())
        m_iconPath = Ryu::resolveUrl(qmlContext(this), m_iconSource);
      const qreal dpr = m.dpr();
      const QColor col = Ryu::withAlpha(fg, fg.alphaF() * fade());
      const QString key = QStringLiteral("i|%1|%2|%3|%4")
                              .arg(m_iconPath, col.name(QColor::HexArgb))
                              .arg(iconSz)
                              .arg(dpr);
      useLayer(kSlotIcon, key,
               QRectF(m.snap(ic.x() - iconSz / 2.0),
                      m.snap(ic.y() - iconSz / 2.0), iconSz, iconSz),
               [&] { return m_iconCache.get(m_iconPath, iconSz, dpr, col); });
    } else {
      glyphLayer(kSlotIcon, m_iconFont, m_iconText, fg, ic, iconSz);
    }
  }

  textLayer(kSlotText, labelFont(), m_label, fg, textX, c.y());
}
