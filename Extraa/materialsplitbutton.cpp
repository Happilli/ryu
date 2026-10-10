#include "./materialsplitbutton.hpp"
#include "../Shapes/springdriver.hpp"
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight[] = {32.0, 40.0, 56.0, 96.0, 136.0};
constexpr qreal kIconSize[] = {20.0, 24.0, 24.0, 32.0, 40.0};
constexpr qreal kFontPx[] = {14.0, 14.0, 16.0, 24.0, 32.0};
constexpr qreal kPadOuter[] = {12.0, 16.0, 24.0, 48.0, 64.0};
constexpr qreal kPadInner[] = {10.0, 12.0, 24.0, 44.0, 60.0};
constexpr qreal kIconGap[] = {4.0, 8.0, 8.0, 12.0, 16.0};
constexpr qreal kTrailW[] = {32.0, 40.0, 56.0, 96.0, 136.0};
constexpr qreal kChevron[] = {10.0, 12.0, 14.0, 20.0, 26.0};
constexpr qreal kInnerR[] = {6.0, 8.0, 8.0, 12.0, 16.0};
constexpr qreal kSegGap = 2.0;
constexpr qreal kMaxStep = 0.004;

QPainterPath sidedRect(const QRectF &r, qreal rl, qreal rr) {
  const qreal maxR = std::min(r.height(), r.width()) / 2.0;
  rl = std::clamp(rl, 0.0, maxR);
  rr = std::clamp(rr, 0.0, maxR);
  const qreal x0 = r.left(), x1 = r.right(), t = r.top(), b = r.bottom();

  QPainterPath p;
  p.moveTo(x0 + rl, t);
  p.lineTo(x1 - rr, t);
  if (rr > 0.0)
    p.arcTo(x1 - 2 * rr, t, 2 * rr, 2 * rr, 90, -90);
  p.lineTo(x1, b - rr);
  if (rr > 0.0)
    p.arcTo(x1 - 2 * rr, b - 2 * rr, 2 * rr, 2 * rr, 0, -90);
  p.lineTo(x0 + rl, b);
  if (rl > 0.0)
    p.arcTo(x0, b - 2 * rl, 2 * rl, 2 * rl, 270, -90);
  p.lineTo(x0, t + rl);
  if (rl > 0.0)
    p.arcTo(x0, t, 2 * rl, 2 * rl, 180, -90);
  p.closeSubpath();
  return p;
}

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
} // namespace

MaterialSplitButton::MaterialSplitButton(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

  updateImplicit();
  retarget(true);
}

QFont MaterialSplitButton::labelFont() const {
  QFont f = m_font;
  f.setPixelSize(qRound(kFontPx[m_sizeStyle]));
  f.setWeight(QFont::Weight(m_fontWeight));
  f.setHintingPreference(QFont::PreferNoHinting);
  f.setStyleStrategy(
      QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
  return f;
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
    w += kIconSize[s];
  if (hasIcon && tw > 0.0)
    w += kIconGap[s];
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
  setImplicitWidth(leadWidth() + kSegGap + kTrailW[m_sizeStyle]);
  setImplicitHeight(kHeight[m_sizeStyle]);
}

int MaterialSplitButton::hit(const QPointF &pt) const {
  const qreal h = kHeight[m_sizeStyle];
  const qreal y = (height() - h) / 2.0;
  if (pt.y() < y || pt.y() > y + h)
    return None;
  const qreal lw = leadWidth();
  const qreal total = lw + kSegGap + kTrailW[m_sizeStyle];
  if (pt.x() < 0.0 || pt.x() > total)
    return None;
  return pt.x() < lw + kSegGap / 2.0 ? Lead : Trail;
}

void MaterialSplitButton::retarget(bool immediate) {
  m_roundLead.t = m_pressZone == Lead ? 1.0 : 0.0;
  m_roundTrail.t = (m_pressZone == Trail || m_menuOpen) ? 1.0 : 0.0;
  m_chev.t = m_menuOpen ? 1.0 : 0.0;
  if (immediate) {
    for (Spring *s : {&m_roundLead, &m_roundTrail, &m_chev}) {
      s->x = s->t;
      s->v = 0.0;
    }
    return;
  }
  ensureDriver();
}

void MaterialSplitButton::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialSplitButton::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  bool active = false;
  active |= stepSpring(m_roundLead.x, m_roundLead.v, m_roundLead.t, m_stiffness,
                       m_damping, dt, 0.001);
  active |= stepSpring(m_roundTrail.x, m_roundTrail.v, m_roundTrail.t,
                       m_stiffness, m_damping, dt, 0.001);
  active |= stepSpring(m_chev.x, m_chev.v, m_chev.t, m_stiffness * 0.8,
                       m_damping, dt, 0.001);
  update();
  if (!active)
    m_driver->stop();
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

void MaterialSplitButton::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialSplitButton::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
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
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const int s = m_sizeStyle;
  const qreal h = kHeight[s];
  const qreal full = h / 2.0;
  const qreal inner = kInnerR[s];
  const qreal y = (height() - h) / 2.0;
  const qreal lw = leadWidth();
  const qreal tw = kTrailW[s];

  const QRectF leadRect(0.0, y, lw, h);
  const QRectF trailRect(lw + kSegGap, y, tw, h);

  const qreal tL = std::clamp(m_roundLead.x, 0.0, 1.0);
  const qreal tT = std::clamp(m_roundTrail.x, 0.0, 1.0);

  const QPainterPath leadPath =
      sidedRect(leadRect, full, inner + (full - inner) * tL);
  const QPainterPath trailPath =
      sidedRect(trailRect, inner + (full - inner) * tT, full);

  const QColor bg = containerColor();
  const QColor fg = foregroundColor();

  auto drawSegment = [&](const QPainterPath &path, int zone) {
    p->setPen(Qt::NoPen);
    if (bg.alpha() > 0) {
      p->setBrush(bg);
      p->drawPath(path);
    }

    if (m_type == Outlined && m_borderWidth > 0.0 &&
        m_borderColor.alpha() > 0) {
      p->save();
      p->setClipPath(path);
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(m_borderColor, m_borderWidth * 2.0, Qt::SolidLine,
                     Qt::FlatCap, Qt::RoundJoin));
      p->drawPath(path);
      p->restore();
      p->setPen(Qt::NoPen);
    }

    qreal layer = 0.0;
    if (m_hoverZone == zone)
      layer = 0.08;
    if (m_pressZone == zone || (m_keyFocus && hasActiveFocus() && zone == Lead))
      layer = 0.10;
    if (layer > 0.0) {
      QColor l = fg;
      l.setAlphaF(layer);
      p->setBrush(l);
      p->drawPath(path);
    }
  };

  drawSegment(leadPath, Lead);
  drawSegment(trailPath, Trail);

  const qreal iconSz = kIconSize[s];
  const bool hasIcon = !m_iconText.isEmpty();
  qreal x = leadRect.left() + kPadOuter[s];

  if (hasIcon) {
    QFont f = m_iconFont;
    f.setPixelSize(qRound(iconSz));
    f.setHintingPreference(QFont::PreferNoHinting);

    QPainterPath gp;
    gp.addText(QPointF(0.0, 0.0), f, m_iconText);
    const QRectF br = gp.boundingRect();
    if (!br.isEmpty()) {
      const qreal k = std::min(1.0, iconSz / std::max(br.width(), br.height()));
      p->save();
      p->translate(x + iconSz / 2.0, leadRect.center().y());
      p->scale(k, k);
      p->translate(-br.center());
      p->setPen(Qt::NoPen);
      p->setBrush(fg);
      p->drawPath(gp);
      p->restore();
    }
    x += iconSz;
    if (!m_label.isEmpty())
      x += kIconGap[s];
  }

  if (!m_label.isEmpty()) {
    const QFont lf = labelFont();
    const QFontMetricsF fm(lf);
    QPainterPath tp;
    tp.addText(QPointF(0.0, 0.0), lf, m_label);
    p->save();
    p->translate(x, leadRect.center().y() + (fm.ascent() - fm.descent()) / 2.0);
    p->setPen(Qt::NoPen);
    p->setBrush(fg);
    p->drawPath(tp);
    p->restore();
  }

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
