#include "./materialsegmentedbutton.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QLineF>
#include <QMetaObject>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight = 40.0;
constexpr qreal kPad = 12.0;
constexpr qreal kIcon = 18.0;
constexpr qreal kGap = 8.0;
constexpr qreal kFontPx = 14.0;
constexpr qreal kCheckStiffness = 0.9;
constexpr qreal kWidthStiffness = 1.2;
constexpr qreal kPressGrow = 0.28;
constexpr qreal kPopKick = 180.0;
constexpr qreal kMarkW = 2.0;
constexpr int kSlots = 3;
const QPointF kCheckA(-4.5, 0.5);
const QPointF kCheckB(-1.5, 3.5);
const QPointF kCheckC(4.5, -3.5);
} // namespace

MaterialSegmentedButton::MaterialSegmentedButton(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.55) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  structureChanged();
}

void MaterialSegmentedButton::componentComplete() {
  SgMaterialItem::componentComplete();
  retarget(true);
  update();
}

QFont MaterialSegmentedButton::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
}

void MaterialSegmentedButton::ensureMetrics() const {
  const int n = count();
  if (!m_metricsDirty && m_textW.size() == n)
    return;
  m_metricsDirty = false;
  m_textW.assign(n, 0.0);
  const QFontMetricsF fm(labelFont());
  for (int i = 0; i < n; ++i) {
    const QString lbl = m_labels.value(i);
    if (!lbl.isEmpty())
      m_textW[i] = fm.horizontalAdvance(lbl);
  }
}

bool MaterialSegmentedButton::hasIcon(int i) const {
  return !m_icons.value(i).isEmpty();
}

qreal MaterialSegmentedButton::slotWidth(int i) const {
  return (hasIcon(i) || m_showCheck) ? kIcon : 0.0;
}

qreal MaterialSegmentedButton::segmentWidth(int i) const {
  ensureMetrics();
  const qreal slot = slotWidth(i);
  const qreal tw = m_textW.value(i);
  qreal w = 2.0 * kPad + slot + tw;
  if (slot > 0.0 && tw > 0.0)
    w += kGap;
  return w;
}

qreal MaterialSegmentedButton::naturalWidth() const {
  qreal total = 0.0;
  const int n = count();
  for (int i = 0; i < n; ++i)
    total += segmentWidth(i);
  return total;
}

QVector<QRectF> MaterialSegmentedButton::layoutRects() const {
  const int n = count();
  QVector<QRectF> out(n);
  if (n == 0)
    return out;
  const qreal y = std::round((height() - kHeight) / 2.0);
  qreal x = 0.0;
  for (int i = 0; i < n; ++i) {
    const qreal base = m_fillWidth ? width() / n : segmentWidth(i);
    const qreal w = std::max(1.0, base + m_width.value(i).x);
    out[i] = QRectF(x, y, w, kHeight);
    x += w;
  }
  return out;
}

void MaterialSegmentedButton::structureChanged() {
  setHand(false);
  m_metricsDirty = true;
  setImplicitWidth(std::ceil(naturalWidth()));
  setImplicitHeight(kHeight);
  const int n = count();
  m_checked.erase(std::remove_if(m_checked.begin(), m_checked.end(),
                                 [n](int i) { return i < 0 || i >= n; }),
                  m_checked.end());
  m_disabled.erase(std::remove_if(m_disabled.begin(), m_disabled.end(),
                                  [n](int i) { return i < 0 || i >= n; }),
                   m_disabled.end());
  m_focusIndex = std::clamp(m_focusIndex, 0, std::max(0, n - 1));
  m_pressIndex = m_hoverIndex = -1;
  retarget(true);
  update();
}

qreal MaterialSegmentedButton::targetWidth(int i) const {
  const int n = count();
  if (m_pressIndex < 0 || n < 2)
    return 0.0;
  const qreal g = kHeight * kPressGrow;
  if (i == m_pressIndex)
    return g;
  const int nb = (m_pressIndex > 0 ? 1 : 0) + (m_pressIndex < n - 1 ? 1 : 0);
  if (i == m_pressIndex - 1 || i == m_pressIndex + 1)
    return -g / nb;
  return 0.0;
}

void MaterialSegmentedButton::kick(int i) {
  const int n = count();
  if (i < 0 || i >= n || n < 2 || m_width.size() != n)
    return;
  const int nb = (i > 0 ? 1 : 0) + (i < n - 1 ? 1 : 0);
  m_width[i].v += kPopKick;
  if (i > 0)
    m_width[i - 1].v -= kPopKick / nb;
  if (i < n - 1)
    m_width[i + 1].v -= kPopKick / nb;
  animate();
}

void MaterialSegmentedButton::retarget(bool immediate) {
  const int n = count();
  if (m_fill.size() != n) {
    m_fill.resize(n);
    m_check.resize(n);
    m_width.resize(n);
    immediate = true;
  }
  for (int i = 0; i < n; ++i) {
    const qreal on = isChecked(i) ? 1.0 : 0.0;
    m_fill[i].go(on, immediate);
    m_check[i].go(on, immediate);
    m_width[i].go(targetWidth(i), immediate);
  }
  if (!immediate)
    animate();
}

bool MaterialSegmentedButton::advance(qreal dt) {
  bool active = false;
  for (int i = 0; i < m_fill.size(); ++i) {
    active |= step(m_fill[i], dt);
    active |= step(m_check[i], dt, 0.001, kCheckStiffness);
    active |= step(m_width[i], dt, 0.02, kWidthStiffness);
  }
  return active;
}

void MaterialSegmentedButton::setLabels(const QStringList &v) {
  if (assign(m_labels, v, &MaterialSegmentedButton::labelsChanged))
    structureChanged();
}

void MaterialSegmentedButton::setIcons(const QStringList &v) {
  if (assign(m_icons, v, &MaterialSegmentedButton::iconsChanged))
    structureChanged();
}

void MaterialSegmentedButton::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialSegmentedButton::fontChanged))
    structureChanged();
}

void MaterialSegmentedButton::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSegmentedButton::iconFontChanged);
}

void MaterialSegmentedButton::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialSegmentedButton::fontWeightChanged))
    structureChanged();
}

void MaterialSegmentedButton::setSelectionMode(SelectionMode v) {
  if (!assign(m_mode, v, &MaterialSegmentedButton::selectionModeChanged))
    return;
  if (v == Single && m_checked.size() > 1)
    m_checked = {m_checked.first()};
  emit checkedIndicesChanged();
  retarget();
}
void MaterialSegmentedButton::setCheckIconSource(const QUrl &v) {
  if (assign(m_checkIconSource, v,
             &MaterialSegmentedButton::checkIconSourceChanged)) {
    m_checkPath.clear();
    m_checkCache.invalidate();
  }
}
void MaterialSegmentedButton::setCheckedIndices(const QList<int> &v) {
  const int n = count();
  QList<int> c = v;
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  c.erase(std::remove_if(c.begin(), c.end(),
                         [n](int i) { return i < 0 || i >= n; }),
          c.end());
  if (m_mode == Single && c.size() > 1)
    c = {c.first()};
  if (assign(m_checked, c, &MaterialSegmentedButton::checkedIndicesChanged))
    retarget();
}

void MaterialSegmentedButton::setCurrentIndex(int i) {
  setCheckedIndices(i < 0 ? QList<int>{} : QList<int>{i});
}

void MaterialSegmentedButton::setChecked(int i, bool on) {
  QList<int> c = m_checked;
  if (m_mode == Single)
    c = on ? QList<int>{i} : QList<int>{};
  else if (on && !c.contains(i))
    c.append(i);
  else if (!on)
    c.removeAll(i);
  setCheckedIndices(c);
}

void MaterialSegmentedButton::setDisabledIndices(const QList<int> &v) {
  if (assign(m_disabled, v, &MaterialSegmentedButton::disabledIndicesChanged))
    structureChanged();
}

void MaterialSegmentedButton::setShowCheck(bool v) {
  if (assign(m_showCheck, v, &MaterialSegmentedButton::showCheckChanged))
    structureChanged();
}

void MaterialSegmentedButton::setFillWidth(bool v) {
  assign(m_fillWidth, v, &MaterialSegmentedButton::fillWidthChanged);
}

void MaterialSegmentedButton::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialSegmentedButton::borderWidthChanged);
}

void MaterialSegmentedButton::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSegmentedButton::borderColorChanged);
}

void MaterialSegmentedButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialSegmentedButton::colorChanged);
}

void MaterialSegmentedButton::setCheckedColor(const QColor &v) {
  assign(m_checkedColor, v, &MaterialSegmentedButton::checkedColorChanged);
}

void MaterialSegmentedButton::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialSegmentedButton::textColorChanged);
}

void MaterialSegmentedButton::setCheckedTextColor(const QColor &v) {
  assign(m_checkedTextColor, v,
         &MaterialSegmentedButton::checkedTextColorChanged);
}

int MaterialSegmentedButton::hit(const QPointF &pt) const {
  const QVector<QRectF> rects = layoutRects();
  for (int i = 0; i < rects.size(); ++i)
    if (rects[i].contains(pt))
      return i;
  return -1;
}

int MaterialSegmentedButton::stepFocus(int from, int dir) const {
  const int n = count();
  int i = from + dir;
  while (i >= 0 && i < n) {
    if (!isDisabled(i))
      return i;
    i += dir;
  }
  return from;
}

void MaterialSegmentedButton::activate(int i) {
  if (i < 0 || i >= count() || isDisabled(i))
    return;

  bool on = true;
  if (m_mode == Single) {
    if (isChecked(i)) {
      QMetaObject::invokeMethod(
          this, [this, i] { emit clicked(i); }, Qt::QueuedConnection);
      return;
    }
    m_checked = {i};
  } else {
    on = !isChecked(i);
    if (on) {
      m_checked.append(i);
      std::sort(m_checked.begin(), m_checked.end());
    } else {
      m_checked.removeAll(i);
    }
  }

  retarget();
  kick(i);
  update();
  emit checkedIndicesChanged();

  QMetaObject::invokeMethod(
      this,
      [this, i, on] {
        emit toggled(i, on);
        emit clicked(i);
      },
      Qt::QueuedConnection);
}

void MaterialSegmentedButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  const int i = hit(e->position());
  if (i < 0 || isDisabled(i)) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  m_pressIndex = m_focusIndex = i;
  retarget();
  update();
  e->accept();
}

void MaterialSegmentedButton::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0)
    return;
  const int pressed = m_pressIndex;
  m_pressIndex = -1;
  const bool inside = hit(e->position()) == pressed;
  retarget();
  update();
  if (inside)
    activate(pressed);
  e->accept();
}

void MaterialSegmentedButton::mouseUngrabEvent() {
  if (m_pressIndex < 0)
    return;
  m_pressIndex = -1;
  retarget();
  update();
}

void MaterialSegmentedButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialSegmentedButton::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (!isEnabled())
    return;
  const int i = hit(e->position());
  const int h = (i >= 0 && !isDisabled(i)) ? i : -1;
  setHand(h >= 0);
  if (h != m_hoverIndex) {
    m_hoverIndex = h;
    update();
  }
}

void MaterialSegmentedButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHand(false);
  m_hoverIndex = -1;
  update();
}

void MaterialSegmentedButton::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || count() == 0) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Left:
    m_focusIndex = stepFocus(m_focusIndex, -1);
    break;
  case Qt::Key_Right:
    m_focusIndex = stepFocus(m_focusIndex, 1);
    break;
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter:
    if (!e->isAutoRepeat())
      activate(m_focusIndex);
    break;
  default:
    e->ignore();
    return;
  }
  m_keyFocus = true;
  update();
  e->accept();
}

void MaterialSegmentedButton::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialSegmentedButton::build(Ryu::Mesh &m) {
  const int n = count();
  if (n == 0 || width() <= 0.0)
    return;

  ensureMetrics();

  const QVector<QRectF> rects = layoutRects();
  const QRectF bounds(rects.first().left(), rects.first().top(),
                      rects.last().right() - rects.first().left(), kHeight);
  const qreal full = kHeight / 2.0;
  const QFont lf = labelFont();
  const qreal cy = bounds.center().y();

  for (int i = 0; i < n; ++i) {
    const QRectF r = rects[i];
    const qreal fill = std::clamp(m_fill.value(i).x, 0.0, 1.0);
    const qreal rl = i == 0 ? full : 0.0;
    const qreal rr = i == n - 1 ? full : 0.0;

    if (m_color.alpha() > 0)
      m.fillRoundedRect(r, rl, rr, rr, rl, m_color);
    if (fill > 0.001)
      m.fillRoundedRect(
          r, rl, rr, rr, rl,
          Ryu::withAlpha(m_checkedColor, m_checkedColor.alphaF() * fill));

    const QColor fg = Ryu::mix(m_textColor, m_checkedTextColor, fill);
    const qreal layer =
        Ryu::stateLayer(m_hoverIndex == i,
                        m_pressIndex == i || (m_keyFocus && hasActiveFocus() &&
                                              m_focusIndex == i));
    if (layer > 0.0)
      m.fillRoundedRect(r, rl, rr, rr, rl, Ryu::withAlpha(fg, layer));
  }

  if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0) {
    const qreal bw = m.snapStroke(m_borderWidth);
    for (int i = 1; i < n; ++i) {
      const qreal x = std::round(rects[i].left()) + 0.5;
      m.line(QPointF(x, bounds.top() + bw / 2.0),
             QPointF(x, bounds.bottom() - bw / 2.0), bw, m_borderColor);
    }
    m.strokeRoundedRect(bounds, full, full, full, full, bw, m_borderColor);
  }

  for (int i = 0; i < n; ++i) {
    const QRectF r = rects[i];
    const qreal fill = std::clamp(m_fill.value(i).x, 0.0, 1.0);
    const qreal chk = std::max(0.0, m_check.value(i).x);
    const qreal chkC = std::min(chk, 1.0);
    const qreal dim = isDisabled(i) ? Ryu::kDisabledOpacity : 1.0;

    const QColor mixed = Ryu::mix(m_textColor, m_checkedTextColor, fill);
    const QColor base = isChecked(i) ? m_checkedTextColor : m_textColor;
    const QColor col = Ryu::withAlpha(base, base.alphaF() * dim);

    const QString lbl = m_labels.value(i);
    const QString ic = m_icons.value(i);
    const bool icon = !ic.isEmpty();
    const qreal tw = m_textW.value(i);

    qreal slotPart = 0.0;
    if (icon)
      slotPart = kIcon;
    else if (m_showCheck)
      slotPart = kIcon * std::min(chk, 1.2);

    qreal gapPart = 0.0;
    if (tw > 0.0 && slotPart > 0.0)
      gapPart = kGap * (icon ? 1.0 : std::min(chk, 1.2));

    const qreal contentW = slotPart + gapPart + tw;
    const qreal x0 = r.center().x() - contentW / 2.0;
    const QPointF c(x0 + kIcon / 2.0, cy);

    if (icon && (!m_showCheck || chkC < 0.5))
      glyphLayer(i * kSlots, m_iconFont, ic, col, c, kIcon);

    if (m_showCheck && chkC > 0.01) {
      if (!m_checkIconSource.isEmpty()) {
        const qreal a =
            std::round(std::clamp((chkC - 0.4) / 0.6, 0.0, 1.0) * 10.0) / 10.0;
        if (a > 0.0) {
          if (m_checkPath.isEmpty())
            m_checkPath = Ryu::resolveUrl(qmlContext(this), m_checkIconSource);
          const qreal dpr = m.dpr();
          const QColor cc = Ryu::withAlpha(
              m_checkedTextColor, m_checkedTextColor.alphaF() * a * dim);
          const QString key = QStringLiteral("c|%1|%2|%3")
                                  .arg(m_checkPath, cc.name(QColor::HexArgb))
                                  .arg(dpr);
          useLayer(
              i * kSlots + 2, key,
              QRectF(m.snap(c.x() - kIcon / 2.0), m.snap(c.y() - kIcon / 2.0),
                     kIcon, kIcon),
              [&] { return m_checkCache.get(m_checkPath, kIcon, dpr, cc); });
        }
      } else {
        const qreal k = kIcon / 18.0;
        const qreal l1 = QLineF(kCheckA, kCheckB).length();
        const qreal l2 = QLineF(kCheckB, kCheckC).length();
        const qreal d = chkC * (l1 + l2);

        auto map = [&](const QPointF &p) { return c + p * k; };

        QList<QPointF> pts;
        pts.append(map(kCheckA));
        if (d <= l1) {
          pts.append(map(kCheckA + (kCheckB - kCheckA) * (d / l1)));
        } else {
          pts.append(map(kCheckB));
          pts.append(map(kCheckB + (kCheckC - kCheckB) * ((d - l1) / l2)));
        }
        m.polyline(pts, kMarkW * k,
                   Ryu::withAlpha(mixed, mixed.alphaF() * chkC * dim));
      }
    }

    textLayer(i * kSlots + 1, lf, lbl, col, x0 + slotPart + gapPart, cy);
  }
}
