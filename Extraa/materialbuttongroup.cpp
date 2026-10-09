#include "./materialbuttongroup.hpp"
#include "../Shapes/springdriver.hpp"
#include <QFontMetricsF>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight[] = {32.0, 40.0, 56.0, 96.0, 136.0};
constexpr qreal kIconSize[] = {20.0, 24.0, 24.0, 32.0, 40.0};
constexpr qreal kFontPx[] = {14.0, 14.0, 16.0, 24.0, 32.0};
constexpr qreal kPad[] = {12.0, 16.0, 24.0, 48.0, 64.0};
constexpr qreal kIconGap[] = {4.0, 8.0, 8.0, 12.0, 16.0};
constexpr qreal kSquareR[] = {12.0, 12.0, 16.0, 28.0, 28.0};
constexpr qreal kInnerR[] = {8.0, 8.0, 8.0, 16.0, 20.0};
constexpr qreal kStdGap[] = {18.0, 12.0, 8.0, 8.0, 8.0};
constexpr qreal kConnectedGap = 2.0;
constexpr qreal kPressGrow = 0.28;
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

MaterialButtonGroup::MaterialButtonGroup(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setActiveFocusOnTab(true);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

  structureChanged();
}

QFont MaterialButtonGroup::labelFont() const {
  QFont f = m_font;
  f.setPixelSize(qRound(kFontPx[m_sizeStyle]));
  f.setWeight(QFont::Weight(m_fontWeight));
  f.setHintingPreference(QFont::PreferNoHinting);
  f.setStyleStrategy(
      QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
  return f;
}

void MaterialButtonGroup::ensureMetrics() const {
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

qreal MaterialButtonGroup::spacingValue() const {
  if (m_spacing >= 0.0)
    return m_spacing;
  return m_variant == Connected ? kConnectedGap : kStdGap[m_sizeStyle];
}

qreal MaterialButtonGroup::itemWidth(int i) const {
  ensureMetrics();
  const bool hasIcon = !m_icons.value(i).isEmpty();
  const qreal tw = m_textW.value(i);
  qreal w = 2.0 * kPad[m_sizeStyle];
  if (hasIcon)
    w += kIconSize[m_sizeStyle];
  if (hasIcon && tw > 0.0)
    w += kIconGap[m_sizeStyle];
  return w + tw;
}

qreal MaterialButtonGroup::naturalWidth() const {
  qreal total = 0.0;
  const int n = count();
  for (int i = 0; i < n; ++i)
    total += itemWidth(i);
  return total + spacingValue() * std::max(0, n - 1);
}

void MaterialButtonGroup::layoutItems() {
  m_layoutDirty = false;
  const int n = count();
  m_rects.assign(n, QRectF());
  if (n == 0)
    return;

  const qreal h = kHeight[m_sizeStyle];
  const qreal gap = spacingValue();
  const qreal total = naturalWidth();
  const qreal extra =
      (m_fillWidth && width() > total) ? (width() - total) / n : 0.0;
  const qreal y = (height() - h) / 2.0;

  qreal x = 0.0;
  for (int i = 0; i < n; ++i) {
    const qreal w = itemWidth(i) + extra;
    m_rects[i] = QRectF(x, y, w, h);
    x += w + gap;
  }
}

QVector<QRectF> MaterialButtonGroup::displayRects() const {
  QVector<QRectF> out = m_rects;
  const qreal gap = spacingValue();
  qreal x = 0.0;
  for (int i = 0; i < out.size(); ++i) {
    const qreal w = std::max(1.0, m_rects[i].width() + m_width.value(i).x);
    out[i] = QRectF(x, m_rects[i].y(), w, m_rects[i].height());
    x += w + gap;
  }
  return out;
}

void MaterialButtonGroup::structureChanged() {
  m_layoutDirty = true;
  m_metricsDirty = true;
  setImplicitWidth(naturalWidth());
  setImplicitHeight(kHeight[m_sizeStyle]);
  m_checked.erase(
      std::remove_if(m_checked.begin(), m_checked.end(),
                     [this](int i) { return i < 0 || i >= count(); }),
      m_checked.end());
  m_focusIndex = std::clamp(m_focusIndex, 0, std::max(0, count() - 1));
  retarget(true);
  update();
}

qreal MaterialButtonGroup::targetRound(int i) const {
  const bool active = isChecked(i) || m_pressIndex == i;
  if (m_variant == Standard)
    return active ? 0.0 : 1.0;
  return active ? 1.0 : 0.0;
}

qreal MaterialButtonGroup::targetWidth(int i) const {
  const int n = count();
  if (m_pressIndex < 0 || n < 2)
    return 0.0;
  const qreal g = kHeight[m_sizeStyle] * kPressGrow;
  if (i == m_pressIndex)
    return g;
  const int nb = (m_pressIndex > 0 ? 1 : 0) + (m_pressIndex < n - 1 ? 1 : 0);
  if (i == m_pressIndex - 1 || i == m_pressIndex + 1)
    return -g / nb;
  return 0.0;
}

void MaterialButtonGroup::retarget(bool immediate) {
  const int n = count();
  if (m_round.size() != n) {
    m_round.resize(n);
    m_width.resize(n);
    immediate = true;
  }
  for (int i = 0; i < n; ++i) {
    m_round[i].t = targetRound(i);
    m_width[i].t = targetWidth(i);
    if (immediate) {
      m_round[i].x = m_round[i].t;
      m_round[i].v = 0.0;
      m_width[i].x = m_width[i].t;
      m_width[i].v = 0.0;
    }
  }
  if (!immediate)
    ensureDriver();
}

void MaterialButtonGroup::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialButtonGroup::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  bool active = false;
  for (int i = 0; i < m_round.size(); ++i) {
    active |= stepSpring(m_round[i].x, m_round[i].v, m_round[i].t, m_stiffness,
                         m_damping, dt, 0.001);
    active |= stepSpring(m_width[i].x, m_width[i].v, m_width[i].t,
                         m_stiffness * 1.2, m_damping, dt, 0.02);
  }
  update();
  if (!active)
    m_driver->stop();
}

void MaterialButtonGroup::setLabels(const QStringList &v) {
  if (assign(m_labels, v, &MaterialButtonGroup::labelsChanged))
    structureChanged();
}

void MaterialButtonGroup::setIcons(const QStringList &v) {
  if (assign(m_icons, v, &MaterialButtonGroup::iconsChanged))
    structureChanged();
}

void MaterialButtonGroup::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialButtonGroup::fontChanged))
    structureChanged();
}

void MaterialButtonGroup::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialButtonGroup::iconFontChanged);
}

void MaterialButtonGroup::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialButtonGroup::fontWeightChanged))
    structureChanged();
}

void MaterialButtonGroup::setVariant(Variant v) {
  if (assign(m_variant, v, &MaterialButtonGroup::variantChanged))
    structureChanged();
}

void MaterialButtonGroup::setSizeStyle(SizeStyle v) {
  if (assign(m_sizeStyle, v, &MaterialButtonGroup::sizeStyleChanged))
    structureChanged();
}

void MaterialButtonGroup::setSelectionMode(SelectionMode v) {
  if (!assign(m_mode, v, &MaterialButtonGroup::selectionModeChanged))
    return;
  if (v == NoSelection)
    m_checked.clear();
  else if (v == Single && m_checked.size() > 1)
    m_checked = {m_checked.first()};
  emit checkedIndicesChanged();
  retarget();
}

void MaterialButtonGroup::setCheckedIndices(const QList<int> &v) {
  QList<int> c = v;
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  c.erase(std::remove_if(c.begin(), c.end(),
                         [this](int i) { return i < 0 || i >= count(); }),
          c.end());
  if (m_mode == NoSelection)
    c.clear();
  if (m_mode == Single && c.size() > 1)
    c = {c.first()};
  if (assign(m_checked, c, &MaterialButtonGroup::checkedIndicesChanged))
    retarget();
}

void MaterialButtonGroup::setCurrentIndex(int i) {
  setCheckedIndices(i < 0 ? QList<int>{} : QList<int>{i});
}

void MaterialButtonGroup::setChecked(int i, bool on) {
  QList<int> c = m_checked;
  if (m_mode == Single)
    c = on ? QList<int>{i} : QList<int>{};
  else if (on && !c.contains(i))
    c.append(i);
  else if (!on)
    c.removeAll(i);
  setCheckedIndices(c);
}

void MaterialButtonGroup::setSpacing(qreal v) {
  if (v < 0.0)
    v = -1.0;
  if (assign(m_spacing, v, &MaterialButtonGroup::spacingChanged))
    structureChanged();
}

void MaterialButtonGroup::setFillWidth(bool v) {
  if (assign(m_fillWidth, v, &MaterialButtonGroup::fillWidthChanged))
    m_layoutDirty = true;
}

void MaterialButtonGroup::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialButtonGroup::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
}

void MaterialButtonGroup::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialButtonGroup::borderWidthChanged);
}

void MaterialButtonGroup::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialButtonGroup::borderColorChanged);
}

void MaterialButtonGroup::setColor(const QColor &v) {
  assign(m_color, v, &MaterialButtonGroup::colorChanged);
}

void MaterialButtonGroup::setCheckedColor(const QColor &v) {
  assign(m_checkedColor, v, &MaterialButtonGroup::checkedColorChanged);
}

void MaterialButtonGroup::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialButtonGroup::textColorChanged);
}

void MaterialButtonGroup::setCheckedTextColor(const QColor &v) {
  assign(m_checkedTextColor, v, &MaterialButtonGroup::checkedTextColorChanged);
}

void MaterialButtonGroup::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickPaintedItem::geometryChange(n, o);
  if (n.size() != o.size())
    m_layoutDirty = true;
}

int MaterialButtonGroup::hit(const QPointF &pt) const {
  const qreal gap = spacingValue();
  for (int i = 0; i < m_rects.size(); ++i)
    if (m_rects[i].adjusted(0, 0, gap, 0).contains(pt))
      return i;
  return -1;
}

void MaterialButtonGroup::activate(int i) {
  if (i < 0 || i >= count() || m_mode == NoSelection) {
    if (i >= 0 && i < count())
      QMetaObject::invokeMethod(
          this, [this, i] { emit clicked(i); }, Qt::QueuedConnection);
    return;
  }

  bool on = true;
  if (m_mode == Single) {
    if (!isChecked(i)) {
      m_checked = {i};
    } else {
      QMetaObject::invokeMethod(
          this, [this, i] { emit clicked(i); }, Qt::QueuedConnection);
      return;
    }
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

void MaterialButtonGroup::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  const int i = hit(e->position());
  if (i < 0) {
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

void MaterialButtonGroup::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0)
    return;
  if (m_layoutDirty)
    layoutItems();
  const int pressed = m_pressIndex;
  m_pressIndex = -1;
  const bool inside = hit(e->position()) == pressed;
  retarget();
  update();
  if (inside)
    activate(pressed);
  e->accept();
}

void MaterialButtonGroup::mouseUngrabEvent() {
  if (m_pressIndex < 0)
    return;
  m_pressIndex = -1;
  retarget();
  update();
}

void MaterialButtonGroup::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || count() == 0) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Left:
    m_focusIndex = std::max(0, m_focusIndex - 1);
    break;
  case Qt::Key_Right:
    m_focusIndex = std::min(count() - 1, m_focusIndex + 1);
    break;
  case Qt::Key_Space:
  case Qt::Key_Return:
  case Qt::Key_Enter:
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

void MaterialButtonGroup::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialButtonGroup::paint(QPainter *p) {
  if (m_layoutDirty)
    layoutItems();
  ensureMetrics();
  const int n = m_rects.size();
  if (n == 0)
    return;

  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const int s = m_sizeStyle;
  const qreal full = kHeight[s] / 2.0;
  const qreal iconSz = kIconSize[s];
  const QFont lf = labelFont();
  const QFontMetricsF fm(lf);
  const QVector<QRectF> rects = displayRects();

  for (int i = 0; i < n; ++i) {
    const QRectF r = rects[i];
    const qreal t = std::clamp(m_round.value(i).x, 0.0, 1.0);
    const bool checked = isChecked(i);

    qreal rl, rr;
    if (m_variant == Standard) {
      rl = rr = kSquareR[s] + (full - kSquareR[s]) * t;
    } else {
      const qreal inner = kInnerR[s] + (full - kInnerR[s]) * t;
      rl = (i == 0) ? full : inner;
      rr = (i == n - 1) ? full : inner;
    }

    QColor bg = checked ? m_checkedColor : m_color;
    if (!bg.isValid())
      bg = checked ? QColor("#6750A4") : QColor("#E8DEF8");

    QColor fg = checked ? m_checkedTextColor : m_textColor;
    if (!fg.isValid())
      fg = checked ? QColor("#FFFFFF") : QColor("#1D192B");

    const QPainterPath path = sidedRect(r, rl, rr);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawPath(path);

    if (m_pressIndex == i ||
        (m_keyFocus && hasActiveFocus() && m_focusIndex == i)) {
      QColor layer = fg;
      layer.setAlphaF(0.10);
      p->setBrush(layer);
      p->drawPath(path);
    }

    if (m_borderWidth > 0.0 && m_borderColor.isValid() &&
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

    const QString lbl = m_labels.value(i);
    const QString ic = m_icons.value(i);
    const qreal lblW = m_textW.value(i);
    const qreal gap = (!ic.isEmpty() && !lbl.isEmpty()) ? kIconGap[s] : 0.0;
    const qreal contentW = (ic.isEmpty() ? 0.0 : iconSz) + gap + lblW;
    qreal x = r.center().x() - contentW / 2.0;

    if (!ic.isEmpty()) {
      QFont f = m_iconFont;
      f.setPixelSize(qRound(iconSz));
      f.setHintingPreference(QFont::PreferNoHinting);

      QPainterPath gp;
      gp.addText(QPointF(0.0, 0.0), f, ic);
      const QRectF br = gp.boundingRect();

      if (!br.isEmpty()) {
        const qreal k =
            std::min(1.0, iconSz / std::max(br.width(), br.height()));
        p->save();
        p->translate(x + iconSz / 2.0, r.center().y());
        p->scale(k, k);
        p->translate(-br.center());
        p->setPen(Qt::NoPen);
        p->setBrush(fg);
        p->drawPath(gp);
        p->restore();
      }
      x += iconSz + gap;
    }

    if (!lbl.isEmpty()) {
      QPainterPath tp;
      tp.addText(QPointF(0.0, 0.0), lf, lbl);
      p->save();
      p->translate(x, r.center().y() + (fm.ascent() - fm.descent()) / 2.0);
      p->setPen(Qt::NoPen);
      p->setBrush(fg);
      p->drawPath(tp);
      p->restore();
    }
  }
}
