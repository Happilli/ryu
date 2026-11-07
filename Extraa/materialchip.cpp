#include "./materialchip.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QImageReader>
#include <QLineF>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QQmlEngine>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight = 32.0;
constexpr qreal kRadius = 8.0;
constexpr qreal kFull = 16.0;
constexpr qreal kPadPlain = 16.0;
constexpr qreal kPadIcon = 8.0;
constexpr qreal kPadAvatar = 4.0;
constexpr qreal kIcon = 18.0;
constexpr qreal kAvatar = 24.0;
constexpr qreal kGap = 8.0;
constexpr qreal kTrailPad = 8.0;
constexpr qreal kTrailHit = 34.0;
constexpr qreal kTrailLayerR = 16.0;
constexpr qreal kFontPx = 14.0;
constexpr qreal kAvatarFontPx = 12.0;
constexpr qreal kPressShrink = 0.04;
constexpr qreal kPressStiffness = 1.4;
constexpr qreal kCheckStiffness = 0.85;
constexpr qreal kSelStiffness = 1.0;
constexpr qreal kHovStiffness = 1.2;
constexpr qreal kMarkW = 2.0;
constexpr qreal kCloseHalf = 4.5;
constexpr qreal kCloseW = 1.5;
constexpr int kSlotLead = 0;
constexpr int kSlotTrail = 1;
constexpr int kSlotText = 2;
constexpr int kSlotAvatar = 3;
constexpr int kSlotAvatarText = 4;
const QPointF kCheckA(-4.5, 0.5);
const QPointF kCheckB(-1.5, 3.5);
const QPointF kCheckC(4.5, -3.5);

qreal lerp(qreal a, qreal b, qreal t) { return a + (b - a) * t; }
} // namespace

MaterialChip::MaterialChip(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

void MaterialChip::componentComplete() {
  SgMaterialItem::componentComplete();
  updateImplicit();
  retarget(true);
  update();
}

QFont MaterialChip::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
}

qreal MaterialChip::textWidth() const {
  if (m_label.isEmpty())
    return 0.0;
  return QFontMetricsF(labelFont()).horizontalAdvance(m_label);
}

bool MaterialChip::hasIcon() const {
  return !m_iconText.isEmpty() ||
         (m_iconSource.isValid() && !m_iconSource.isEmpty());
}

bool MaterialChip::hasAvatar() const {
  return !m_avatarText.isEmpty() ||
         (m_avatarSource.isValid() && !m_avatarSource.isEmpty());
}

bool MaterialChip::customTrailing() const {
  return !m_trailingIconText.isEmpty() ||
         (m_trailingIconSource.isValid() && !m_trailingIconSource.isEmpty());
}

bool MaterialChip::trailingShown() const {
  return customTrailing() || m_closable;
}

bool MaterialChip::checkEligible() const {
  return m_type == Filter && m_showCheck && !hasAvatar();
}

bool MaterialChip::isToggle() const { return m_type == Filter || m_checkable; }

qreal MaterialChip::leadPad(qreal chk) const {
  if (hasAvatar())
    return kPadAvatar;
  if (hasIcon())
    return kPadIcon;
  if (checkEligible())
    return lerp(kPadPlain, kPadIcon, chk);
  return kPadPlain;
}

qreal MaterialChip::slotWidth(qreal chk) const {
  if (hasAvatar())
    return kAvatar;
  if (hasIcon())
    return kIcon;
  if (checkEligible())
    return kIcon * chk;
  return 0.0;
}

qreal MaterialChip::gapWidth(qreal chk) const {
  const qreal slot = slotWidth(chk);
  if (slot <= 0.0 || m_textW <= 0.0)
    return 0.0;
  return kGap * ((hasAvatar() || hasIcon()) ? 1.0 : chk);
}

qreal MaterialChip::trailWidth() const {
  return trailingShown() ? kGap + kIcon + kTrailPad : kPadPlain;
}

QColor MaterialChip::containerBase() const {
  if (m_color.isValid())
    return m_color;
  return m_elevated ? QColor("#F7F2FA") : QColor(Qt::transparent);
}

QColor MaterialChip::containerSelected() const {
  return m_selectedColor.isValid() ? m_selectedColor : QColor("#E8DEF8");
}

QColor MaterialChip::contentBase() const {
  if (m_contentColor.isValid())
    return m_contentColor;
  return m_type == Assist ? QColor("#1D1B20") : QColor("#49454F");
}

QColor MaterialChip::contentSelected() const {
  return m_selectedContentColor.isValid() ? m_selectedContentColor
                                          : QColor("#1D192B");
}

QColor MaterialChip::leadIconBase() const {
  if (m_iconColor.isValid())
    return m_iconColor;
  return m_type == Assist ? QColor("#6750A4") : contentBase();
}

void MaterialChip::updateImplicit() {
  m_textW = textWidth();
  const qreal chk = std::clamp(m_check.x, 0.0, 1.0);
  const qreal w =
      leadPad(chk) + slotWidth(chk) + gapWidth(chk) + m_textW + trailWidth();
  setImplicitWidth(std::ceil(w));
  setImplicitHeight(kHeight);
}

void MaterialChip::retarget(bool immediate) {
  m_round.go(m_pressZone == Body ? 1.0 : 0.0, immediate);
  m_press.go(m_pressZone == Body ? 1.0 : 0.0, immediate);
  m_sel.go(m_selected ? 1.0 : 0.0, immediate);
  m_check.go(m_selected && checkEligible() ? 1.0 : 0.0, immediate);
  m_hov.go(m_hoverZone == Body ? 1.0 : 0.0, immediate);
  const bool trailActive =
      trailingShown() &&
      (m_hoverZone == Trail || m_pressZone == Trail ||
       (m_keyFocus && hasActiveFocus() && m_focusZone == Trail));
  m_tlayer.go(trailActive ? 1.0 : 0.0, immediate);
  m_tpress.go(m_pressZone == Trail ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialChip::advance(qreal dt) {
  bool active = step(m_round, dt);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  active |= step(m_sel, dt, 0.001, kSelStiffness);
  active |= step(m_check, dt, 0.001, kCheckStiffness);
  active |= step(m_hov, dt, 0.001, kHovStiffness);
  active |= step(m_tlayer, dt, 0.001, kHovStiffness);
  active |= step(m_tpress, dt, 0.001, kPressStiffness);
  updateImplicit();
  return active;
}

void MaterialChip::setLabel(const QString &v) {
  if (assign(m_label, v, &MaterialChip::labelChanged))
    updateImplicit();
}

void MaterialChip::setType(Type v) {
  if (assign(m_type, v, &MaterialChip::typeChanged)) {
    updateImplicit();
    retarget();
  }
}

void MaterialChip::setElevated(bool v) {
  assign(m_elevated, v, &MaterialChip::elevatedChanged);
}

void MaterialChip::setRounded(bool v) {
  assign(m_rounded, v, &MaterialChip::roundedChanged);
}

void MaterialChip::setSelected(bool v) {
  if (assign(m_selected, v, &MaterialChip::selectedChanged))
    retarget();
}

void MaterialChip::setCheckable(bool v) {
  assign(m_checkable, v, &MaterialChip::checkableChanged);
}

void MaterialChip::setClosable(bool v) {
  if (assign(m_closable, v, &MaterialChip::closableChanged)) {
    if (!trailingShown() && m_focusZone == Trail)
      m_focusZone = Body;
    updateImplicit();
    retarget();
  }
}

void MaterialChip::setShowCheck(bool v) {
  if (assign(m_showCheck, v, &MaterialChip::showCheckChanged)) {
    updateImplicit();
    retarget();
  }
}

void MaterialChip::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialChip::fontChanged))
    updateImplicit();
}

void MaterialChip::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialChip::fontWeightChanged))
    updateImplicit();
}

void MaterialChip::setIconText(const QString &v) {
  if (assign(m_iconText, v, &MaterialChip::iconTextChanged))
    updateImplicit();
}

void MaterialChip::setIconSource(const QUrl &v) {
  if (assign(m_iconSource, v, &MaterialChip::iconSourceChanged)) {
    m_leadPath.clear();
    m_leadCache.invalidate();
    updateImplicit();
  }
}

void MaterialChip::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialChip::iconFontChanged);
}

void MaterialChip::setTrailingIconText(const QString &v) {
  if (assign(m_trailingIconText, v, &MaterialChip::trailingIconTextChanged)) {
    if (!trailingShown() && m_focusZone == Trail)
      m_focusZone = Body;
    updateImplicit();
  }
}

void MaterialChip::setTrailingIconSource(const QUrl &v) {
  if (assign(m_trailingIconSource, v,
             &MaterialChip::trailingIconSourceChanged)) {
    m_trailPath.clear();
    m_trailCache.invalidate();
    if (!trailingShown() && m_focusZone == Trail)
      m_focusZone = Body;
    updateImplicit();
  }
}

void MaterialChip::setAvatarText(const QString &v) {
  if (assign(m_avatarText, v, &MaterialChip::avatarTextChanged))
    updateImplicit();
}

void MaterialChip::setAvatarSource(const QUrl &v) {
  if (assign(m_avatarSource, v, &MaterialChip::avatarSourceChanged)) {
    m_avatarDirty = true;
    ++m_avatarGen;
    updateImplicit();
  }
}

void MaterialChip::setColor(const QColor &v) {
  assign(m_color, v, &MaterialChip::colorChanged);
}

void MaterialChip::setSelectedColor(const QColor &v) {
  assign(m_selectedColor, v, &MaterialChip::selectedColorChanged);
}

void MaterialChip::setContentColor(const QColor &v) {
  assign(m_contentColor, v, &MaterialChip::contentColorChanged);
}

void MaterialChip::setSelectedContentColor(const QColor &v) {
  assign(m_selectedContentColor, v, &MaterialChip::selectedContentColorChanged);
}

void MaterialChip::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialChip::iconColorChanged);
}

void MaterialChip::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialChip::borderColorChanged);
}

void MaterialChip::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v), &MaterialChip::borderWidthChanged);
}

void MaterialChip::setAvatarColor(const QColor &v) {
  assign(m_avatarColor, v, &MaterialChip::avatarColorChanged);
}

void MaterialChip::setAvatarTextColor(const QColor &v) {
  assign(m_avatarTextColor, v, &MaterialChip::avatarTextColorChanged);
}

int MaterialChip::hit(const QPointF &pt) const {
  const qreal y = std::round((height() - kHeight) / 2.0);
  if (pt.y() < y || pt.y() > y + kHeight || pt.x() < 0.0 || pt.x() > width())
    return None;
  if (trailingShown() && pt.x() >= width() - kTrailHit)
    return Trail;
  return Body;
}

void MaterialChip::setPressZone(int z) {
  if (m_pressZone == z)
    return;
  const bool was = m_pressZone != None;
  m_pressZone = z;
  if (was != (z != None))
    emit pressedChanged();
  retarget();
  update();
}

void MaterialChip::setHoverZone(int z) {
  if (m_hoverZone == z)
    return;
  const bool was = m_hoverZone != None;
  m_hoverZone = z;
  if (was != (z != None))
    emit hoveredChanged();
  setHand(z != None);
  retarget();
  update();
}

void MaterialChip::activate(int zone) {
  if (zone == Trail) {
    emit trailingClicked();
    if (m_closable && !customTrailing())
      emit closeClicked();
    update();
    return;
  }
  if (isToggle()) {
    m_selected = !m_selected;
    emit selectedChanged();
    emit toggled();
    retarget();
  }
  emit clicked();
  update();
}

void MaterialChip::click() {
  if (!isEnabled())
    return;
  activate(Body);
}

void MaterialChip::mousePressEvent(QMouseEvent *e) {
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
  m_focusZone = z;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressZone(z);
  e->accept();
}

void MaterialChip::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressZone == None || m_keyDown)
    return;
  const int pressed = m_pressZone;
  const bool inside = hit(e->position()) == pressed;
  setPressZone(None);
  if (inside)
    activate(pressed);
  e->accept();
}

void MaterialChip::mouseUngrabEvent() {
  if (!m_keyDown)
    setPressZone(None);
}

void MaterialChip::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  if (isEnabled())
    setHoverZone(hit(e->position()));
}

void MaterialChip::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (isEnabled())
    setHoverZone(hit(e->position()));
}

void MaterialChip::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHoverZone(None);
}

void MaterialChip::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Left:
    m_focusZone = Body;
    break;
  case Qt::Key_Right:
    if (trailingShown())
      m_focusZone = Trail;
    break;
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (!e->isAutoRepeat()) {
      m_keyFocus = true;
      m_keyDown = true;
      setPressZone(m_focusZone);
    }
    break;
  case Qt::Key_Backspace:
  case Qt::Key_Delete:
    if (m_closable && !customTrailing() && !e->isAutoRepeat()) {
      m_keyFocus = true;
      emit closeClicked();
    } else {
      e->ignore();
      return;
    }
    break;
  default:
    e->ignore();
    return;
  }
  m_keyFocus = true;
  retarget();
  update();
  e->accept();
}

void MaterialChip::keyReleaseEvent(QKeyEvent *e) {
  if (!m_keyDown || e->isAutoRepeat()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter: {
    m_keyDown = false;
    const int zone = m_pressZone;
    setPressZone(None);
    if (zone != None)
      activate(zone);
    break;
  }
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialChip::focusInEvent(QFocusEvent *e) {
  QQuickItem::focusInEvent(e);
  if (!trailingShown())
    m_focusZone = Body;
}

void MaterialChip::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  if (m_keyDown) {
    m_keyDown = false;
    setPressZone(None);
  }
  retarget();
  update();
}

void MaterialChip::loadAvatar() {
  m_avatarDirty = false;
  m_avatarImg = QImage();
  if (!m_avatarSource.isValid() || m_avatarSource.isEmpty())
    return;
  const QString path = Ryu::resolveUrl(qmlContext(this), m_avatarSource);
  QImageReader r(path);
  const QImage in = r.read();
  if (in.isNull()) {
    qWarning("MaterialChip: failed to load avatar '%s'", qPrintable(path));
    return;
  }
  constexpr int px = int(kAvatar) * 3;
  const QImage scaled = in.scaled(px, px, Qt::KeepAspectRatioByExpanding,
                                  Qt::SmoothTransformation);
  m_avatarImg = scaled.copy((scaled.width() - px) / 2,
                            (scaled.height() - px) / 2, px, px);
}

void MaterialChip::iconLayer(Ryu::Mesh &m, int slot, Ryu::IconCache &cache,
                             QString &path, const QUrl &src,
                             const QString &glyph, const QColor &col,
                             const QPointF &c, qreal size) {
  if (src.isValid() && !src.isEmpty()) {
    if (path.isEmpty())
      path = Ryu::resolveUrl(qmlContext(this), src);
    const qreal dpr = m.dpr();
    const QColor cc = Ryu::withAlpha(col, col.alphaF() * fade());
    const QString key = QStringLiteral("i|%1|%2|%3|%4")
                            .arg(path, cc.name(QColor::HexArgb))
                            .arg(size)
                            .arg(dpr);
    useLayer(slot, key,
             QRectF(m.snap(c.x() - size / 2.0), m.snap(c.y() - size / 2.0),
                    size, size),
             [&] { return cache.get(path, size, dpr, cc); });
  } else {
    glyphLayer(slot, m_iconFont, glyph, col, c, size);
  }
}

void MaterialChip::build(Ryu::Mesh &m) {
  const qreal w = width(), ih = height();
  if (w <= 0.0 || ih <= 0.0)
    return;

  const qreal dpr = m.dpr();
  const qreal sel = std::clamp(m_sel.x, 0.0, 1.0);
  const qreal selQ = std::round(sel * 8.0) / 8.0;
  const qreal chk = std::clamp(m_check.x, 0.0, 1.0);
  const qreal press = std::max(0.0, m_press.x);
  const bool av = hasAvatar();
  const bool ic = hasIcon();
  const bool ce = checkEligible();

  const qreal y = std::round((ih - kHeight) / 2.0);
  const qreal scale = 1.0 - kPressShrink * std::min(press, 1.4);
  const QPointF c(w / 2.0, y + kHeight / 2.0);
  const QRectF rect(c.x() - w * scale / 2.0, c.y() - kHeight * scale / 2.0,
                    w * scale, kHeight * scale);

  const qreal baseR = (m_rounded || av) ? kFull : kRadius;
  const qreal radius =
      std::max(0.0, baseR + (kFull - baseR) * m_round.x) * scale;

  const QColor cb = containerBase();
  const QColor cs = containerSelected();
  const QColor bg = cb.alpha() == 0 ? Ryu::withAlpha(cs, cs.alphaF() * sel)
                                    : Ryu::mix(cb, cs, sel);

  if (m_elevated) {
    m.fillRoundedRect(rect.translated(0.0, 2.0), radius, radius, radius, radius,
                      QColor(0, 0, 0, 22));
    m.fillRoundedRect(rect.translated(0.0, 1.0), radius, radius, radius, radius,
                      QColor(0, 0, 0, 30));
  }

  if (bg.alpha() > 0)
    m.fillRoundedRect(rect, radius, radius, radius, radius, bg);

  const qreal borderA = 1.0 - sel;
  if (!m_elevated && m_borderWidth > 0.0 && borderA > 0.01 &&
      m_borderColor.alpha() > 0)
    m.strokeRoundedRect(
        rect, radius, radius, radius, radius, m.snapStroke(m_borderWidth),
        Ryu::withAlpha(m_borderColor, m_borderColor.alphaF() * borderA));

  const QColor fgSel = contentSelected();
  const QColor fg = Ryu::mix(contentBase(), fgSel, selQ);
  const QColor iconCol = Ryu::mix(leadIconBase(), fgSel, selQ);

  const bool bodyActive =
      m_pressZone == Body ||
      (m_keyFocus && hasActiveFocus() && m_focusZone == Body);
  const qreal hov = std::clamp(m_hov.x, 0.0, 1.0);
  const qreal la =
      std::max(Ryu::kHoverLayer * hov, bodyActive ? Ryu::kPressLayer : 0.0);
  if (la > 0.0)
    m.fillRoundedRect(rect, radius, radius, radius, radius,
                      Ryu::withAlpha(fg, la));

  const bool trail = trailingShown();
  const qreal icx = w - kTrailPad - kIcon / 2.0;

  if (trail) {
    const qreal tl = std::clamp(m_tlayer.x, 0.0, 1.2);
    if (tl > 0.01) {
      const bool tPress =
          m_pressZone == Trail ||
          (m_keyFocus && hasActiveFocus() && m_focusZone == Trail && m_keyDown);
      const qreal a =
          (tPress ? Ryu::kPressLayer : Ryu::kHoverLayer) * std::min(tl, 1.0);
      m.fillCircle(QPointF(icx, c.y()), kTrailLayerR * (0.5 + 0.5 * tl),
                   Ryu::withAlpha(fg, a));
    }
  }

  const qreal slot = slotWidth(chk);
  const qreal gap = gapWidth(chk);
  const qreal extra = trail ? 0.0 : std::max(0.0, (w - implicitWidth()) / 2.0);
  const qreal x = leadPad(chk) + extra;
  const qreal cy = c.y();

  if (av) {
    const QPointF ac(x + kAvatar / 2.0, cy);
    if (m_avatarDirty)
      loadAvatar();
    if (!m_avatarImg.isNull()) {
      const qreal fd = fade();
      const QString key = QStringList{"av", QString::number(m_avatarGen),
                                      QString::number(dpr), QString::number(fd)}
                              .join(QLatin1Char('|'));
      useLayer(kSlotAvatar, key,
               QRectF(m.snap(ac.x() - kAvatar / 2.0),
                      m.snap(ac.y() - kAvatar / 2.0), kAvatar, kAvatar),
               [&] {
                 QImage img = Ryu::newImage(kAvatar, kAvatar, dpr);
                 QPainter p(&img);
                 p.setRenderHint(QPainter::Antialiasing, true);
                 p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                 p.setOpacity(fd);
                 QPainterPath clip;
                 clip.addEllipse(QRectF(0.0, 0.0, kAvatar, kAvatar));
                 p.setClipPath(clip);
                 p.drawImage(QRectF(0.0, 0.0, kAvatar, kAvatar), m_avatarImg);
                 p.end();
                 return img;
               });
    } else {
      m.fillCircle(ac, kAvatar / 2.0, m_avatarColor);
      const QFont af = Ryu::styledFont(m_font, kAvatarFontPx, QFont::Medium);
      const qreal tw = QFontMetricsF(af).horizontalAdvance(m_avatarText);
      textLayer(kSlotAvatarText, af, m_avatarText, m_avatarTextColor,
                ac.x() - tw / 2.0, ac.y());
    }
  } else if (ic) {
    const qreal iconA = ce ? std::round((1.0 - chk) * 10.0) / 10.0 : 1.0;
    if (iconA > 0.0) {
      const QPointF lc(x + kIcon / 2.0, cy);
      iconLayer(m, kSlotLead, m_leadCache, m_leadPath, m_iconSource, m_iconText,
                Ryu::withAlpha(iconCol, iconCol.alphaF() * iconA), lc, kIcon);
    }
  }

  if (ce && chk > 0.01) {
    const QPointF cc(x + kIcon / 2.0, cy);
    const qreal l1 = QLineF(kCheckA, kCheckB).length();
    const qreal l2 = QLineF(kCheckB, kCheckC).length();
    const qreal d = chk * (l1 + l2);
    const qreal k = kIcon / 18.0;
    auto map = [&](const QPointF &p) { return cc + p * k; };

    QList<QPointF> pts;
    pts.append(map(kCheckA));
    if (d <= l1) {
      pts.append(map(kCheckA + (kCheckB - kCheckA) * (d / l1)));
    } else {
      pts.append(map(kCheckB));
      pts.append(map(kCheckB + (kCheckC - kCheckB) * ((d - l1) / l2)));
    }
    m.polyline(pts, kMarkW * k, fg);
  }

  textLayer(kSlotText, labelFont(), m_label, fg, x + slot + gap, cy);

  if (trail) {
    const QPointF tc(icx, cy);
    if (customTrailing()) {
      iconLayer(m, kSlotTrail, m_trailCache, m_trailPath, m_trailingIconSource,
                m_trailingIconText, fg, tc, kIcon);
    } else {
      const qreal tp = std::clamp(m_tpress.x, 0.0, 1.4);
      const qreal half = kCloseHalf * (1.0 - 0.18 * tp);
      const qreal lw = m.snapStroke(kCloseW);
      m.line(QPointF(tc.x() - half, tc.y() - half),
             QPointF(tc.x() + half, tc.y() + half), lw, fg);
      m.line(QPointF(tc.x() - half, tc.y() + half),
             QPointF(tc.x() + half, tc.y() - half), lw, fg);
    }
  }
}
