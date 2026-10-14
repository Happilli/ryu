#include "./materiallist.hpp"
#include "../Shapes/springdriver.hpp"
#include <QDir>
#include <QFontMetricsF>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQmlFile>
#include <QUrl>
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
constexpr qreal kDisabled = 0.38;
constexpr qreal kMaxStep = 0.004;

QPainterPath cornerRect(const QRectF &r, qreal tl, qreal tr, qreal br,
                        qreal bl) {
  const qreal maxR = std::min(r.height(), r.width()) / 2.0;
  tl = std::clamp(tl, 0.0, maxR);
  tr = std::clamp(tr, 0.0, maxR);
  br = std::clamp(br, 0.0, maxR);
  bl = std::clamp(bl, 0.0, maxR);
  const qreal x0 = r.left(), x1 = r.right(), t = r.top(), b = r.bottom();

  QPainterPath p;
  p.moveTo(x0 + tl, t);
  p.lineTo(x1 - tr, t);
  if (tr > 0.0)
    p.arcTo(x1 - 2 * tr, t, 2 * tr, 2 * tr, 90, -90);
  p.lineTo(x1, b - br);
  if (br > 0.0)
    p.arcTo(x1 - 2 * br, b - 2 * br, 2 * br, 2 * br, 0, -90);
  p.lineTo(x0 + bl, b);
  if (bl > 0.0)
    p.arcTo(x0, b - 2 * bl, 2 * bl, 2 * bl, 270, -90);
  p.lineTo(x0, t + tl);
  if (tl > 0.0)
    p.arcTo(x0, t, 2 * tl, 2 * tl, 180, -90);
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

void drawGlyph(QPainter *p, const QFont &font, const QString &text,
               const QPointF &c, qreal size, const QColor &color) {
  QFont f = font;
  f.setPixelSize(qMax(1, qRound(size)));
  f.setHintingPreference(QFont::PreferNoHinting);

  QPainterPath gp;
  gp.addText(QPointF(0.0, 0.0), f, text);
  const QRectF br = gp.boundingRect();
  if (br.isEmpty())
    return;

  const qreal k = std::min(1.0, size / std::max(br.width(), br.height()));
  p->save();
  p->translate(c);
  p->scale(k, k);
  p->translate(-br.center());
  p->setPen(Qt::NoPen);
  p->setBrush(color);
  p->drawPath(gp);
  p->restore();
}

void drawText(QPainter *p, const QFont &f, const QString &s, qreal x, qreal cy,
              const QColor &c) {
  if (s.isEmpty())
    return;
  const QFontMetricsF fm(f);
  QPainterPath tp;
  tp.addText(QPointF(0.0, 0.0), f, s);
  p->save();
  p->translate(x, cy + (fm.ascent() - fm.descent()) / 2.0);
  p->setPen(Qt::NoPen);
  p->setBrush(c);
  p->drawPath(tp);
  p->restore();
}

QImage loadSquare(const QString &src) {
  QString path = src;
  const QUrl u(src);
  if (u.isLocalFile() || u.scheme().compare("qrc", Qt::CaseInsensitive) == 0)
    path = QQmlFile::urlToLocalFileOrQrc(u);
  else if (path.startsWith(QLatin1String("~/")))
    path = QDir::homePath() + path.mid(1);

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

MaterialList::MaterialList(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setImplicitWidth(360.0);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

  structureChanged();
}

QFont MaterialList::textFont(qreal px, int weight) const {
  QFont f = m_font;
  f.setPixelSize(qRound(px));
  f.setWeight(QFont::Weight(weight));
  f.setHintingPreference(QFont::PreferNoHinting);
  f.setStyleStrategy(
      QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
  return f;
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

void MaterialList::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialList::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  const bool active = stepSpring(m_press.x, m_press.v, m_press.t, m_stiffness,
                                 m_damping, dt, 0.001);
  update();
  if (!active) {
    if (m_press.t == 0.0)
      m_pressShown = -1;
    m_driver->stop();
  }
}

void MaterialList::setModel(const QVariantList &v) {
  m_raw = v;
  m_items.clear();
  m_items.reserve(v.size());

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
        e.image = loadSquare(img);
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

void MaterialList::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialList::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
}

void MaterialList::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickPaintedItem::geometryChange(n, o);
  if (n.size() != o.size())
    m_layoutDirty = true;
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
  m_press.t = 1.0;
  ensureDriver();
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
  m_press.t = 0.0;
  ensureDriver();
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
  m_press.t = 0.0;
  ensureDriver();
  update();
}

void MaterialList::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialList::hoverMoveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverMoveEvent(e);
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
  QQuickPaintedItem::hoverLeaveEvent(e);
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
  QQuickPaintedItem::focusInEvent(e);
  if (!selectable(m_focusIndex))
    m_focusIndex = step(-1, 1);
}

void MaterialList::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialList::paint(QPainter *p) {
  if (m_layoutDirty)
    layoutItems();
  const int n = m_items.size();
  if (n == 0 || width() <= 0.0)
    return;

  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  p->setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (!isEnabled())
    p->setOpacity(kDisabled);

  const QFont headF = textFont(16.0, QFont::Normal);
  const QFont supF = textFont(14.0, QFont::Normal);
  const QFont ovF = textFont(11.0, QFont::Medium);
  const QFont trailF = textFont(11.0, QFont::Medium);
  const QColor base = resolvedItemColor();
  const QColor iconBase =
      m_iconColor.isValid() ? m_iconColor : m_supportingColor;

  for (int i = 0; i < n; ++i) {
    const Entry &e = m_items[i];
    const QRectF r = m_rects[i];

    if (e.divider) {
      if (m_variant == Standard) {
        const qreal y = std::floor(r.center().y()) + 0.5;
        p->setPen(QPen(m_dividerColor, 1.0));
        p->drawLine(QPointF(r.left() + kPad, y), QPointF(r.right() - kPad, y));
        p->setPen(Qt::NoPen);
      }
      continue;
    }

    const bool checked = isChecked(i);
    p->save();
    if (!e.enabled)
      p->setOpacity(p->opacity() * kDisabled);

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
    const QPainterPath path = cornerRect(r, rt, rt, rb, rb);

    const QColor bg = checked ? m_selectedColor : base;
    const QColor fg = checked ? m_selectedTextColor : m_textColor;
    const QColor sup = checked ? m_selectedTextColor : m_supportingColor;
    const QColor ic = checked ? m_selectedTextColor : iconBase;

    p->setPen(Qt::NoPen);
    if (bg.alpha() > 0) {
      p->setBrush(bg);
      p->drawPath(path);
    }

    qreal layer = 0.0;
    if (m_hoverIndex == i)
      layer = 0.08;
    if (m_pressIndex == i ||
        (m_keyFocus && hasActiveFocus() && m_focusIndex == i))
      layer = 0.10;
    if (layer > 0.0) {
      QColor l = fg;
      l.setAlphaF(layer);
      p->setBrush(l);
      p->drawPath(path);
    }

    const qreal cy = r.center().y();
    qreal xl = r.left() + kPad;
    qreal xr = r.right() - kPad;

    if (!e.image.isNull()) {
      const QRectF ir(xl, cy - kImage / 2.0, kImage, kImage);
      QPainterPath clip;
      clip.addRoundedRect(ir, kImageR, kImageR);
      p->save();
      p->setClipPath(clip);
      p->drawImage(ir, e.image);
      p->restore();
      xl += kImage + kGap;
    } else if (!e.avatarText.isEmpty()) {
      p->setBrush(m_avatarColor);
      p->drawEllipse(QPointF(xl + kAvatar / 2.0, cy), kAvatar / 2.0,
                     kAvatar / 2.0);
      const QFont af = textFont(16.0, QFont::Medium);
      const qreal tw = QFontMetricsF(af).horizontalAdvance(e.avatarText);
      drawText(p, af, e.avatarText, xl + kAvatar / 2.0 - tw / 2.0, cy,
               m_avatarTextColor);
      xl += kAvatar + kGap;
    } else if (!e.leadingIcon.isEmpty()) {
      drawGlyph(p, m_iconFont, e.leadingIcon, QPointF(xl + kIcon / 2.0, cy),
                kIcon, ic);
      xl += kIcon + kGap;
    }

    bool hasTrailing = false;
    if (!e.trailingIcon.isEmpty()) {
      drawGlyph(p, m_iconFont, e.trailingIcon, QPointF(xr - kIcon / 2.0, cy),
                kIcon, ic);
      xr -= kIcon;
      hasTrailing = true;
    }
    if (!e.trailingText.isEmpty()) {
      if (hasTrailing)
        xr -= kGap;
      const qreal tw = QFontMetricsF(trailF).horizontalAdvance(e.trailingText);
      drawText(p, trailF, e.trailingText, xr - tw, cy, sup);
      xr -= tw;
      hasTrailing = true;
    }
    if (hasTrailing)
      xr -= kGap;

    const qreal avail = std::max(0.0, xr - xl);
    struct Line {
      QString s;
      const QFont *f;
      qreal lh;
      QColor c;
    };
    QVector<Line> lines;
    if (!e.overline.isEmpty())
      lines.append({e.overline, &ovF, kOverlineLh, sup});
    if (!e.headline.isEmpty())
      lines.append({e.headline, &headF, kHeadlineLh, fg});
    if (!e.supporting.isEmpty())
      lines.append({e.supporting, &supF, kSupportLh, sup});

    qreal total = 0.0;
    for (const Line &l : std::as_const(lines))
      total += l.lh;
    qreal y = cy - total / 2.0;
    for (const Line &l : std::as_const(lines)) {
      const QString s =
          QFontMetricsF(*l.f).elidedText(l.s, Qt::ElideRight, avail);
      drawText(p, *l.f, s, xl, y + l.lh / 2.0, l.c);
      y += l.lh;
    }

    p->restore();
  }
}
