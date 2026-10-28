#include "./materiallist.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QQmlEngine>
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

qreal MaterialList::rowHeight(const Entry &e) const {
  if (e.divider)
    return m_variant == Segmented ? kSegDivider : kDivider;
  return kHeight[std::min(e.lines(), 3)];
}

qreal MaterialList::naturalHeight() const {
  qreal h = 0.0;
  for (const Entry &e : m_items)
    h += rowHeight(e) + (e.divider ? 0.0 : itemGap());
  if (!m_items.isEmpty() && !m_items.last().divider)
    h -= itemGap();
  return std::max(0.0, h);
}

void MaterialList::layoutItems() {
  m_layoutDirty = false;
  const int n = m_items.size();
  m_rects.assign(n, QRectF());
  qreal y = 0.0;
  for (int i = 0; i < n; ++i) {
    const qreal h = rowHeight(m_items[i]);
    m_rects[i] = QRectF(0.0, y, width(), h);
    y += h + (m_items[i].divider ? 0.0 : itemGap());
  }
}

void MaterialList::structureChanged() {
  m_layoutDirty = true;
  setImplicitHeight(std::ceil(naturalHeight()));
  m_checked.erase(std::remove_if(m_checked.begin(), m_checked.end(),
                                 [this](int i) { return !selectable(i); }),
                  m_checked.end());
  if (!selectable(m_focusIndex))
    m_focusIndex = step(-1, 1);
  update();
}

bool MaterialList::selectable(int i) const {
  return i >= 0 && i < m_items.size() && !m_items[i].divider &&
         m_items[i].enabled;
}

int MaterialList::rowAt(const QPointF &pt) const {
  for (int i = 0; i < m_rects.size(); ++i)
    if (m_rects[i].adjusted(0, 0, 0, itemGap()).contains(pt))
      return i;
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

bool MaterialList::advance(qreal dt) {
  const bool active = SgMaterialItem::step(m_press, dt);
  if (!active && m_press.t == 0.0)
    m_pressShown = -1;
  return active;
}

void MaterialList::setModel(const QVariantList &v) {
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
      const QString img = m.value("image").toString();
      if (!img.isEmpty())
        e.image = loadSquare(Ryu::resolveUrl(qmlContext(this), QUrl(img)));
    }
    m_items.append(e);
  }

  m_pressIndex = m_pressShown = m_hoverIndex = -1;
  const QList<int> before = m_checked;
  structureChanged();
  emit modelChanged();
  if (before != m_checked)
    emit checkedIndicesChanged();
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
  if (n.size() != o.size()) {
    m_layoutDirty = true;
    update();
  }
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
  m_pressIndex = m_focusIndex = m_pressShown = i;
  m_press.go(1.0);
  animate();
  update();
  e->accept();
}

void MaterialList::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0)
    return;
  if (m_layoutDirty)
    layoutItems();
  const int pressed = m_pressIndex;
  m_pressIndex = -1;
  m_press.go(0.0);
  animate();
  const bool inside = rowAt(e->position()) == pressed;
  update();
  if (inside)
    activate(pressed);
  e->accept();
}

void MaterialList::mouseUngrabEvent() {
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
  if (h != m_hoverIndex) {
    m_hoverIndex = h;
    update();
  }
}

void MaterialList::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  m_hoverIndex = -1;
  update();
}

void MaterialList::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || m_items.isEmpty()) {
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

    const qreal layer =
        Ryu::stateLayer(m_hoverIndex == i,
                        m_pressIndex == i || (m_keyFocus && hasActiveFocus() &&
                                              m_focusIndex == i));
    if (layer > 0.0)
      m.fillRoundedRect(r, rt, rt, rb, rb, d(Ryu::withAlpha(fg, layer)));

    const int slot = i * kSlots;
    const qreal cy = r.center().y();
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
    if (!e.trailingIcon.isEmpty()) {
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
