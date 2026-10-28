#include "./materialiconbutton.hpp"
#include "../Core/sizes.hpp"
#include <QPainter>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kWidth[3][5] = {{32.0, 40.0, 56.0, 96.0, 136.0},
                                {28.0, 32.0, 48.0, 64.0, 104.0},
                                {40.0, 52.0, 72.0, 128.0, 184.0}};
constexpr qreal kBorder[] = {1.0, 1.0, 1.0, 2.0, 3.0};
constexpr qreal kPressShrink = 0.06;
constexpr qreal kPressStiffness = 1.4;
} // namespace

MaterialIconButton::MaterialIconButton(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.5) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  updateImplicit();
  retarget(true);
}

QColor MaterialIconButton::containerColor() const {
  if (m_checked && m_checkedColor.isValid())
    return m_checkedColor;
  if (!m_checked && m_color.isValid())
    return m_color;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#6750A4") : QColor("#E6E0E9");
  case Tonal:
    return m_checked ? QColor("#625B71") : QColor("#E8DEF8");
  case Outlined:
    return m_checked ? QColor("#322F35") : QColor(Qt::transparent);
  case Standard:
    return QColor(Qt::transparent);
  }
  return QColor(Qt::transparent);
}

QColor MaterialIconButton::contentColor() const {
  if (m_checked && m_checkedIconColor.isValid())
    return m_checkedIconColor;
  if (!m_checked && m_iconColor.isValid())
    return m_iconColor;
  switch (m_type) {
  case Filled:
    return (m_checked || !m_checkable) ? QColor("#FFFFFF") : QColor("#6750A4");
  case Tonal:
    return m_checked ? QColor("#FFFFFF") : QColor("#1D192B");
  case Outlined:
    return m_checked ? QColor("#F5EFF7") : QColor("#49454F");
  case Standard:
    return m_checked ? QColor("#6750A4") : QColor("#49454F");
  }
  return QColor("#49454F");
}

qreal MaterialIconButton::resolvedIconSize() const {
  return m_iconSize >= 0.0 ? m_iconSize : Ryu::kBtnIcon[m_sizeStyle];
}

qreal MaterialIconButton::resolvedBorderWidth() const {
  return m_borderWidth >= 0.0 ? m_borderWidth : kBorder[m_sizeStyle];
}

qreal MaterialIconButton::targetRound() const {
  if (m_pressed)
    return 0.0;
  qreal base = m_shape == Round ? 1.0 : 0.0;
  if (m_checkable && m_checked)
    base = 1.0 - base;
  return base;
}

void MaterialIconButton::updateImplicit() {
  setImplicitWidth(kWidth[m_widthStyle][m_sizeStyle]);
  setImplicitHeight(Ryu::kBtnHeight[m_sizeStyle]);
}

void MaterialIconButton::retarget(bool immediate) {
  m_round.go(targetRound(), immediate);
  m_press.go(m_pressed ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialIconButton::advance(qreal dt) {
  bool active = step(m_round, dt);
  active |= step(m_press, dt, 0.001, kPressStiffness);
  return active;
}

void MaterialIconButton::setType(Type v) {
  assign(m_type, v, &MaterialIconButton::typeChanged);
}

void MaterialIconButton::setSizeStyle(SizeStyle v) {
  if (assign(m_sizeStyle, v, &MaterialIconButton::sizeStyleChanged))
    updateImplicit();
}

void MaterialIconButton::setWidthStyle(WidthStyle v) {
  if (assign(m_widthStyle, v, &MaterialIconButton::widthStyleChanged))
    updateImplicit();
}

void MaterialIconButton::setShape(Shape v) {
  if (assign(m_shape, v, &MaterialIconButton::shapeChanged))
    retarget();
}

void MaterialIconButton::setCheckable(bool v) {
  if (assign(m_checkable, v, &MaterialIconButton::checkableChanged))
    retarget();
}

void MaterialIconButton::setChecked(bool v) {
  if (assign(m_checked, v, &MaterialIconButton::checkedChanged))
    retarget();
}

void MaterialIconButton::setIconSource(const QUrl &v) {
  if (assign(m_iconSource, v, &MaterialIconButton::iconSourceChanged)) {
    m_iconPath.clear();
    m_imgLayer.key.clear();
  }
}

void MaterialIconButton::setIconText(const QString &v) {
  assign(m_iconText, v, &MaterialIconButton::iconTextChanged);
}

void MaterialIconButton::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialIconButton::iconFontChanged);
}

void MaterialIconButton::setIconSize(qreal v) {
  if (v < 0.0)
    v = -1.0;
  assign(m_iconSize, v, &MaterialIconButton::iconSizeChanged);
}

void MaterialIconButton::setColor(const QColor &v) {
  assign(m_color, v, &MaterialIconButton::colorChanged);
}

void MaterialIconButton::setCheckedColor(const QColor &v) {
  assign(m_checkedColor, v, &MaterialIconButton::checkedColorChanged);
}

void MaterialIconButton::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialIconButton::iconColorChanged);
}

void MaterialIconButton::setCheckedIconColor(const QColor &v) {
  assign(m_checkedIconColor, v, &MaterialIconButton::checkedIconColorChanged);
}

void MaterialIconButton::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialIconButton::borderColorChanged);
}

void MaterialIconButton::setBorderWidth(qreal v) {
  if (v < 0.0)
    v = -1.0;
  assign(m_borderWidth, v, &MaterialIconButton::borderWidthChanged);
}

void MaterialIconButton::setPressed(bool v) {
  if (m_pressed == v)
    return;
  m_pressed = v;
  emit pressedChanged();
  retarget();
  update();
}

void MaterialIconButton::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  update();
}

void MaterialIconButton::activate() {
  if (m_checkable) {
    m_checked = !m_checked;
    emit checkedChanged();
    emit toggled();
    retarget();
  }
  emit clicked();
  update();
}

void MaterialIconButton::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  setPressed(true);
  e->accept();
}

void MaterialIconButton::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool inside = contains(e->position());
  setPressed(false);
  if (inside)
    activate();
  e->accept();
}

void MaterialIconButton::mouseUngrabEvent() { setPressed(false); }

void MaterialIconButton::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  if (isEnabled()) {
    setHovered(true);
    setHand(true);
  }
}

void MaterialIconButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  setHovered(false);
  setHand(false);
}

void MaterialIconButton::keyPressEvent(QKeyEvent *e) {
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
      activate();
    }
    break;
  default:
    e->ignore();
    return;
  }
  e->accept();
}

void MaterialIconButton::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

QSGNode *MaterialIconButton::updatePaintNode(QSGNode *old,
                                             UpdatePaintNodeData *d) {
  if (!old) {
    m_imgNode = nullptr;
    m_glyphNode = nullptr;
    m_imgLayer.shown.clear();
    m_glyphLayer.shown.clear();
  }
  QSGNode *root = SgMaterialItem::updatePaintNode(old, d);
  syncLayer(root, m_imgNode, m_imgLayer);
  syncLayer(root, m_glyphNode, m_glyphLayer);
  return root;
}

void MaterialIconButton::syncLayer(QSGNode *root, QSGSimpleTextureNode *&node,
                                   Layer &l) {
  if (!l.on || l.img.isNull() || !window()) {
    if (node) {
      root->removeChildNode(node);
      delete node;
      node = nullptr;
    }
    return;
  }

  if (!node) {
    node = new QSGSimpleTextureNode;
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);
    root->appendChildNode(node);
    l.shown.clear();
  }

  if (l.shown != l.key) {
    node->setTexture(window()->createTextureFromImage(l.img));
    l.shown = l.key;
  }
  node->setRect(l.rect);
}

void MaterialIconButton::build(Ryu::Mesh &m) {
  m_imgLayer.on = false;
  m_glyphLayer.on = false;

  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  const qreal dpr = m.dpr();
  const qreal scale = 1.0 - kPressShrink * m_press.x;
  const QPointF c(w / 2.0, h / 2.0);
  const QRectF rect(c.x() - w * scale / 2.0, c.y() - h * scale / 2.0, w * scale,
                    h * scale);

  const qreal full = std::min(w, h) / 2.0;
  const qreal sq = std::min(Ryu::kBtnSquareR[m_sizeStyle], full);
  const qreal radius = std::max(0.0, sq + (full - sq) * m_round.x) * scale;

  const QColor bg = containerColor();
  const QColor fg = contentColor();

  if (bg.alpha() > 0)
    m.fillRoundedRect(rect, radius, radius, radius, radius, bg);

  const qreal bw = resolvedBorderWidth();
  if (m_type == Outlined && !m_checked && bw > 0.0 && m_borderColor.alpha() > 0)
    m.strokeRoundedRect(rect, radius, radius, radius, radius, m.snapStroke(bw),
                        m_borderColor);

  const qreal layer =
      Ryu::stateLayer(m_hovered, m_pressed || (m_keyFocus && hasActiveFocus()));
  if (layer > 0.0)
    m.fillRoundedRect(rect, radius, radius, radius, radius,
                      Ryu::withAlpha(fg, layer));

  const qreal iconSz = resolvedIconSize();
  if (iconSz <= 0.0)
    return;

  const qreal fade = isEnabled() ? 1.0 : Ryu::kDisabledOpacity;
  const QColor ic = Ryu::withAlpha(fg, fg.alphaF() * fade);
  const qreal is = iconSz * scale;
  const QRectF ir(c.x() - is / 2.0, c.y() - is / 2.0, is, is);

  if (m_iconSource.isValid() && !m_iconSource.isEmpty()) {
    if (m_iconPath.isEmpty())
      m_iconPath = Ryu::resolveUrl(qmlContext(this), m_iconSource);
    const QString key = QStringLiteral("i|%1|%2|%3|%4")
                            .arg(m_iconPath, ic.name(QColor::HexArgb))
                            .arg(iconSz)
                            .arg(dpr);
    if (m_imgLayer.key != key) {
      m_imgLayer.key = key;
      m_imgLayer.img = m_iconCache.get(m_iconPath, iconSz, dpr, ic);
    }
    m_imgLayer.rect = ir;
    m_imgLayer.on = true;
  }

  if (!m_iconText.isEmpty()) {
    const QString key =
        QStringLiteral("g|%1|%2|%3|%4|%5")
            .arg(m_iconText, m_iconFont.toString(), ic.name(QColor::HexArgb))
            .arg(iconSz)
            .arg(dpr);
    if (m_glyphLayer.key != key) {
      m_glyphLayer.key = key;
      const int px = std::max(1, int(std::ceil(iconSz * dpr)));
      QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
      img.setDevicePixelRatio(dpr);
      img.fill(Qt::transparent);
      QPainter p(&img);
      p.setRenderHint(QPainter::Antialiasing, true);
      p.setRenderHint(QPainter::TextAntialiasing, true);
      Ryu::drawGlyph(&p, m_iconFont, m_iconText,
                     QPointF(iconSz / 2.0, iconSz / 2.0), iconSz, ic);
      p.end();
      m_glyphLayer.img = img;
    }
    m_glyphLayer.rect = ir;
    m_glyphLayer.on = true;
  }
}
