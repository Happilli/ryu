#include "./materialswitch.hpp"
#include "../Shapes/springdriver.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
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
constexpr qreal kMaxStep = 0.004;

bool stepSpring(qreal &x, qreal &v, qreal t, qreal k, qreal z, qreal dt,
                qreal eps) {
  if (std::abs(x - t) < eps && std::abs(v) < eps * 10.0) {
    x = t;
    v = 0.0;
    return false;
  }
  const qreal c = 2.0 * z * std::sqrt(k);
  const int steps = std::max(1, int(std::ceil(dt / kMaxStep)));
  const qreal h = dt / steps;
  for (int i = 0; i < steps; ++i) {
    const qreal acc = -k * (x - t) - c * v;
    v += acc * h;
    x += v * h;
  }
  return true;
}

QColor mix(const QColor &a, const QColor &b, qreal t) {
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}
} // namespace

MaterialSwitch::MaterialSwitch(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setImplicitWidth(kTrackW);
  setImplicitHeight(kTrackH);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

  retarget(true);
}

void MaterialSwitch::retarget(bool immediate) {
  if (!m_dragging)
    m_pos.t = m_checked ? 1.0 : 0.0;
  m_press.t = m_pressed ? 1.0 : 0.0;
  if (immediate) {
    m_pos.x = m_pos.t;
    m_pos.v = 0.0;
    m_press.x = m_press.t;
    m_press.v = 0.0;
    return;
  }
  ensureDriver();
}

void MaterialSwitch::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialSwitch::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  bool active = false;
  if (!m_dragging)
    active |= stepSpring(m_pos.x, m_pos.v, m_pos.t, m_stiffness, m_damping, dt,
                         0.001);
  active |= stepSpring(m_press.x, m_press.v, m_press.t, m_stiffness * 1.4,
                       m_damping, dt, 0.001);
  update();
  if (!active && !m_dragging)
    m_driver->stop();
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

void MaterialSwitch::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialSwitch::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
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
  if (!isEnabled()) {
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
  const qreal dx = e->position().x() - m_pressX;
  if (!m_dragging && std::abs(dx) > 3.0) {
    m_dragging = true;
    setKeepMouseGrab(true);
    ensureDriver();
  }
  if (m_dragging) {
    const qreal off = (width() - kTrackW) / 2.0;
    const qreal t = (e->position().x() - off - kOffX) / (kOnX - kOffX);
    m_pos.x = std::clamp(t, 0.0, 1.0);
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
    retarget();
  }
  setPressed(false);
}

void MaterialSwitch::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  if (isEnabled())
    setHovered(true);
}

void MaterialSwitch::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setHovered(false);
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
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialSwitch::paint(QPainter *p) {
  p->setRenderHint(QPainter::Antialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const qreal pos = std::clamp(m_pos.x, 0.0, 1.0);
  const qreal press = std::clamp(m_press.x, 0.0, 1.0);

  p->translate((width() - kTrackW) / 2.0, (height() - kTrackH) / 2.0);

  const QRectF track(0.0, 0.0, kTrackW, kTrackH);
  const QColor trackBg = mix(m_trackColor, m_checkedTrackColor, pos);
  p->setPen(Qt::NoPen);
  p->setBrush(trackBg);
  p->drawRoundedRect(track, kTrackH / 2.0, kTrackH / 2.0);

  const qreal borderA = 1.0 - pos;
  if (borderA > 0.01 && m_borderColor.alpha() > 0) {
    QColor bc = m_borderColor;
    bc.setAlphaF(bc.alphaF() * borderA);
    const qreal half = kBorder / 2.0;
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(bc, kBorder));
    p->drawRoundedRect(track.adjusted(half, half, -half, -half),
                       kTrackH / 2.0 - half, kTrackH / 2.0 - half);
    p->setPen(Qt::NoPen);
  }

  const qreal offD = m_showIcons ? kOffIconD : kOffD;
  qreal d = offD + (kOnD - offD) * pos;
  d = d + (kPressD - d) * press;
  d = std::max(0.0, d);
  const qreal cx = kOffX + (kOnX - kOffX) * pos;
  const qreal cy = kTrackH / 2.0;
  const QColor handleBg = mix(m_handleColor, m_checkedHandleColor, pos);

  qreal layer = 0.0;
  if (m_hovered)
    layer = 0.08;
  if (m_pressed || (m_keyFocus && hasActiveFocus()))
    layer = 0.10;
  if (layer > 0.0) {
    QColor l = mix(m_handleColor, m_checkedTrackColor, pos);
    l.setAlphaF(layer);
    p->setBrush(l);
    p->drawEllipse(QPointF(cx, cy), kLayerD / 2.0, kLayerD / 2.0);
  }

  p->setBrush(handleBg);
  p->drawEllipse(QPointF(cx, cy), d / 2.0, d / 2.0);

  if (m_showIcons) {
    const QColor ic = mix(m_iconColor, m_checkedIconColor, pos);
    const qreal s = 4.0;
    QPen pen(ic, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setBrush(Qt::NoBrush);

    const qreal offA = 1.0 - pos;
    if (offA > 0.02) {
      QColor c = ic;
      c.setAlphaF(c.alphaF() * offA);
      pen.setColor(c);
      p->setPen(pen);
      p->drawLine(QPointF(cx - s, cy - s), QPointF(cx + s, cy + s));
      p->drawLine(QPointF(cx - s, cy + s), QPointF(cx + s, cy - s));
    }
    if (pos > 0.02) {
      QColor c = ic;
      c.setAlphaF(c.alphaF() * pos);
      pen.setColor(c);
      p->setPen(pen);
      QPainterPath check;
      check.moveTo(cx - s, cy + 0.5);
      check.lineTo(cx - s / 3.0, cy + s * 0.8);
      check.lineTo(cx + s, cy - s * 0.7);
      p->drawPath(check);
    }
  }
}
