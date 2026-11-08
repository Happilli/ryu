#include "./materiallist.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight[] = {56.0, 56.0, 72.0, 88.0};
constexpr qreal kPad = 16.0;
constexpr qreal kGap = 16.0;
constexpr qreal kIcon = 24.0;
constexpr qreal kAvatar = 40.0;
constexpr qreal kImage = 56.0;
constexpr qreal kImageR = 8.0;
constexpr qreal kOuterR = 16.0;
constexpr qreal kInnerR = 4.0;
constexpr qreal kSegGap = 2.0;
constexpr qreal kSegDivider = 8.0;
constexpr qreal kDivider = 1.0;
constexpr qreal kOverlineLh = 16.0;
constexpr qreal kHeadlineLh = 24.0;
constexpr qreal kSupportLh = 20.0;
constexpr qreal kChevronHalf = 5.0;
constexpr qreal kChevronZone = 56.0;
constexpr qreal kChevronGap = 8.0;
constexpr qreal kChevronLayerR = 20.0;
constexpr qreal kChevronW = 2.0;

constexpr int kSlots = 8;
constexpr int kSlotImage = 0;
constexpr int kSlotAvatar = 1;
constexpr int kSlotLeading = 2;
constexpr int kSlotTrailingIcon = 3;
constexpr int kSlotTrailingText = 4;
constexpr int kSlotOverline = 5;
constexpr int kSlotHeadline = 6;
constexpr int kSlotSupporting = 7;

QImage loadSquare(const QString &path) {
  QImageReader r(path);
  const QImage in = r.read();
  if (in.isNull()) {
    qWarning("MaterialList: failed to load image '%s'", qPrintable(path));
    return {};
  }
  constexpr int px = int(kImage) * 2;
  const QImage scaled = in.scaled(px, px, Qt::KeepAspectRatioByExpanding,
                                  Qt::SmoothTransformation);
  return scaled.copy((scaled.width() - px) / 2, (scaled.height() - px) / 2, px,
                     px);
}

bool isInside(const QQuickItem *ancestor, const QQuickItem *item) {
  for (const QQuickItem *p = item; p; p = p->parentItem())
    if (p == ancestor)
      return true;
  return false;
}
} // namespace

MaterialList::MaterialList(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.6) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setImplicitWidth(360.0);
  structureChanged();
}

QColor MaterialList::resolvedItemColor() const {
  if (m_itemColor.isValid())
    return m_itemColor;
  return m_variant == Segmented ? QColor("#F3EDF7") : QColor(Qt::transparent);
}

qreal MaterialList::itemGap() const {
  return m_variant == Segmented ? kSegGap : 0.0;
}

qreal MaterialList::headerHeight(const Entry &e) const {
  return kHeight[std::min(e.lines(), 3)];
}

qreal MaterialList::contentNatural(int i) const {
  if (i < 0 || i >= m_rows.size() || !m_rows[i].content)
    return 0.0;
  const QQuickItem *c = m_rows[i].content;
  const qreal h = c->implicitHeight() > 0.0 ? c->implicitHeight() : c->height();
  return std::max(0.0, h) + m_contentPad;
}

qreal MaterialList::rowHeight(int i) const {
  const Entry &e = m_items[i];
  if (e.divider)
    return m_variant == Segmented ? kSegDivider : kDivider;
  qreal h = headerHeight(e);
  if (i < m_rows.size() && expandable(i))
    h += std::max(0.0, m_rows[i].open.x) * contentNatural(i);
  return h;
}

qreal MaterialList::naturalHeight() const {
  qreal h = 0.0;
  for (int i = 0; i < m_items.size(); ++i)
    h += rowHeight(i) + (m_items[i].divider ? 0.0 : itemGap());
  if (!m_items.isEmpty() && !m_items.last().divider)
    h -= itemGap();
  return std::max(0.0, h);
}

QRectF MaterialList::headerRect(int i) const {
  QRectF r = m_rects.value(i);
  if (i >= 0 && i < m_items.size() && !m_items[i].divider)
    r.setHeight(headerHeight(m_items[i]));
  return r;
}

void MaterialList::layoutItems() {
  m_layoutDirty = false;
  const int n = m_items.size();
  m_rects.assign(n, QRectF());
  qreal y = 0.0;
  for (int i = 0; i < n; ++i) {
    const qreal h = rowHeight(i);
    m_rects[i] = QRectF(0.0, y, width(), h);
    y += h + (m_items[i].divider ? 0.0 : itemGap());
  }
}

void MaterialList::relayout() {
  if (m_relaying) {
    m_relayAgain = true;
    return;
  }
  m_relaying = true;
  for (int pass = 0; pass < 4; ++pass) {
    m_relayAgain = false;
    m_layoutDirty = true;
    setImplicitHeight(std::ceil(naturalHeight()));
    layoutItems();
    positionContent();
    if (!m_relayAgain)
      break;
  }
  m_relaying = false;
  update();
}

void MaterialList::positionContent() {
  if (m_layoutDirty)
    layoutItems();
  for (int i = 0; i < m_rows.size(); ++i) {
    Row &r = m_rows[i];
    if (!r.clip || !r.content)
      continue;
    if (i >= m_rects.size() || !expandable(i)) {
      r.clip->setVisible(false);
      continue;
    }
    const QRectF rect = m_rects[i];
    const qreal hh = headerHeight(m_items[i]);
    const qreal ch = std::max(0.0, rect.height() - hh);
    const bool vis = (r.expanded || r.open.x > 0.0005) && ch > 0.01;
    r.clip->setEnabled(m_items[i].enabled);
    r.clip->setPosition(QPointF(rect.left(), rect.top() + hh));
    r.clip->setSize(QSizeF(rect.width(), ch));
    r.clip->setOpacity(std::clamp(r.open.x * 1.5, 0.0, 1.0));
    r.clip->setVisible(vis);
    r.content->setPosition(QPointF(m_contentPad, 0.0));
    r.content->setWidth(std::max(0.0, rect.width() - 2.0 * m_contentPad));
  }
}

void MaterialList::structureChanged() {
  m_checked.erase(std::remove_if(m_checked.begin(), m_checked.end(),
                                 [this](int i) { return !selectable(i); }),
                  m_checked.end());
  if (!selectable(m_focusIndex))
    m_focusIndex = step(-1, 1);
  relayout();
}

bool MaterialList::selectable(int i) const {
  return i >= 0 && i < m_items.size() && !m_items[i].divider &&
         m_items[i].enabled;
}

bool MaterialList::expandable(int i) const {
  if (i < 0 || i >= m_items.size())
    return false;
  const Entry &e = m_items[i];
  return !e.divider && e.expandable && (e.delegate || m_delegate);
}

bool MaterialList::canToggle(int i) const {
  return expandable(i) && m_items[i].enabled;
}

MaterialList::ExpandTrigger MaterialList::effectiveTrigger() const {
  if (m_trigger != Auto)
    return m_trigger;
  return m_mode == NoSelection ? Header : Chevron;
}

bool MaterialList::chevronZone(int i, const QPointF &pt) const {
  if (!expandable(i) || i >= m_rects.size())
    return false;
  const QRectF h = headerRect(i);
  return h.contains(pt) && pt.x() >= h.right() - kChevronZone;
}

int MaterialList::rowAt(const QPointF &pt) const {
  const int n = std::min<int>(m_rects.size(), m_items.size());
  for (int i = 0; i < n; ++i) {
    QRectF r = headerRect(i);
    const bool open = i < m_rows.size() && m_rows[i].open.x > 0.001;
    if (!(expandable(i) && open))
      r.adjust(0, 0, 0, itemGap());
    if (r.contains(pt))
      return i;
  }
  return -1;
}

int MaterialList::step(int from, int dir) const {
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

bool MaterialList::contentHasFocus() const {
  if (!window())
    return false;
  const QQuickItem *f = window()->activeFocusItem();
  return f && f != this && isInside(this, f);
}

QQmlComponent *MaterialList::delegateFor(int i) const {
  if (i < 0 || i >= m_items.size())
    return nullptr;
  if (m_items[i].delegate)
    return m_items[i].delegate.data();
  return m_delegate.data();
}

void MaterialList::ensureContent(int i) {
  if (i < 0 || i >= m_rows.size())
    return;
  Row &r = m_rows[i];
  if (r.content)
    return;

  QQmlComponent *comp = delegateFor(i);
  if (!comp || comp->status() != QQmlComponent::Ready)
    return;

  QQmlContext *parentCtx = comp->creationContext();
  if (!parentCtx)
    parentCtx = qmlContext(this);
  if (!parentCtx && qmlEngine(this))
    parentCtx = qmlEngine(this)->rootContext();
  if (!parentCtx)
    return;

  auto *clip = new QQuickItem(this);
  clip->setClip(true);
  clip->setVisible(false);

  const QVariant item = m_raw.value(i);
  auto *ctx = new QQmlContext(parentCtx, clip);
  ctx->setContextProperty(QStringLiteral("index"), i);
  ctx->setContextProperty(QStringLiteral("modelData"), item);

  QObject *obj = comp->beginCreate(ctx);
  if (!obj) {
    qWarning("MaterialList: failed to create content: %s",
             qPrintable(comp->errorString()));
    delete clip;
    return;
  }

  QVariantMap init;
  const QMetaObject *mo = obj->metaObject();
  if (mo->indexOfProperty("index") >= 0)
    init.insert(QStringLiteral("index"), i);
  if (mo->indexOfProperty("modelData") >= 0)
    init.insert(QStringLiteral("modelData"), item);
  if (!init.isEmpty())
    comp->setInitialProperties(obj, init);

  auto *it = qobject_cast<QQuickItem *>(obj);
  if (!it) {
    comp->completeCreate();
    qWarning("MaterialList: content delegate root must be an Item");
    delete obj;
    delete clip;
    return;
  }

  it->setParent(clip);
  it->setParentItem(clip);
  comp->completeCreate();

  r.clip = clip;
  r.content = it;

  connect(it, &QQuickItem::implicitHeightChanged, this, [this] { relayout(); });
  connect(it, &QQuickItem::heightChanged, this, [this] { relayout(); });
}

void MaterialList::destroyContent(int i) {
  if (i < 0 || i >= m_rows.size())
    return;
  Row &r = m_rows[i];
  if (r.clip) {
    r.clip->setVisible(false);
    r.clip->setParentItem(nullptr);
    r.clip->deleteLater();
  }
  r.clip = nullptr;
  r.content = nullptr;
}

void MaterialList::destroyAllContent() {
  if (contentHasFocus())
    forceActiveFocus(Qt::OtherFocusReason);
  for (int i = 0; i < m_rows.size(); ++i)
    destroyContent(i);
}

bool MaterialList::applyExpanded(int i, bool on) {
  if (i < 0 || i >= m_rows.size())
    return false;
  Row &r = m_rows[i];
  if (r.expanded == on)
    return false;
  r.expanded = on;

  if (on) {
    ensureContent(i);
  } else if (r.clip && window()) {
    const QQuickItem *f = window()->activeFocusItem();
    if (f && isInside(r.clip, f))
      forceActiveFocus(Qt::OtherFocusReason);
  }

  const bool immediate =
      !m_animated || !isComponentComplete() || !window() || !isVisible();
  r.open.go(on ? 1.0 : 0.0, immediate);
  emit expandedChanged(i, on);
  return true;
}

void MaterialList::finishExpansion() {
  relayout();
  animate();
  emit expandedIndicesChanged();
  update();
}

QList<int> MaterialList::expandedIndices() const {
  QList<int> out;
  for (int i = 0; i < m_rows.size(); ++i)
    if (m_rows[i].expanded)
      out.append(i);
  return out;
}

bool MaterialList::isExpanded(int i) const {
  return i >= 0 && i < m_rows.size() && m_rows[i].expanded;
}

void MaterialList::setExpanded(int i, bool on) {
  if (i < 0 || i >= m_rows.size())
    return;
  if (on && !canToggle(i))
    return;
  if (!on && !m_rows[i].expanded)
    return;
  bool changed = false;
  if (on && m_singleExpand)
    for (int j = 0; j < m_rows.size(); ++j)
      if (j != i)
        changed |= applyExpanded(j, false);
  changed |= applyExpanded(i, on);
  if (changed)
    finishExpansion();
}

void MaterialList::toggleExpanded(int i) { setExpanded(i, !isExpanded(i)); }

void MaterialList::expandAll() {
  bool changed = false;
  for (int i = 0; i < m_rows.size(); ++i) {
    if (!canToggle(i))
      continue;
    changed |= applyExpanded(i, true);
    if (m_singleExpand)
      break;
  }
  if (changed)
    finishExpansion();
}

void MaterialList::collapseAll() {
  bool changed = false;
  for (int i = 0; i < m_rows.size(); ++i)
    changed |= applyExpanded(i, false);
  if (changed)
    finishExpansion();
}

QQuickItem *MaterialList::contentItem(int i) const {
  if (i < 0 || i >= m_rows.size())
    return nullptr;
  return m_rows[i].content;
}

bool MaterialList::advance(qreal dt) {
  bool active = SgMaterialItem::step(m_press, dt);
  if (!active && m_press.t == 0.0)
    m_pressShown = -1;

  bool moving = false;
  for (Row &r : m_rows)
    if (SgMaterialItem::step(r.open, dt, 0.001))
      moving = true;

  if (moving || m_wasMoving)
    relayout();
  m_wasMoving = moving;
  return active || moving;
}

void MaterialList::setModel(const QVariantList &v) {
  destroyAllContent();

  m_raw = v;
  m_items.clear();
  m_items.reserve(v.size());
  ++m_gen;

  for (const QVariant &it : v) {
    Entry e;
    if (it.metaType().id() == QMetaType::QString) {
      const QString s = it.toString();
      if (s == QLatin1String("-") || s == QLatin1String("---"))
        e.divider = true;
      else
        e.headline = s;
    } else {
      const QVariantMap m = it.toMap();
      e.divider = m.value("divider").toBool();
      e.overline = m.value("overline").toString();
      e.headline = m.value("headline").toString();
      e.supporting = m.value("supporting").toString();
      e.trailingText = m.value("trailingText").toString();
      e.leadingIcon = m.value("leadingIcon").toString();
      e.trailingIcon = m.value("trailingIcon").toString();
      e.avatarText = m.value("avatarText").toString();
      e.enabled = m.contains("enabled") ? m.value("enabled").toBool() : true;
      e.expandable = m.value("expandable").toBool();
      e.expanded = m.value("expanded").toBool();
      e.delegate = qobject_cast<QQmlComponent *>(
          qvariant_cast<QObject *>(m.value("delegate")));
      if (e.delegate)
        e.expandable = true;
      const QString img = m.value("image").toString();
      if (!img.isEmpty())
        e.image = loadSquare(Ryu::resolveUrl(qmlContext(this), QUrl(img)));
    }
    m_items.append(e);
  }

  m_rows.clear();
  m_rows.resize(m_items.size());
  bool any = false;
  for (int i = 0; i < m_items.size(); ++i) {
    const Entry &e = m_items[i];
    if (!e.expanded || !e.expandable || e.divider || !e.enabled)
      continue;
    if (m_singleExpand && any)
      continue;
    any = true;
    m_rows[i].expanded = true;
    m_rows[i].open.go(1.0, true);
    ensureContent(i);
  }

  m_pressIndex = m_pressShown = m_hoverIndex = -1;
  m_pressChevron = false;
  m_hoverChevron = false;
  m_wasMoving = false;
  const QList<int> before = m_checked;
  structureChanged();
  emit modelChanged();
  if (before != m_checked)
    emit checkedIndicesChanged();
  emit expandedIndicesChanged();
}

void MaterialList::setVariant(Variant v) {
  if (assign(m_variant, v, &MaterialList::variantChanged))
    structureChanged();
}

void MaterialList::setSelectionMode(SelectionMode v) {
  if (!assign(m_mode, v, &MaterialList::selectionModeChanged))
    return;
  if (v == NoSelection)
    m_checked.clear();
  else if (v == Single && m_checked.size() > 1)
    m_checked = {m_checked.first()};
  emit checkedIndicesChanged();
}

void MaterialList::setCheckedIndices(const QList<int> &v) {
  QList<int> c = v;
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  c.erase(std::remove_if(c.begin(), c.end(),
                         [this](int i) { return !selectable(i); }),
          c.end());
  if (m_mode == NoSelection)
    c.clear();
  if (m_mode == Single && c.size() > 1)
    c = {c.first()};
  assign(m_checked, c, &MaterialList::checkedIndicesChanged);
}

void MaterialList::setCurrentIndex(int i) {
  setCheckedIndices(i < 0 ? QList<int>{} : QList<int>{i});
}

void MaterialList::setChecked(int i, bool on) {
  QList<int> c = m_checked;
  if (m_mode == Single)
    c = on ? QList<int>{i} : QList<int>{};
  else if (on && !c.contains(i))
    c.append(i);
  else if (!on)
    c.removeAll(i);
  setCheckedIndices(c);
}

void MaterialList::setContentDelegate(QQmlComponent *c) {
  if (m_delegate == c)
    return;
  m_delegate = c;
  destroyAllContent();
  for (int i = 0; i < m_rows.size(); ++i)
    if (m_rows[i].expanded)
      ensureContent(i);
  emit contentDelegateChanged();
  structureChanged();
}

void MaterialList::setExpandTrigger(ExpandTrigger v) {
  if (assign(m_trigger, v, &MaterialList::expandTriggerChanged))
    m_hoverChevron = false;
}

void MaterialList::setSingleExpand(bool v) {
  if (!assign(m_singleExpand, v, &MaterialList::singleExpandChanged))
    return;
  if (!v)
    return;
  bool seen = false;
  bool changed = false;
  for (int i = 0; i < m_rows.size(); ++i) {
    if (!m_rows[i].expanded)
      continue;
    if (seen)
      changed |= applyExpanded(i, false);
    else
      seen = true;
  }
  if (changed)
    finishExpansion();
}

void MaterialList::setExpandedIndices(const QList<int> &v) {
  QList<int> c = v;
  std::sort(c.begin(), c.end());
  c.erase(std::unique(c.begin(), c.end()), c.end());
  c.erase(std::remove_if(c.begin(), c.end(),
                         [this](int i) { return !canToggle(i); }),
          c.end());
  if (m_singleExpand && c.size() > 1)
    c = {c.first()};
  bool changed = false;
  for (int i = 0; i < m_rows.size(); ++i)
    changed |= applyExpanded(i, c.contains(i));
  if (changed)
    finishExpansion();
}

void MaterialList::setContentPadding(qreal v) {
  if (assign(m_contentPad, std::max(0.0, v),
             &MaterialList::contentPaddingChanged))
    relayout();
}

void MaterialList::setAnimated(bool v) {
  assign(m_animated, v, &MaterialList::animatedChanged);
}

void MaterialList::setShowChevron(bool v) {
  assign(m_showChevron, v, &MaterialList::showChevronChanged);
}

void MaterialList::setChevronColor(const QColor &v) {
  assign(m_chevronColor, v, &MaterialList::chevronColorChanged);
}

void MaterialList::setFont(const QFont &v) {
  assign(m_font, v, &MaterialList::fontChanged);
}
void MaterialList::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialList::iconFontChanged);
}
void MaterialList::setItemColor(const QColor &v) {
  assign(m_itemColor, v, &MaterialList::itemColorChanged);
}
void MaterialList::setSelectedColor(const QColor &v) {
  assign(m_selectedColor, v, &MaterialList::selectedColorChanged);
}
void MaterialList::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialList::textColorChanged);
}
void MaterialList::setSelectedTextColor(const QColor &v) {
  assign(m_selectedTextColor, v, &MaterialList::selectedTextColorChanged);
}
void MaterialList::setSupportingColor(const QColor &v) {
  assign(m_supportingColor, v, &MaterialList::supportingColorChanged);
}
void MaterialList::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialList::iconColorChanged);
}
void MaterialList::setAvatarColor(const QColor &v) {
  assign(m_avatarColor, v, &MaterialList::avatarColorChanged);
}
void MaterialList::setAvatarTextColor(const QColor &v) {
  assign(m_avatarTextColor, v, &MaterialList::avatarTextColorChanged);
}
void MaterialList::setDividerColor(const QColor &v) {
  assign(m_dividerColor, v, &MaterialList::dividerColorChanged);
}

void MaterialList::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickItem::geometryChange(n, o);
  if (n.width() != o.width())
    relayout();
  else if (n.size() != o.size())
    update();
}

void MaterialList::activate(int i) {
  if (!selectable(i))
    return;
  const QVariant item = m_raw.value(i);

  if (m_mode == Single) {
    if (!isChecked(i)) {
      m_checked = {i};
      emit checkedIndicesChanged();
    }
  } else if (m_mode == Multiple) {
    if (isChecked(i))
      m_checked.removeAll(i);
    else {
      m_checked.append(i);
      std::sort(m_checked.begin(), m_checked.end());
    }
    emit checkedIndicesChanged();
  }

  if (canToggle(i) && effectiveTrigger() == Header)
    toggleExpanded(i);

  update();

  QMetaObject::invokeMethod(
      this, [this, i, item] { emit clicked(i, item); }, Qt::QueuedConnection);
}

void MaterialList::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  const int i = rowAt(e->position());
  if (!selectable(i)) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  m_pressChevron =
      chevronZone(i, e->position()) && effectiveTrigger() == Chevron;
  m_pressIndex = m_focusIndex = i;
  if (!m_pressChevron) {
    m_pressShown = i;
    m_press.go(1.0);
    animate();
  }
  update();
  e->accept();
}

void MaterialList::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0)
    return;
  if (m_layoutDirty)
    layoutItems();
  const int pressed = m_pressIndex;
  const bool chev = m_pressChevron;
  m_pressIndex = -1;
  m_pressChevron = false;
  m_press.go(0.0);
  animate();
  const bool onChev =
      chevronZone(pressed, e->position()) && effectiveTrigger() == Chevron;
  const bool inside = rowAt(e->position()) == pressed && onChev == chev;
  update();
  if (inside) {
    if (chev)
      toggleExpanded(pressed);
    else
      activate(pressed);
  }
  e->accept();
}

void MaterialList::mouseUngrabEvent() {
  m_pressChevron = false;
  if (m_pressIndex < 0)
    return;
  m_pressIndex = -1;
  m_press.go(0.0);
  animate();
  update();
}

void MaterialList::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialList::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (!isEnabled())
    return;
  if (m_layoutDirty)
    layoutItems();
  const int i = rowAt(e->position());
  const int h = selectable(i) ? i : -1;
  const bool chev =
      h >= 0 && chevronZone(h, e->position()) && effectiveTrigger() == Chevron;
  if (h != m_hoverIndex || chev != m_hoverChevron) {
    m_hoverIndex = h;
    m_hoverChevron = chev;
    update();
  }
}

void MaterialList::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  m_hoverIndex = -1;
  m_hoverChevron = false;
  update();
}

void MaterialList::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || m_items.isEmpty() || contentHasFocus()) {
    e->ignore();
    return;
  }
  switch (e->key()) {
  case Qt::Key_Down:
    m_focusIndex = step(m_focusIndex, 1);
    break;
  case Qt::Key_Up:
    m_focusIndex = step(m_focusIndex, -1);
    break;
  case Qt::Key_Home:
    m_focusIndex = step(-1, 1);
    break;
  case Qt::Key_End:
    m_focusIndex = step(-1, -1);
    break;
  case Qt::Key_Right:
    if (!canToggle(m_focusIndex)) {
      e->ignore();
      return;
    }
    setExpanded(m_focusIndex, true);
    break;
  case Qt::Key_Left:
    if (!expandable(m_focusIndex)) {
      e->ignore();
      return;
    }
    setExpanded(m_focusIndex, false);
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

void MaterialList::focusInEvent(QFocusEvent *e) {
  QQuickItem::focusInEvent(e);
  if (!selectable(m_focusIndex))
    m_focusIndex = step(-1, 1);
}

void MaterialList::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialList::build(Ryu::Mesh &m) {
  if (m_layoutDirty)
    layoutItems();
  const int n = m_items.size();
  if (n == 0 || width() <= 0.0)
    return;

  const qreal dpr = m.dpr();
  const QFont headF = Ryu::styledFont(m_font, 16.0, QFont::Normal);
  const QFont supF = Ryu::styledFont(m_font, 14.0, QFont::Normal);
  const QFont ovF = Ryu::styledFont(m_font, 11.0, QFont::Medium);
  const QFont trailF = ovF;
  const QFont avatarF = Ryu::styledFont(m_font, 16.0, QFont::Medium);
  const QColor base = resolvedItemColor();
  const QColor iconBase =
      m_iconColor.isValid() ? m_iconColor : m_supportingColor;
  const ExpandTrigger trig = effectiveTrigger();

  for (int i = 0; i < n; ++i) {
    const Entry &e = m_items[i];
    const QRectF r = m_rects[i];

    if (e.divider) {
      if (m_variant == Standard) {
        const qreal y = std::floor(r.center().y()) + 0.5;
        m.line(QPointF(r.left() + kPad, y), QPointF(r.right() - kPad, y),
               m.snapStroke(1.0), m_dividerColor);
      }
      continue;
    }

    const bool checked = isChecked(i);
    const bool exp = expandable(i);
    const qreal open = (exp && i < m_rows.size()) ? m_rows[i].open.x : 0.0;
    const QRectF hr = headerRect(i);
    const qreal dim = e.enabled ? 1.0 : Ryu::kDisabledOpacity;
    auto d = [dim](const QColor &c) {
      return Ryu::withAlpha(c, c.alphaF() * dim);
    };

    qreal rt = 0.0, rb = 0.0;
    if (m_variant == Segmented) {
      const bool first = i == 0 || m_items[i - 1].divider;
      const bool last = i == n - 1 || m_items[i + 1].divider;
      rt = first ? kOuterR : kInnerR;
      rb = last ? kOuterR : kInnerR;
      if (m_pressShown == i) {
        const qreal t = std::clamp(m_press.x, 0.0, 1.0);
        rt += (kOuterR - rt) * t;
        rb += (kOuterR - rb) * t;
      }
    }

    const QColor bg = checked ? m_selectedColor : base;
    const QColor fg = checked ? m_selectedTextColor : m_textColor;
    const QColor sup = checked ? m_selectedTextColor : m_supportingColor;
    const QColor ic = checked ? m_selectedTextColor : iconBase;

    if (bg.alpha() > 0)
      m.fillRoundedRect(r, rt, rt, rb, rb, d(bg));

    const bool chevOnly = exp && trig == Chevron;
    const bool hoverRow = m_hoverIndex == i && !(chevOnly && m_hoverChevron);
    const bool pressRow = m_pressIndex == i && !m_pressChevron;
    const bool keyRow = m_keyFocus && hasActiveFocus() && m_focusIndex == i;
    const qreal layer = Ryu::stateLayer(hoverRow, pressRow || keyRow);
    if (layer > 0.0) {
      const qreal rbH = open > 0.001 ? 0.0 : rb;
      m.fillRoundedRect(hr, rt, rt, rbH, rbH, d(Ryu::withAlpha(fg, layer)));
    }

    const int slot = i * kSlots;
    const qreal cy = hr.center().y();
    qreal xl = r.left() + kPad;
    qreal xr = r.right() - kPad;

    if (!e.image.isNull()) {
      const QString key = QStringList{"img",
                                      QString::number(m_gen),
                                      QString::number(i),
                                      QString::number(dim),
                                      QString::number(fade()),
                                      QString::number(dpr)}
                              .join(QLatin1Char('|'));
      useLayer(slot + kSlotImage, key,
               QRectF(m.snap(xl), m.snap(cy - kImage / 2.0), kImage, kImage),
               [&] {
                 QImage img = Ryu::newImage(kImage, kImage, dpr);
                 QPainter p(&img);
                 p.setRenderHint(QPainter::Antialiasing, true);
                 p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                 p.setOpacity(fade() * dim);
                 QPainterPath clip;
                 clip.addRoundedRect(QRectF(0.0, 0.0, kImage, kImage), kImageR,
                                     kImageR);
                 p.setClipPath(clip);
                 p.drawImage(QRectF(0.0, 0.0, kImage, kImage), e.image);
                 p.end();
                 return img;
               });
      xl += kImage + kGap;
    } else if (!e.avatarText.isEmpty()) {
      m.fillCircle(QPointF(xl + kAvatar / 2.0, cy), kAvatar / 2.0,
                   d(m_avatarColor));
      const qreal tw = QFontMetricsF(avatarF).horizontalAdvance(e.avatarText);
      textLayer(slot + kSlotAvatar, avatarF, e.avatarText, d(m_avatarTextColor),
                xl + kAvatar / 2.0 - tw / 2.0, cy);
      xl += kAvatar + kGap;
    } else if (!e.leadingIcon.isEmpty()) {
      glyphLayer(slot + kSlotLeading, m_iconFont, e.leadingIcon, d(ic),
                 QPointF(xl + kIcon / 2.0, cy), kIcon);
      xl += kIcon + kGap;
    }

    bool hasTrailing = false;

    if (exp && m_showChevron) {
      const QPointF cc(xr - kIcon / 2.0, cy);
      const bool chevPress = m_pressChevron && m_pressIndex == i;
      const bool chevHover = m_hoverChevron && m_hoverIndex == i;
      if (chevOnly && (chevPress || chevHover))
        m.fillCircle(cc, kChevronLayerR,
                     d(Ryu::withAlpha(fg, chevPress ? Ryu::kPressLayer
                                                    : Ryu::kHoverLayer)));
      const qreal ang = M_PI * open;
      const qreal ca = std::cos(ang), sa = std::sin(ang);
      auto rot = [&](qreal px, qreal py) {
        return QPointF(cc.x() + px * ca - py * sa, cc.y() + px * sa + py * ca);
      };
      const QColor cv = m_chevronColor.isValid() ? m_chevronColor : ic;
      m.polyline({rot(-kChevronHalf, -kChevronHalf / 2.0),
                  rot(0.0, kChevronHalf / 2.0),
                  rot(kChevronHalf, -kChevronHalf / 2.0)},
                 m.snapStroke(kChevronW), d(cv));
      xr -= kIcon;
      hasTrailing = true;
    }

    if (!e.trailingIcon.isEmpty()) {
      if (hasTrailing)
        xr -= kChevronGap;
      glyphLayer(slot + kSlotTrailingIcon, m_iconFont, e.trailingIcon, d(ic),
                 QPointF(xr - kIcon / 2.0, cy), kIcon);
      xr -= kIcon;
      hasTrailing = true;
    }
    if (!e.trailingText.isEmpty()) {
      if (hasTrailing)
        xr -= kGap;
      const qreal tw = QFontMetricsF(trailF).horizontalAdvance(e.trailingText);
      textLayer(slot + kSlotTrailingText, trailF, e.trailingText, d(sup),
                xr - tw, cy);
      xr -= tw;
      hasTrailing = true;
    }
    if (hasTrailing)
      xr -= kGap;

    const qreal avail = std::max(0.0, xr - xl);
    struct Line {
      int slot;
      QString s;
      const QFont *f;
      qreal lh;
      QColor c;
    };
    QVector<Line> lines;
    if (!e.overline.isEmpty())
      lines.append({kSlotOverline, e.overline, &ovF, kOverlineLh, sup});
    if (!e.headline.isEmpty())
      lines.append({kSlotHeadline, e.headline, &headF, kHeadlineLh, fg});
    if (!e.supporting.isEmpty())
      lines.append({kSlotSupporting, e.supporting, &supF, kSupportLh, sup});

    qreal total = 0.0;
    for (const Line &l : std::as_const(lines))
      total += l.lh;
    qreal y = cy - total / 2.0;
    for (const Line &l : std::as_const(lines)) {
      const QString s =
          QFontMetricsF(*l.f).elidedText(l.s, Qt::ElideRight, avail);
      textLayer(slot + l.slot, *l.f, s, d(l.c), xl, y + l.lh / 2.0);
      y += l.lh;
    }
  }
}
