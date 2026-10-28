#include "./materialsearchview.hpp"
#include "../Core/paintutil.hpp"
#include <QFontMetricsF>
#include <QMetaObject>
#include <QPainter>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPad = 8.0;
constexpr qreal kRowPad = 16.0;
constexpr qreal kIcon = 24.0;
constexpr qreal kGap = 16.0;
constexpr qreal kRowR = 20.0;
constexpr qreal kEmptyH = 72.0;
constexpr qreal kHeadLh = 24.0;
constexpr qreal kSupLh = 20.0;
} // namespace

MaterialSearchView::MaterialSearchView(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.6) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setTransformOrigin(Top);
  setVisible(false);
  setImplicitWidth(440.0);
  setImplicitHeight(contentHeight());
  applyPop();
}

qreal MaterialSearchView::viewport() const {
  if (m_items.isEmpty())
    return 0.0;
  return std::min<int>(m_items.size(), m_maxVisible) * m_itemH;
}

qreal MaterialSearchView::contentHeight() const {
  if (m_items.isEmpty())
    return kEmptyH;
  return 2.0 * kPad + viewport();
}

qreal MaterialSearchView::maxScroll() const {
  return std::max(0.0, m_items.size() * m_itemH - viewport());
}

bool MaterialSearchView::selectable(int i) const {
  return i >= 0 && i < m_items.size() && m_items[i].enabled;
}

int MaterialSearchView::rowAt(const QPointF &pt) const {
  if (pt.y() < kPad || pt.y() > height() - kPad)
    return -1;
  const int i = int(std::floor((pt.y() - kPad + m_scroll.x) / m_itemH));
  return (i >= 0 && i < m_items.size()) ? i : -1;
}

int MaterialSearchView::nextSelectable(int from, int dir) const {
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

void MaterialSearchView::structureChanged() {
  setImplicitHeight(contentHeight());
  emit contentHeightChanged();
  m_scroll.t = std::clamp(m_scroll.t, 0.0, maxScroll());
  m_scroll.x = std::clamp(m_scroll.x, 0.0, maxScroll());
  retarget(true);
  update();
}

void MaterialSearchView::retarget(bool reveal) {
  if (m_current >= 0 && m_current < m_items.size()) {
    const qreal top = m_current * m_itemH;
    if (m_hlA.x < 0.02) {
      m_hlY.x = top;
      m_hlY.v = 0.0;
    }
    m_hlY.t = top;
    m_hlA.t = 1.0;
    if (reveal) {
      const qreal vp = viewport();
      if (top < m_scroll.t)
        m_scroll.t = top;
      else if (top + m_itemH > m_scroll.t + vp)
        m_scroll.t = top + m_itemH - vp;
      m_scroll.t = std::clamp(m_scroll.t, 0.0, maxScroll());
    }
  } else {
    m_hlA.t = 0.0;
  }
  animate();
}

void MaterialSearchView::applyPop() {
  const qreal p = m_pop.x;
  setScale(0.96 + 0.04 * p);
  setOpacity(std::clamp(p * 1.6, 0.0, 1.0));
}

bool MaterialSearchView::advance(qreal dt) {
  const bool popActive = step(m_pop, dt);
  bool active = popActive;
  active |= step(m_hlY, dt, 0.05, 1.2);
  active |= step(m_scroll, dt, 0.05, 1.0);
  active |= Ryu::stepSpring(m_hlA, springStiffness(), 1.0, dt, 0.001);
  applyPop();
  if (!m_open && !popActive)
    setVisible(false);
  return active;
}

void MaterialSearchView::setCurrentInternal(int i, bool reveal) {
  if (m_current == i)
    return;
  m_current = i;
  emit currentIndexChanged();
  retarget(reveal);
  update();
}

void MaterialSearchView::trigger(int i) {
  if (!selectable(i))
    return;
  const QVariant item = m_raw.value(i);
  QMetaObject::invokeMethod(
      this, [this, i, item] { emit triggered(i, item); }, Qt::QueuedConnection);
}

void MaterialSearchView::moveCurrent(int dir) {
  setCurrentInternal(nextSelectable(m_current, dir < 0 ? -1 : 1), true);
}

void MaterialSearchView::triggerCurrent() { trigger(m_current); }

void MaterialSearchView::setOpen(bool v) {
  if (m_open == v)
    return;
  m_open = v;
  m_pressIndex = -1;
  if (v)
    setVisible(true);
  m_pop.t = v ? 1.0 : 0.0;
  animate();
  update();
  emit openChanged();
  if (v)
    emit opened();
  else
    emit closed();
}

void MaterialSearchView::setModel(const QVariantList &v) {
  m_raw = v;
  m_items.clear();
  m_items.reserve(v.size());

  for (const QVariant &it : v) {
    Entry e;
    if (it.metaType().id() == QMetaType::QString) {
      e.headline = it.toString();
    } else {
      const QVariantMap m = it.toMap();
      e.headline = m.value("headline").toString();
      e.supporting = m.value("supporting").toString();
      e.trailingText = m.value("trailingText").toString();
      e.leadingIcon = m.value("leadingIcon").toString();
      e.enabled = m.contains("enabled") ? m.value("enabled").toBool() : true;
    }
    m_items.append(e);
  }

  m_pressIndex = -1;
  m_scroll.go(0.0, true);

  const int next = m_autoSelect ? nextSelectable(-1, 1) : -1;
  if (next != m_current) {
    m_current = next;
    emit currentIndexChanged();
  }
  structureChanged();
  emit modelChanged();
}

void MaterialSearchView::setCurrentIndex(int i) {
  setCurrentInternal(selectable(i) ? i : -1, true);
}

void MaterialSearchView::setAutoSelect(bool v) {
  assign(m_autoSelect, v, &MaterialSearchView::autoSelectChanged);
}

void MaterialSearchView::setEmptyText(const QString &v) {
  assign(m_emptyText, v, &MaterialSearchView::emptyTextChanged);
}

void MaterialSearchView::setFont(const QFont &v) {
  assign(m_font, v, &MaterialSearchView::fontChanged);
}

void MaterialSearchView::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSearchView::iconFontChanged);
}

void MaterialSearchView::setItemHeight(qreal v) {
  v = std::max(40.0, v);
  if (assign(m_itemH, v, &MaterialSearchView::itemHeightChanged))
    structureChanged();
}

void MaterialSearchView::setMaxVisibleItems(int v) {
  v = std::max(1, v);
  if (assign(m_maxVisible, v, &MaterialSearchView::maxVisibleItemsChanged))
    structureChanged();
}

void MaterialSearchView::setRadius(qreal v) {
  assign(m_radius, std::max(0.0, v), &MaterialSearchView::radiusChanged);
}

void MaterialSearchView::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialSearchView::borderWidthChanged);
}

void MaterialSearchView::setColor(const QColor &v) {
  assign(m_color, v, &MaterialSearchView::colorChanged);
}

void MaterialSearchView::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialSearchView::textColorChanged);
}

void MaterialSearchView::setSupportingColor(const QColor &v) {
  assign(m_supportingColor, v, &MaterialSearchView::supportingColorChanged);
}

void MaterialSearchView::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialSearchView::iconColorChanged);
}

void MaterialSearchView::setHighlightColor(const QColor &v) {
  assign(m_highlightColor, v, &MaterialSearchView::highlightColorChanged);
}

void MaterialSearchView::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSearchView::borderColorChanged);
}

void MaterialSearchView::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickItem::geometryChange(n, o);
  if (n.size() != o.size()) {
    m_scroll.t = std::clamp(m_scroll.t, 0.0, maxScroll());
    m_scroll.x = std::clamp(m_scroll.x, 0.0, maxScroll());
    update();
  }
}

void MaterialSearchView::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  const int i = rowAt(e->position());
  m_pressIndex = selectable(i) ? i : -1;
  if (m_pressIndex >= 0)
    setCurrentInternal(m_pressIndex, false);
  update();
  e->accept();
}

void MaterialSearchView::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressIndex < 0) {
    e->accept();
    return;
  }
  const int pressed = m_pressIndex;
  m_pressIndex = -1;
  const bool inside = rowAt(e->position()) == pressed;
  update();
  if (inside)
    trigger(pressed);
  e->accept();
}

void MaterialSearchView::mouseUngrabEvent() {
  m_pressIndex = -1;
  update();
}

void MaterialSearchView::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialSearchView::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (!isEnabled() || !m_open)
    return;
  const int i = rowAt(e->position());
  if (selectable(i))
    setCurrentInternal(i, false);
}

void MaterialSearchView::wheelEvent(QWheelEvent *e) {
  const qreal d = e->pixelDelta().y() != 0 ? qreal(e->pixelDelta().y())
                                           : e->angleDelta().y() * 0.5;
  m_scroll.t = std::clamp(m_scroll.t - d, 0.0, maxScroll());
  animate();
  e->accept();
}

void MaterialSearchView::build(Ryu::Mesh &m) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  m.fillRoundedRect(QRectF(0.0, 0.0, w, h), m_radius, m_radius, m_radius,
                    m_radius, m_color);

  if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0)
    m.strokeRoundedRect(QRectF(0.0, 0.0, w, h), m_radius, m_radius, m_radius,
                        m_radius, m.snapStroke(m_borderWidth), m_borderColor);

  const QFont headF = Ryu::styledFont(m_font, 16.0, QFont::Normal);
  const QFont supF = Ryu::styledFont(m_font, 14.0, QFont::Normal);
  const QFont trailF = Ryu::styledFont(m_font, 11.0, QFont::Medium);

  if (m_items.isEmpty()) {
    const qreal tw = QFontMetricsF(supF).horizontalAdvance(m_emptyText);
    textLayer(0, supF, m_emptyText, m_supportingColor, (w - tw) / 2.0, h / 2.0);
    return;
  }

  const QRectF vr(0.0, kPad, w, std::max(0.0, h - 2.0 * kPad));

  const qreal a = std::clamp(m_hlA.x, 0.0, 1.0);
  if (a > 0.001) {
    const QRectF hr =
        QRectF(kPad, kPad + m_hlY.x - m_scroll.x, w - 2.0 * kPad, m_itemH)
            .intersected(vr);
    if (!hr.isEmpty()) {
      m.fillRoundedRect(
          hr, kRowR, kRowR, kRowR, kRowR,
          Ryu::withAlpha(m_highlightColor, m_highlightColor.alphaF() * a));
      if (m_pressIndex >= 0 && m_pressIndex == m_current)
        m.fillRoundedRect(hr, kRowR, kRowR, kRowR, kRowR,
                          Ryu::withAlpha(m_textColor, Ryu::kPressLayer * a));
    }
  }

  const qreal dpr = m.dpr();
  const qreal fd = fade();
  const int n = m_items.size();
  const int first = std::max(0, int(std::floor(m_scroll.x / m_itemH)));
  const int last =
      std::min(n - 1, int(std::ceil((m_scroll.x + viewport()) / m_itemH)));
  const qreal rw = std::ceil((w - 2.0 * kPad) * dpr) / dpr;
  const qreal rh = std::ceil(m_itemH * dpr) / dpr;

  for (int i = first; i <= last; ++i) {
    const Entry &e = m_items[i];
    const qreal y = kPad + i * m_itemH - m_scroll.x;
    const qreal top = std::max(0.0, vr.top() - y);
    const qreal bot = std::min(m_itemH, vr.bottom() - y);
    if (bot - top < 0.25)
      continue;

    const QString key = QStringList{"r",
                                    e.headline,
                                    e.supporting,
                                    e.trailingText,
                                    e.leadingIcon,
                                    e.enabled ? "1" : "0",
                                    QString::number(rw),
                                    QString::number(rh),
                                    QString::number(qRound(top * 4.0)),
                                    QString::number(qRound(bot * 4.0)),
                                    m_font.key(),
                                    m_iconFont.key(),
                                    m_textColor.name(QColor::HexArgb),
                                    m_supportingColor.name(QColor::HexArgb),
                                    m_iconColor.name(QColor::HexArgb),
                                    QString::number(dpr),
                                    QString::number(fd)}
                            .join(QLatin1Char('|'));

    useLayer(i, key, QRectF(m.snap(kPad), m.snap(y), rw, rh), [&] {
      QImage img = Ryu::newImage(rw, rh, dpr);
      QPainter p(&img);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      p.setClipRect(QRectF(0.0, top, rw, bot - top));
      p.setOpacity(fd * (e.enabled ? 1.0 : Ryu::kDisabledOpacity));

      const qreal cy = m_itemH / 2.0;
      qreal xl = kRowPad;
      qreal xr = rw - kRowPad;

      if (!e.leadingIcon.isEmpty()) {
        Ryu::drawGlyph(&p, m_iconFont, e.leadingIcon,
                       QPointF(xl + kIcon / 2.0, cy), kIcon, m_iconColor);
        xl += kIcon + kGap;
      }

      if (!e.trailingText.isEmpty()) {
        const qreal tw =
            QFontMetricsF(trailF).horizontalAdvance(e.trailingText);
        Ryu::drawLabel(&p, trailF, e.trailingText, xr - tw, cy,
                       m_supportingColor);
        xr -= tw + kGap;
      }

      const qreal avail = std::max(0.0, xr - xl);
      const bool two = !e.supporting.isEmpty();
      const qreal total = kHeadLh + (two ? kSupLh : 0.0);
      qreal ty = cy - total / 2.0;

      const QString hs =
          QFontMetricsF(headF).elidedText(e.headline, Qt::ElideRight, avail);
      Ryu::drawLabel(&p, headF, hs, xl, ty + kHeadLh / 2.0, m_textColor);
      ty += kHeadLh;

      if (two) {
        const QString ss =
            QFontMetricsF(supF).elidedText(e.supporting, Qt::ElideRight, avail);
        Ryu::drawLabel(&p, supF, ss, xl, ty + kSupLh / 2.0, m_supportingColor);
      }
      p.end();
      return img;
    });
  }
}
