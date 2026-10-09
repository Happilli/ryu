#include "../Shapes/springdriver.hpp"
#include "./materialiconbutton.hpp"
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQmlFile>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight[] = {32.0, 40.0, 56.0, 96.0, 136.0};
constexpr qreal kWidth[3][5] = {{32.0, 40.0, 56.0, 96.0, 136.0},
                                {28.0, 32.0, 48.0, 64.0, 104.0},
                                {40.0, 52.0, 72.0, 128.0, 184.0}};
constexpr qreal kIconSz[] = {20.0, 24.0, 24.0, 32.0, 40.0};
constexpr qreal kSquareR[] = {12.0, 12.0, 16.0, 28.0, 28.0};
constexpr qreal kBorder[] = {1.0, 1.0, 1.0, 2.0, 3.0};
constexpr qreal kPressShrink = 0.06;
constexpr qreal kMaxStep = 0.004;

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

MaterialIconButton::MaterialIconButton(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

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

QColor MaterialIconButton::outlineColor() const { return m_borderColor; }

qreal MaterialIconButton::resolvedIconSize() const {
  return m_iconSize >= 0.0 ? m_iconSize : kIconSz[m_sizeStyle];
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
  setImplicitHeight(kHeight[m_sizeStyle]);
}

void MaterialIconButton::retarget(bool immediate) {
  m_round.t = targetRound();
  m_press.t = m_pressed ? 1.0 : 0.0;
  if (immediate) {
    m_round.x = m_round.t;
    m_round.v = 0.0;
    m_press.x = m_press.t;
    m_press.v = 0.0;
    return;
  }
  ensureDriver();
}

void MaterialIconButton::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialIconButton::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  bool active = false;
  active |= stepSpring(m_round.x, m_round.v, m_round.t, m_stiffness, m_damping,
                       dt, 0.001);
  active |= stepSpring(m_press.x, m_press.v, m_press.t, m_stiffness * 1.4,
                       m_damping, dt, 0.001);
  update();
  if (!active)
    m_driver->stop();
}

void MaterialIconButton::setType(Type v) {
  assign(m_type, v, &MaterialIconButton::typeChanged);
  m_iconDirty = true;
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
  if (assign(m_iconSource, v, &MaterialIconButton::iconSourceChanged))
    m_iconDirty = true;
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
  if (assign(m_iconSize, v, &MaterialIconButton::iconSizeChanged))
    m_iconDirty = true;
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

void MaterialIconButton::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialIconButton::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
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
  QQuickPaintedItem::hoverEnterEvent(e);
  if (isEnabled())
    setHovered(true);
}

void MaterialIconButton::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  setHovered(false);
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
  QQuickPaintedItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialIconButton::buildIcon(qreal dpr, const QColor &c) {
  const qreal sz = resolvedIconSize();
  const int px = qRound(sz * dpr);
  if (!m_iconDirty && px == m_iconPx && c == m_iconBuiltColor)
    return;
  m_iconDirty = false;
  m_iconPx = px;
  m_iconBuiltColor = c;
  m_icon = QImage();
  if (px <= 0 || !m_iconSource.isValid() || m_iconSource.isEmpty())
    return;

  const QString path = QQmlFile::urlToLocalFileOrQrc(m_iconSource);
  QImageReader r(path);
  QSize s = r.size();
  if (s.isValid()) {
    s.scale(px, px, Qt::KeepAspectRatio);
    r.setScaledSize(s);
  }
  const QImage src = r.read();
  if (src.isNull()) {
    qWarning("MaterialIconButton: failed to load icon '%s'", qPrintable(path));
    return;
  }

  QImage out(px, px, QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter q(&out);
  q.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QSize d = src.size();
  d.scale(px, px, Qt::KeepAspectRatio);
  q.drawImage(
      QRect((px - d.width()) / 2, (px - d.height()) / 2, d.width(), d.height()),
      src);
  q.setCompositionMode(QPainter::CompositionMode_SourceIn);
  q.fillRect(out.rect(), c);
  q.end();
  out.setDevicePixelRatio(dpr);
  m_icon = std::move(out);
}

void MaterialIconButton::paint(QPainter *p) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const qreal scale = 1.0 - kPressShrink * m_press.x;
  p->translate(w / 2.0, h / 2.0);
  p->scale(scale, scale);
  p->translate(-w / 2.0, -h / 2.0);

  const qreal full = std::min(w, h) / 2.0;
  const qreal sq = std::min(kSquareR[m_sizeStyle], full);
  const qreal radius = std::max(0.0, sq + (full - sq) * m_round.x);
  const QRectF rect(0.0, 0.0, w, h);

  QPainterPath path;
  path.addRoundedRect(rect, radius, radius);

  const QColor bg = containerColor();
  const QColor fg = contentColor();

  p->setPen(Qt::NoPen);
  if (bg.alpha() > 0) {
    p->setBrush(bg);
    p->drawPath(path);
  }

  const qreal bw = resolvedBorderWidth();
  if (m_type == Outlined && !m_checked && bw > 0.0 &&
      outlineColor().alpha() > 0) {
    const qreal half = bw / 2.0;
    const qreal br = std::max(0.0, radius - half);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(outlineColor(), bw));
    p->drawRoundedRect(rect.adjusted(half, half, -half, -half), br, br);
    p->setPen(Qt::NoPen);
  }

  qreal layer = 0.0;
  if (m_hovered)
    layer = 0.08;
  if (m_pressed || (m_keyFocus && hasActiveFocus()))
    layer = 0.10;
  if (layer > 0.0) {
    QColor l = fg;
    l.setAlphaF(layer);
    p->setBrush(l);
    p->drawPath(path);
  }

  const qreal iconSz = resolvedIconSize();
  if (iconSz <= 0.0)
    return;

  if (m_iconSource.isValid() && !m_iconSource.isEmpty()) {
    buildIcon(window() ? window()->devicePixelRatio() : 1.0, fg);
    if (!m_icon.isNull())
      p->drawImage(QPointF(w / 2.0 - iconSz / 2.0, h / 2.0 - iconSz / 2.0),
                   m_icon);
  }

  if (!m_iconText.isEmpty()) {
    QFont f = m_iconFont;
    f.setPixelSize(qMax(1, qRound(iconSz)));
    f.setHintingPreference(QFont::PreferNoHinting);

    QPainterPath gp;
    gp.addText(QPointF(0.0, 0.0), f, m_iconText);
    const QRectF br = gp.boundingRect();
    if (!br.isEmpty()) {
      const qreal k = std::min(1.0, iconSz / std::max(br.width(), br.height()));
      p->save();
      p->translate(w / 2.0, h / 2.0);
      p->scale(k, k);
      p->translate(-br.center());
      p->setPen(Qt::NoPen);
      p->setBrush(fg);
      p->drawPath(gp);
      p->restore();
    }
  }
}
