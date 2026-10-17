#include "./materialmenu.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPadV = 8.0;
constexpr qreal kPadH = 8.0;
constexpr qreal kItemPad = 12.0;
constexpr qreal kIcon = 20.0;
constexpr qreal kIconGap = 12.0;
constexpr qreal kShortGap = 24.0;
constexpr qreal kFontPx = 14.0;
constexpr qreal kShortPx = 12.0;
constexpr qreal kSepH = 17.0;
constexpr qreal kItemR = 12.0;
} // namespace

MaterialMenu::MaterialMenu(QQuickItem *parent)
    : MaterialItem(parent, 520.0, 0.6) {
  setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  setAcceptHoverEvents(true);
  setTransformOrigin(TopLeft);
  setVisible(false);
  structureChanged();
  applyPop();
}

QFont MaterialMenu::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
}

QFont MaterialMenu::shortcutFont() const {
  return Ryu::styledFont(m_font, kShortPx, QFont::Normal);
}

void MaterialMenu::ensureMetrics() const {
  const int n = m_items.size();
  if (!m_metricsDirty && m_labelW.size() == n)
    return;
  m_metricsDirty = false;
  m_labelW.assign(n, 0.0);
  m_shortW.assign(n, 0.0);
  m_maxLabelW = 0.0;
  m_maxShortW = 0.0;
  m_hasIcon = false;

  const QFontMetricsF lm(labelFont());
  const QFontMetricsF sm(shortcutFont());
  for (int i = 0; i < n; ++i) {
    const Entry &e = m_items[i];
    if (e.separator)
      continue;
    m_labelW[i] = lm.horizontalAdvance(e.label);
    m_shortW[i] = e.shortcut.isEmpty() ? 0.0 : sm.horizontalAdvance(e.shortcut);
    m_maxLabelW = std::max(m_maxLabelW, m_labelW[i]);
    m_maxShortW = std::max(m_maxShortW, m_shortW[i]);
    if (!e.icon.isEmpty())
      m_hasIcon = true;
  }
}

qreal MaterialMenu::naturalWidth() const {
  ensureMetrics();
  qreal w = 2.0 * kPadH + 2.0 * kItemPad + m_maxLabelW;
  if (m_hasIcon)
    w += kIcon + kIconGap;
  if (m_maxShortW > 0.0)
    w += kShortGap + m_maxShortW;
  return std::max(w, m_minWidth);
}

qreal MaterialMenu::naturalHeight() const {
  qreal h = 2.0 * kPadV;
  for (const Entry &e : m_items)
    h += e.separator ? kSepH : m_itemH;
  return h;
}

void MaterialMenu::layoutItems() {
  m_layoutDirty = false;
  const int n = m_items.size();
  m_rects.assign(n, QRectF());
  const qreal w = std::max(0.0, width() - 2.0 * kPadH);
  qreal y = kPadV;
  for (int i = 0; i < n; ++i) {
    const qreal h = m_items[i].separator ? kSepH : m_itemH;
    m_rects[i] = QRectF(kPadH, y, w, h);
    y += h;
  }
}

void MaterialMenu::structureChanged() {
  m_metricsDirty = true;
  m_layoutDirty = true;
  setImplicitWidth(std::ceil(naturalWidth()));
  setImplicitHeight(std::ceil(naturalHeight()));
  update();
}

bool MaterialMenu::selectable(int i) const {
  return i >= 0 && i < m_items.size() && !m_items[i].separator &&
         m_items[i].enabled;
}

int MaterialMenu::rowAt(const QPointF &pt) const {
  for (int i = 0; i < m_rects.size(); ++i)
    if (m_rects[i].adjusted(-kPadH, 0.0, kPadH, 0.0).contains(pt))
      return i;
  return -1;
}

int MaterialMenu::step(int from, int dir) const {
  const int n = m_items.size();
  if (n == 0)
    return -1;
  int i = from;
  if (i < 0)
    i = dir > 0 ? -1 : 0;
  for (int k = 0; k < n; ++k) {
    i = (i + dir + n) % n;
    if (selectable(i))
      return i;
  }
  return from;
}

void MaterialMenu::setCurrent(int i) {
  if (m_current == i)
    return;
  m_current = i;
  emit currentIndexChanged();
  retarget();
  update();
}

void MaterialMenu::trigger(int i) {
  if (!selectable(i))
    return;
  const QVariant item = m_raw.value(i);
  if (m_closeOnTrigger)
    setOpen(false);
  QMetaObject::invokeMethod(
      this, [this, i, item] { emit triggered(i, item); }, Qt::QueuedConnection);
}

void MaterialMenu::retarget() {
  if (m_layoutDirty)
    layoutItems();
  if (m_current >= 0 && m_current < m_rects.size()) {
    const QRectF r = m_rects[m_current];
    if (m_hlA.x < 0.02) {
      m_hlY.x = r.y();
      m_hlH.x = r.height();
      m_hlY.v = 0.0;
      m_hlH.v = 0.0;
    }
    m_hlY.t = r.y();
    m_hlH.t = r.height();
    m_hlA.t = 1.0;
    m_hlBase = m_items[m_current].danger ? m_dangerColor : m_textColor;
  } else {
    m_hlA.t = 0.0;
  }
  animate();
}

void MaterialMenu::applyPop() {
  const qreal p = m_pop.x;
  setScale(0.92 + 0.08 * p);
  setOpacity(std::clamp(p * 1.6, 0.0, 1.0));
}

bool MaterialMenu::advance(qreal dt) {
  const bool popActive = MaterialItem::step(m_pop, dt);
  bool active = popActive;
  active |= MaterialItem::step(m_hlY, dt, 0.05, 1.2);
  active |= MaterialItem::step(m_hlH, dt, 0.05, 1.2);
  active |= Ryu::stepSpring(m_hlA, springStiffness(), 1.0, dt, 0.001);

  applyPop();

  if (!m_open && !popActive)
    setVisible(false);
  return active;
}

void MaterialMenu::popup(qreal x, qreal y) {
  const qreal w = width();
  const qreal h = height();
  TransformOrigin origin = TopLeft;

  if (QQuickItem *p = parentItem()) {
    const qreal m = m_edgeMargin;
    const qreal pw = p->width();
    const qreal ph = p->height();
    const bool flipX = x + w > pw - m;
    const bool flipY = y + h > ph - m;
    if (flipX)
      x -= w;
    if (flipY)
      y -= h;
    x = std::clamp(x, m, std::max(m, pw - w - m));
    y = std::clamp(y, m, std::max(m, ph - h - m));
    origin = flipX ? (flipY ? BottomRight : TopRight)
                   : (flipY ? BottomLeft : TopLeft);
  }

  setTransformOrigin(origin);
  setPosition(QPointF(std::round(x), std::round(y)));
  setOpen(true);
}

void MaterialMenu::setOpen(bool v) {
  if (m_open == v)
    return;
  m_open = v;
  m_pressIndex = -1;

  if (v) {
    setVisible(true);
    m_hlA.x = m_hlA.t = 0.0;
    if (m_current != -1) {
      m_current = -1;
      emit currentIndexChanged();
    }
    forceActiveFocus();
  } else {
    setCurrent(-1);
  }

  m_pop.t = v ? 1.0 : 0.0;
  animate();
  update();
  emit openChanged();
  if (v)
    emit opened();
  else
    emit closed();
}

void MaterialMenu::setModel(const QVariantList &v) {
  m_raw = v;
  m_items.clear();
  m_items.reserve(v.size());

  for (const QVariant &it : v) {
    Entry e;
    if (it.metaType().id() == QMetaType::QString) {
      const QString s = it.toString();
      if (s == QLatin1String("-") || s == QLatin1String("---"))
        e.separator = true;
      else
        e.label = s;
    } else {
      const QVariantMap m = it.toMap();
      e.separator = m.value("separator").toBool();
      e.label = m.value("label").toString();
      e.icon = m.value("icon").toString();
      e.shortcut = m.value("shortcut").toString();
      e.enabled = m.contains("enabled") ? m.value("enabled").toBool() : true;
      e.danger = m.value("danger").toBool();
    }
    m_items.append(e);
  }

  m_pressIndex = -1;
  if (!selectable(m_current) && m_current != -1) {
    m_current = -1;
    m_hlA.t = 0.0;
    emit currentIndexChanged();
  }
  structureChanged();
  emit modelChanged();
}

void MaterialMenu::setCloseOnTrigger(bool v) {
  assign(m_closeOnTrigger, v, &MaterialMenu::closeOnTriggerChanged);
}

void MaterialMenu::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialMenu::fontChanged))
    structureChanged();
}

void MaterialMenu::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialMenu::fontWeightChanged))
    structureChanged();
}

void MaterialMenu::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialMenu::iconFontChanged);
}

void MaterialMenu::setItemHeight(qreal v) {
  v = std::max(24.0, v);
  if (assign(m_itemH, v, &MaterialMenu::itemHeightChanged))
    structureChanged();
}

void MaterialMenu::setRadius(qreal v) {
  assign(m_radius, std::max(0.0, v), &MaterialMenu::radiusChanged);
}

void MaterialMenu::setMinimumWidth(qreal v) {
  if (assign(m_minWidth, std::max(0.0, v), &MaterialMenu::minimumWidthChanged))
    structureChanged();
}

void MaterialMenu::setEdgeMargin(qreal v) {
  assign(m_edgeMargin, std::max(0.0, v), &MaterialMenu::edgeMarginChanged);
}

void MaterialMenu::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v), &MaterialMenu::borderWidthChanged);
}

void MaterialMenu::setColor(const QColor &v) {
  assign(m_color, v, &MaterialMenu::colorChanged);
}

void MaterialMenu::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialMenu::textColorChanged);
}

void MaterialMenu::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialMenu::iconColorChanged);
}

void MaterialMenu::setShortcutColor(const QColor &v) {
  assign(m_shortcutColor, v, &MaterialMenu::shortcutColorChanged);
}

void MaterialMenu::setDangerColor(const QColor &v) {
  assign(m_dangerColor, v, &MaterialMenu::dangerColorChanged);
}

void MaterialMenu::setSeparatorColor(const QColor &v) {
  assign(m_separatorColor, v, &MaterialMenu::separatorColorChanged);
}

void MaterialMenu::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialMenu::borderColorChanged);
}

void MaterialMenu::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickPaintedItem::geometryChange(n, o);
  if (n.size() != o.size())
    m_layoutDirty = true;
}

void MaterialMenu::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  forceActiveFocus(Qt::MouseFocusReason);
  const int i = rowAt(e->position());
  m_pressIndex = selectable(i) ? i : -1;
  if (m_pressIndex >= 0)
    setCurrent(m_pressIndex);
  update();
  e->accept();
}

void MaterialMenu::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0) {
    e->accept();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  const int pressed = m_pressIndex;
  m_pressIndex = -1;
  const bool inside = rowAt(e->position()) == pressed;
  update();
  if (inside)
    trigger(pressed);
  e->accept();
}

void MaterialMenu::mouseUngrabEvent() {
  m_pressIndex = -1;
  update();
}

void MaterialMenu::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialMenu::hoverMoveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverMoveEvent(e);
  if (!isEnabled() || !m_open)
    return;
  if (m_layoutDirty)
    layoutItems();
  const int i = rowAt(e->position());
  setCurrent(selectable(i) ? i : -1);
}

void MaterialMenu::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setCurrent(-1);
}

void MaterialMenu::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || !m_open) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Down:
    setCurrent(step(m_current, 1));
    break;
  case Qt::Key_Up:
    setCurrent(step(m_current, -1));
    break;
  case Qt::Key_Home:
    setCurrent(step(-1, 1));
    break;
  case Qt::Key_End:
    setCurrent(step(-1, -1));
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
  case Qt::Key_Space:
    if (m_current >= 0)
      trigger(m_current);
    break;
  case Qt::Key_Escape:
    setOpen(false);
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialMenu::paint(QPainter *p) {
  if (m_layoutDirty)
    layoutItems();
  ensureMetrics();

  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  preparePainter(p);

  QPainterPath card;
  card.addRoundedRect(QRectF(0.0, 0.0, w, h), m_radius, m_radius);

  p->setPen(Qt::NoPen);
  p->setBrush(m_color);
  p->drawPath(card);

  if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0)
    Ryu::strokeInside(p, card, m_borderColor, m_borderWidth);

  const qreal a = std::clamp(m_hlA.x, 0.0, 1.0);
  if (a > 0.001 && !m_rects.isEmpty()) {
    const bool pressed = m_pressIndex >= 0 && m_pressIndex == m_current;
    p->setBrush(Ryu::withAlpha(
        m_hlBase, (pressed ? Ryu::kPressLayer : Ryu::kHoverLayer) * a));
    p->drawRoundedRect(
        QRectF(kPadH, m_hlY.x, w - 2.0 * kPadH, std::max(0.0, m_hlH.x)), kItemR,
        kItemR);
  }

  const QFont lf = labelFont();
  const QFont sf = shortcutFont();
  const qreal iconCol = m_hasIcon ? kIcon + kIconGap : 0.0;

  for (int i = 0; i < m_items.size(); ++i) {
    const Entry &e = m_items[i];
    const QRectF r = m_rects.value(i);

    if (e.separator) {
      const qreal y = std::floor(r.center().y()) + 0.5;
      p->setPen(QPen(m_separatorColor, 1.0));
      p->drawLine(QPointF(r.left() + kItemPad, y),
                  QPointF(r.right() - kItemPad, y));
      p->setPen(Qt::NoPen);
      continue;
    }

    const QColor fg = e.danger ? m_dangerColor : m_textColor;
    const QColor ic = e.danger
                          ? m_dangerColor
                          : (m_iconColor.isValid() ? m_iconColor : m_textColor);
    const QColor sc = e.danger ? m_dangerColor : m_shortcutColor;
    const qreal cy = r.center().y();

    p->save();
    if (!e.enabled)
      p->setOpacity(p->opacity() * Ryu::kDisabledOpacity);

    qreal x = r.left() + kItemPad;
    if (!e.icon.isEmpty())
      Ryu::drawGlyph(p, m_iconFont, e.icon, QPointF(x + kIcon / 2.0, cy), kIcon,
                     ic);
    x += iconCol;

    Ryu::drawLabel(p, lf, e.label, x, cy, fg);
    Ryu::drawLabel(p, sf, e.shortcut, r.right() - kItemPad - m_shortW.value(i),
                   cy, sc);

    p->restore();
  }
}
