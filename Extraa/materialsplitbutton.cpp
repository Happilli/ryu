#include "./materialsplitbutton.hpp"
#include "../Core/paintutil.hpp"
#include "../Core/sizes.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPadOuter[] = {12.0, 16.0, 24.0, 48.0, 64.0};
constexpr qreal kPadInner[] = {10.0, 12.0, 24.0, 44.0, 60.0};
constexpr qreal kTrailW[] = {32.0, 40.0, 56.0, 96.0, 136.0};
constexpr qreal kChevron[] = {10.0, 12.0, 14.0, 20.0, 26.0};
constexpr qreal kInnerR[] = {6.0, 8.0, 8.0, 12.0, 16.0};
constexpr qreal kSegGap = 2.0;
} // namespace

MaterialSplitButton::MaterialSplitButton(QQuickItem *parent)
    : MaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

QFont MaterialSplitButton::labelFont() const {
  return Ryu::styledFont(m_font, Ryu::kBtnFont[m_sizeStyle], m_fontWeight);
}

qreal MaterialSplitButton::textWidth() const {
  if (m_label.isEmpty())
    return 0.0;
  return QFontMetricsF(labelFont()).horizontalAdvance(m_label);
}

qreal MaterialSplitButton::leadWidth() const {
  const int s = m_sizeStyle;
  const bool hasIcon = !m_iconText.isEmpty();
  const qreal tw = textWidth();
  qreal w = kPadOuter[s] + kPadInner[s] + tw;
  if (hasIcon)
    w += Ryu::kBtnIcon[s];
  if (hasIcon && tw > 0.0)
    w += Ryu::kBtnIconGap[s];
  return w;
}

QColor MaterialSplitButton::containerColor() const {
  if (m_color.isValid())
    return m_color;
  switch (m_type) {
  case Filled:
    return QColor("#6750A4");
  case Tonal:
    return QColor("#E8DEF8");
  case Elevated:
    return QColor("#F7F2FA");
  case Outlined:
    return QColor(Qt::transparent);
  }
  return QColor("#6750A4");
}

QColor MaterialSplitButton::foregroundColor() const {
  if (m_contentColor.isValid())
    return m_contentColor;
  switch (m_type) {
  case Filled:
    return QColor("#FFFFFF");
  case Tonal:
    return QColor("#1D192B");
  case Elevated:
    return QColor("#6750A4");
  case Outlined:
    return QColor("#49454F");
  }
  return QColor("#FFFFFF");
}

void MaterialSplitButton::updateImplicit() {
  setImplicitWidth(std::ceil(leadWidth() + kSegGap + kTrailW[m_sizeStyle]));
  setImplicitHeight(Ryu::kBtnHeight[m_sizeStyle]);
}

int MaterialSplitButton::hit(const QPointF &pt) const {
  const qreal h = Ryu::kBtnHeight[m_sizeStyle];
  const qreal y = std::round((height() - h) / 2.0);
  if (pt.y() < y || pt.y() > y + h)
    return None;
  const qreal lw = leadWidth();
  const qreal total = lw + kSegGap + kTrailW[m_sizeStyle];
  if (pt.x() < 0.0 || pt.x() > total)
    return None;
  return pt.x() < lw + kSegGap / 2.0 ? Lead : Trail;
}

void MaterialSplitButton::retarget(bool immediate) {
  m_roundLead.go(m_pressZone == Lead ? 1.0 : 0.0, immediate);
  m_roundTrail.go((m_pressZone == Trail || m_menuOpen) ? 1.0 : 0.0, immediate);
  m_chev.go(m_menuOpen ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialSplitButton::advance(qreal dt) {
  bool active = step(m_roundLead, dt);
  active |= step(m_roundTrail, dt);
  active |= step(m_chev, dt, 0.001, 0.8);
  return active;
}

void MaterialSplitButton::setLabel(const QString &v) {
  if (assign(m_label, v, &MaterialSplitButton::labelChanged))
    updateImplicit();
}

void MaterialSplitButton::setIconText(const QString &v) {
  if (assign(m_iconText, v, &MaterialSplitButton::iconTextChanged))
    updateImplicit();
}

void MaterialSplitButton::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSplitButton::iconFontChanged);
}

void MaterialSplitButton::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialSplitButton::fontChanged))
    updateImplicit();
}

void MaterialSplitButton::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialSplitButton::fontWeightChanged))
    updateImplicit();
}

void MaterialSplitButton::setType(Type v) {
  assign(m_type, v, &MaterialSplitButton::typeChanged);
}

void MaterialSplitButton::setSizeStyle(SizeStyle v) {
  if (assign(m_sizeStyle, v, &MaterialSplitButton::sizeStyleChanged))
    updateImplicit();
}

void MaterialSplitButton::setMenuOpen(bool v) {
  if (m_menuOpen == v)
    return;
  m_menuOpen = v;
  emit menuOpenChanged();
  retarget();
  update();
}

void MaterialSplitButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialSplitButton::colorChanged);
}

void MaterialSplitButton::setContentColor(const QColor &v) {
  assign(m_contentColor, v, &MaterialSplitButton::contentColorChanged);
}

void MaterialSplitButton::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSplitButton::borderColorChanged);
}

void MaterialSplitButton::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialSplitButton::borderWidthChanged);
}

void MaterialSplitButton::setPressZone(int z) {
  if (m_pressZone == z)
    return;
  m_pressZone = z;
  retarget();
  update();
}

void MaterialSplitButton::setHoverZone(int z) {
  if (m_hoverZone == z)
    return;
  m_hoverZone = z;
  update();
}

void MaterialSplitButton::activate(int zone) {
  if (zone == Lead) {
    emit clicked();
  } else if (zone == Trail) {
    m_menuOpen = !m_menuOpen;
    emit menuOpenChanged();
    retarget();
    emit menuClicked();
  }
  update();
}

void MaterialSplitButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  const int z = hit(e->position());
  if (z == None) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressZone(z);
  e->accept();
}

void MaterialSplitButton::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressZone == None)
    return;
  const int pressed = m_pressZone;
  const bool inside = hit(e->position()) == pressed;
  setPressZone(None);
  if (inside)
    activate(pressed);
  e->accept();
}

void MaterialSplitButton::mouseUngrabEvent() { setPressZone(None); }

void MaterialSplitButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  if (isEnabled())
    setHoverZone(hit(e->position()));
}

void MaterialSplitButton::hoverMoveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverMoveEvent(e);
  if (isEnabled())
    setHoverZone(hit(e->position()));
}

void MaterialSplitButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setHoverZone(None);
}

void MaterialSplitButton::keyPressEvent(QKeyEvent *e) {
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
      activate(Lead);
    }
    break;
  case Qt::Key_Down:
  case Qt::Key_Up:
    m_keyFocus = true;
    activate(Trail);
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialSplitButton::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialSplitButton::paint(QPainter *p) {
  preparePainter(p);

  const int s = m_sizeStyle;
  const qreal h = Ryu::kBtnHeight[s];
  const qreal full = h / 2.0;
  const qreal inner = kInnerR[s];
  const qreal y = std::round((height() - h) / 2.0);
  const qreal lw = leadWidth();
  const qreal tw = kTrailW[s];

  const QRectF leadRect(0.0, y, lw, h);
  const QRectF trailRect(lw + kSegGap, y, tw, h);

  const qreal tL = std::clamp(m_roundLead.x, 0.0, 1.0);
  const qreal tT = std::clamp(m_roundTrail.x, 0.0, 1.0);

  const QPainterPath leadPath =
      Ryu::sidedRect(leadRect, full, inner + (full - inner) * tL);
  const QPainterPath trailPath =
      Ryu::sidedRect(trailRect, inner + (full - inner) * tT, full);

  const QColor bg = containerColor();
  const QColor fg = foregroundColor();

  auto drawSegment = [&](const QPainterPath &path, int zone) {
    p->setPen(Qt::NoPen);
    if (bg.alpha() > 0) {
      p->setBrush(bg);
      p->drawPath(path);
    }

    if (m_type == Outlined && m_borderWidth > 0.0 && m_borderColor.alpha() > 0)
      Ryu::strokeInside(p, path, m_borderColor, m_borderWidth);

    const qreal layer =
        Ryu::stateLayer(m_hoverZone == zone,
                        m_pressZone == zone ||
                            (m_keyFocus && hasActiveFocus() && zone == Lead));
    if (layer > 0.0) {
      p->setBrush(Ryu::withAlpha(fg, layer));
      p->drawPath(path);
    }
  };

  drawSegment(leadPath, Lead);
  drawSegment(trailPath, Trail);

  const qreal iconSz = Ryu::kBtnIcon[s];
  const qreal cy = leadRect.center().y();
  qreal x = leadRect.left() + kPadOuter[s];

  if (!m_iconText.isEmpty()) {
    Ryu::drawGlyph(p, m_iconFont, m_iconText, QPointF(x + iconSz / 2.0, cy),
                   iconSz, fg);
    x += iconSz;
    if (!m_label.isEmpty())
      x += Ryu::kBtnIconGap[s];
  }

  Ryu::drawLabel(p, labelFont(), m_label, x, cy, fg);

  const qreal cw = kChevron[s] / 2.0;
  p->save();
  p->translate(trailRect.center());
  p->rotate(180.0 * m_chev.x);
  QPainterPath chev;
  chev.moveTo(-cw, -cw / 2.0);
  chev.lineTo(0.0, cw / 2.0);
  chev.lineTo(cw, -cw / 2.0);
  p->setBrush(Qt::NoBrush);
  p->setPen(QPen(fg, std::max(1.5, iconSz / 10.0), Qt::SolidLine, Qt::RoundCap,
                 Qt::RoundJoin));
  p->drawPath(chev);
  p->restore();
}
