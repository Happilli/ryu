#include "./materialbuttongroup.hpp"
#include "../Core/paintutil.hpp"
#include "../Core/sizes.hpp"
#include <QFontMetricsF>
#include <QMetaObject>
#include <QPainter>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kPad[] = {12.0, 16.0, 24.0, 48.0, 64.0};
constexpr qreal kInnerR[] = {8.0, 8.0, 8.0, 16.0, 20.0};
constexpr qreal kStdGap[] = {18.0, 12.0, 8.0, 8.0, 8.0};
constexpr qreal kConnectedGap = 2.0;
constexpr qreal kPressGrow = 0.28;
} // namespace

MaterialButtonGroup::MaterialButtonGroup(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setActiveFocusOnTab(true);
  structureChanged();
}

void MaterialButtonGroup::componentComplete() {
  SgMaterialItem::componentComplete();
  retarget(true);
  update();
}

QFont MaterialButtonGroup::labelFont() const {
  return Ryu::styledFont(m_font, Ryu::kBtnFont[m_sizeStyle], m_fontWeight);
}
qreal MaterialButtonGroup::uniformWidth() const {
  qreal w = 0.0;
  for (int i = 0; i < count(); ++i)
    w = std::max(w, itemWidth(i));
  return w;
}

qreal MaterialButtonGroup::naturalWidth() const {
  const int n = count();
  qreal total = 0.0;
  if (m_equalWidth)
    total = uniformWidth() * n;
  else
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

  const qreal h = Ryu::kBtnHeight[m_sizeStyle];
  const qreal gap = spacingValue();
  const qreal total = naturalWidth();
  const qreal y = std::round((height() - h) / 2.0);

  qreal eq = m_equalWidth ? uniformWidth() : 0.0;
  qreal extra = 0.0;
  if (m_fillWidth && width() > total) {
    if (m_equalWidth)
      eq = (width() - gap * (n - 1)) / n;
    else
      extra = (width() - total) / n;
  }

  qreal x = 0.0;
  for (int i = 0; i < n; ++i) {
    const qreal w = m_equalWidth ? eq : itemWidth(i) + extra;
    m_rects[i] = QRectF(x, y, w, h);
    x += w + gap;
  }
}

void MaterialButtonGroup::setEqualWidth(bool v) {
  if (assign(m_equalWidth, v, &MaterialButtonGroup::equalWidthChanged))
    structureChanged();
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

qreal MaterialButtonGroup::padValue() const {
  return m_padX >= 0.0 ? m_padX : kPad[m_sizeStyle];
}

qreal MaterialButtonGroup::itemWidth(int i) const {
  ensureMetrics();
  const bool hasIcon = !m_icons.value(i).isEmpty();
  const qreal tw = m_textW.value(i);
  qreal w = 2.0 * padValue();
  if (hasIcon)
    w += Ryu::kBtnIcon[m_sizeStyle];
  if (hasIcon && tw > 0.0)
    w += Ryu::kBtnIconGap[m_sizeStyle];
  return w + tw;
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
  setImplicitWidth(std::ceil(naturalWidth()));
  setImplicitHeight(Ryu::kBtnHeight[m_sizeStyle]);
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
  const qreal g = Ryu::kBtnHeight[m_sizeStyle] * kPressGrow;
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
    m_round[i].go(targetRound(i), immediate);
    m_width[i].go(targetWidth(i), immediate);
  }
  if (!immediate)
    animate();
}

bool MaterialButtonGroup::advance(qreal dt) {
  bool active = false;
  for (int i = 0; i < m_round.size(); ++i) {
    active |= step(m_round[i], dt);
    active |= step(m_width[i], dt, 0.02, 1.2);
  }
  return active;
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

void MaterialButtonGroup::setHorizontalPadding(qreal v) {
  if (v < 0.0)
    v = -1.0;
  if (assign(m_padX, v, &MaterialButtonGroup::horizontalPaddingChanged))
    structureChanged();
}

void MaterialButtonGroup::setFillWidth(bool v) {
  if (assign(m_fillWidth, v, &MaterialButtonGroup::fillWidthChanged))
    m_layoutDirty = true;
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
  QQuickItem::geometryChange(n, o);
  if (n.size() != o.size()) {
    m_layoutDirty = true;
    update();
  }
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
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

QSGNode *MaterialButtonGroup::updatePaintNode(QSGNode *old,
                                              UpdatePaintNodeData *d) {
  if (!old) {
    for (Cell &c : m_cells) {
      c.node = nullptr;
      c.shown.clear();
    }
  }
  QSGNode *root = SgMaterialItem::updatePaintNode(old, d);
  for (Cell &c : m_cells)
    syncCell(root, c);
  while (!m_cells.empty() && !m_cells.back().node)
    m_cells.pop_back();
  return root;
}

void MaterialButtonGroup::syncCell(QSGNode *root, Cell &c) {
  if (!c.on || c.img.isNull() || !window()) {
    if (c.node) {
      root->removeChildNode(c.node);
      delete c.node;
      c.node = nullptr;
    }
    return;
  }

  if (!c.node) {
    c.node = new QSGSimpleTextureNode;
    c.node->setOwnsTexture(true);
    c.node->setFiltering(QSGTexture::Linear);
    root->appendChildNode(c.node);
    c.shown.clear();
  }

  if (c.shown != c.key) {
    c.node->setTexture(window()->createTextureFromImage(c.img));
    c.shown = c.key;
  }
  c.node->setRect(c.rect);
}

void MaterialButtonGroup::build(Ryu::Mesh &m) {
  for (Cell &c : m_cells)
    c.on = false;

  if (m_layoutDirty)
    layoutItems();
  ensureMetrics();
  const int n = m_rects.size();
  if (n == 0)
    return;
  if (int(m_cells.size()) < n)
    m_cells.resize(n);

  const int s = m_sizeStyle;
  const qreal full = Ryu::kBtnHeight[s] / 2.0;
  const qreal iconSz = Ryu::kBtnIcon[s];
  const qreal dpr = m.dpr();
  const qreal fade = isEnabled() ? 1.0 : Ryu::kDisabledOpacity;
  const QFont lf = labelFont();
  const QFontMetricsF fm(lf);
  const QVector<QRectF> rects = displayRects();

  for (int i = 0; i < n; ++i) {
    const QRectF r = rects[i];
    const qreal t = std::clamp(m_round.value(i).x, 0.0, 1.0);
    const bool checked = isChecked(i);

    qreal rl, rr;
    if (m_variant == Standard) {
      rl = rr = kInnerR[s] + (full - kInnerR[s]) * t;
      const qreal sq = Ryu::kBtnSquareR[s];
      rl = rr = sq + (full - sq) * t;
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

    m.fillRoundedRect(r, rl, rr, rr, rl, bg);

    if (m_pressIndex == i ||
        (m_keyFocus && hasActiveFocus() && m_focusIndex == i))
      m.fillRoundedRect(r, rl, rr, rr, rl,
                        Ryu::withAlpha(fg, Ryu::kPressLayer));

    if (m_borderWidth > 0.0 && m_borderColor.isValid() &&
        m_borderColor.alpha() > 0)
      m.strokeRoundedRect(r, rl, rr, rr, rl, m.snapStroke(m_borderWidth),
                          m_borderColor);

    const QString lbl = m_labels.value(i);
    const QString ic = m_icons.value(i);
    const qreal lblW = m_textW.value(i);
    const qreal gap =
        (!ic.isEmpty() && !lbl.isEmpty()) ? Ryu::kBtnIconGap[s] : 0.0;
    const qreal contentW = (ic.isEmpty() ? 0.0 : iconSz) + gap + lblW;
    if (contentW <= 0.0)
      continue;

    const QColor col = Ryu::withAlpha(fg, fg.alphaF() * fade);
    const qreal iw = std::ceil(contentW) + 2.0;
    const qreal ih = std::ceil(std::max(iconSz, fm.height())) + 2.0;

    Cell &c = m_cells[i];
    const QString key =
        QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
            .arg(lbl, ic, m_font.toString(), m_iconFont.toString())
            .arg(m_fontWeight)
            .arg(s)
            .arg(col.name(QColor::HexArgb), QString::number(dpr),
                 QString::number(iw));
    if (c.key != key) {
      c.key = key;
      const int pw = std::max(1, int(std::ceil(iw * dpr)));
      const int ph = std::max(1, int(std::ceil(ih * dpr)));
      QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
      img.setDevicePixelRatio(dpr);
      img.fill(Qt::transparent);
      QPainter p(&img);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      p.setRenderHint(QPainter::SmoothPixmapTransform, true);
      qreal x = (iw - contentW) / 2.0;
      if (!ic.isEmpty()) {
        Ryu::drawGlyph(&p, m_iconFont, ic, QPointF(x + iconSz / 2.0, ih / 2.0),
                       iconSz, col);
        x += iconSz + gap;
      }
      Ryu::drawLabel(&p, lf, lbl, x, ih / 2.0, col);
      p.end();
      c.img = img;
    }

    c.rect = QRectF(m.snap(r.center().x() - iw / 2.0),
                    m.snap(r.center().y() - ih / 2.0), iw, ih);
    c.on = true;
  }
}
