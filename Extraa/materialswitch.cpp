#include "./materialswitch.hpp"
#include "../Core/paintutil.hpp"
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kTrackW = 52.0;
constexpr qreal kTrackH = 32.0;
constexpr qreal kBorder = 2.0;
constexpr qreal kOffX = 16.0;
constexpr qreal kOnX = kTrackW - 16.0;
constexpr qreal kOffD = 16.0;
constexpr qreal kOffIconD = 24.0;
constexpr qreal kOnD = 24.0;
constexpr qreal kPressD = 28.0;
constexpr qreal kLayerD = 40.0;
constexpr qreal kIconS = 4.0;
constexpr qreal kIconW = 2.0;
constexpr qreal kDragSlop = 3.0;
constexpr qreal kPressStiffness = 1.4;
} // namespace

MaterialSwitch::MaterialSwitch(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.55) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setImplicitWidth(kTrackW);
  setImplicitHeight(kTrackH);
  retarget(true);
}

bool MaterialSwitch::overTrack(const QPointF &pt) const {
  const QRectF track((width() - kTrackW) / 2.0, (height() - kTrackH) / 2.0,
                     kTrackW, kTrackH);
  return track.contains(pt);
}

void MaterialSwitch::componentComplete() {
  SgMaterialItem::componentComplete();
  retarget(true);
  update();
}

void MaterialSwitch::retarget(bool immediate) {
  if (!m_dragging)
    m_pos.go(m_checked ? 1.0 : 0.0, immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialSwitch::advance(qreal dt) {
  bool active = m_dragging;
  if (!m_dragging)
    active |= step(m_pos, dt);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  return active;
}

void MaterialSwitch::setChecked(bool v) {
  if (m_checked == v)
    return;
  m_checked = v;
  emit checkedChanged();
  retarget();
  update();
}

void MaterialSwitch::userSet(bool on) {
  if (m_checked != on) {
    m_checked = on;
    emit checkedChanged();
    emit toggled();
  }
  retarget();
  update();
}

void MaterialSwitch::setShowIcons(bool v) {
  assign(m_showIcons, v, &MaterialSwitch::showIconsChanged);
}

void MaterialSwitch::setTrackColor(const QColor &v) {
  assign(m_trackColor, v, &MaterialSwitch::trackColorChanged);
}

void MaterialSwitch::setCheckedTrackColor(const QColor &v) {
  assign(m_checkedTrackColor, v, &MaterialSwitch::checkedTrackColorChanged);
}

void MaterialSwitch::setHandleColor(const QColor &v) {
  assign(m_handleColor, v, &MaterialSwitch::handleColorChanged);
}

void MaterialSwitch::setCheckedHandleColor(const QColor &v) {
  assign(m_checkedHandleColor, v, &MaterialSwitch::checkedHandleColorChanged);
}

void MaterialSwitch::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSwitch::borderColorChanged);
}

void MaterialSwitch::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialSwitch::iconColorChanged);
}

void MaterialSwitch::setCheckedIconColor(const QColor &v) {
  assign(m_checkedIconColor, v, &MaterialSwitch::checkedIconColorChanged);
}

void MaterialSwitch::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialSwitch::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialSwitch::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled() || !overTrack(e->position())) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  m_dragging = false;
  m_pressX = e->position().x();
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialSwitch::mouseMoveEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const qreal x = e->position().x();
  if (!m_dragging && std::abs(x - m_pressX) > kDragSlop) {
    m_dragging = true;
    setKeepMouseGrab(true);
    animate();
  }
  if (m_dragging) {
    const qreal off = (width() - kTrackW) / 2.0;
    m_pos.x = std::clamp((x - off - kOffX) / (kOnX - kOffX), 0.0, 1.0);
    m_pos.v = 0.0;
    update();
  }
  e->accept();
}

void MaterialSwitch::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool wasDragging = m_dragging;
  const bool inside = contains(e->position());
  m_dragging = false;
  setKeepMouseGrab(false);
  setPressed(false);

  if (wasDragging) {
    userSet(m_pos.x > 0.5);
  } else if (inside) {
    userSet(!m_checked);
    emit clicked();
  } else {
    retarget();
  }
  e->accept();
}

void MaterialSwitch::mouseUngrabEvent() {
  if (m_dragging) {
    m_dragging = false;
    setKeepMouseGrab(false);
    retarget();
  }
  setPressed(false);
}

void MaterialSwitch::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialSwitch::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (!isEnabled())
    return;
  const bool over = overTrack(e->position());
  setHovered(over);
  setHand(over);
}

void MaterialSwitch::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHovered(false);
  setHand(false);
}

void MaterialSwitch::keyPressEvent(QKeyEvent *e) {
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
      userSet(!m_checked);
      emit clicked();
    }
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialSwitch::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialSwitch::build(Ryu::Mesh &m) {
  const qreal posRaw = m_pos.x;
  const qreal pos = std::clamp(posRaw, 0.0, 1.0);
  const qreal press = std::max(0.0, m_press.x);

  m.setOrigin(QPointF(m.snap((width() - kTrackW) / 2.0),
                      m.snap((height() - kTrackH) / 2.0)));

  const qreal tr = kTrackH / 2.0;
  const QRectF track(0.0, 0.0, kTrackW, kTrackH);
  m.fillRoundedRect(track, tr, tr, tr, tr,
                    Ryu::mix(m_trackColor, m_checkedTrackColor, pos));

  const qreal borderA = 1.0 - pos;
  if (borderA > 0.01 && m_borderColor.alpha() > 0)
    m.strokeRoundedRect(
        track, tr, tr, tr, tr, m.snapStroke(kBorder),
        Ryu::withAlpha(m_borderColor, m_borderColor.alphaF() * borderA));

  const qreal offD = m_showIcons ? kOffIconD : kOffD;
  const qreal baseD = offD + (kOnD - offD) * pos;
  const qreal d = std::max(0.0, baseD + (kPressD - baseD) * press);
  const qreal r = d / 2.0;
  const qreal cx = std::clamp(kOffX + (kOnX - kOffX) * posRaw, r, kTrackW - r);
  const qreal cy = kTrackH / 2.0;
  const QPointF c(cx, cy);

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  if (layer > 0.0)
    m.fillCircle(c, kLayerD / 2.0,
                 Ryu::withAlpha(
                     Ryu::mix(m_handleColor, m_checkedTrackColor, pos), layer));

  m.fillCircle(c, r, Ryu::mix(m_handleColor, m_checkedHandleColor, pos));

  if (!m_showIcons)
    return;

  const QColor ic = Ryu::mix(m_iconColor, m_checkedIconColor, pos);
  const qreal iw = m.snapStroke(kIconW);

  const qreal offA = 1.0 - pos;
  if (offA > 0.02) {
    const QColor col = Ryu::withAlpha(ic, ic.alphaF() * offA);
    m.line(QPointF(cx - kIconS, cy - kIconS), QPointF(cx + kIconS, cy + kIconS),
           iw, col);
    m.line(QPointF(cx - kIconS, cy + kIconS), QPointF(cx + kIconS, cy - kIconS),
           iw, col);
  }

  if (pos > 0.02)
    m.polyline({QPointF(cx - kIconS, cy + 0.5),
                QPointF(cx - kIconS / 3.0, cy + kIconS * 0.8),
                QPointF(cx + kIconS, cy - kIconS * 0.7)},
               iw, Ryu::withAlpha(ic, ic.alphaF() * pos));
}
