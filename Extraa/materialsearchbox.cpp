#include "./materialsearchbox.hpp"
#include "../Shapes/springdriver.hpp"
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImageReader>
#include <QInputMethod>
#include <QKeySequence>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlFile>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kHeight = 56.0;
constexpr qreal kPad = 16.0;
constexpr qreal kIcon = 24.0;
constexpr qreal kClearIcon = 20.0;
constexpr qreal kGap = 16.0;
constexpr qreal kFontPx = 16.0;
constexpr qreal kHit = 40.0;
constexpr qreal kDefaultW = 360.0;
constexpr qreal kClearHalf = 5.0;
constexpr qreal kMaxStep = 0.004;
constexpr int kBlinkMs = 530;

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

QColor mix(const QColor &a, const QColor &b, qreal t) {
  t = std::clamp(t, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QString resolvePath(const QQmlContext *ctx, const QUrl &u) {
  if (u.isLocalFile() || u.scheme().compare("qrc", Qt::CaseInsensitive) == 0)
    return QQmlFile::urlToLocalFileOrQrc(u);

  if (!u.scheme().isEmpty() && u.scheme().size() != 1)
    return {};

  QString p = u.toString();
  if (p.startsWith(QLatin1String("~/")))
    p = QDir::homePath() + p.mid(1);

  if (QDir::isAbsolutePath(p))
    return p;

  if (ctx) {
    const QString local =
        QQmlFile::urlToLocalFileOrQrc(ctx->resolvedUrl(QUrl(p)));
    if (!local.isEmpty() &&
        (local.startsWith(QLatin1Char(':')) || QFileInfo::exists(local)))
      return local;
  }

  return QFileInfo(p).absoluteFilePath();
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
} // namespace

MaterialSearchBox::MaterialSearchBox(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
  setAntialiasing(true);
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setFlag(ItemAcceptsInputMethod, true);
  setCursor(QCursor(Qt::IBeamCursor));
  setImplicitWidth(kDefaultW);
  setImplicitHeight(kHeight);

  auto *driver = new SpringDriver(this);
  driver->tick = [this](qreal dt) { tick(dt); };
  m_driver = driver;

  m_blink = new QTimer(this);
  m_blink->setInterval(kBlinkMs);
  connect(m_blink, &QTimer::timeout, this, [this] {
    m_cursorOn = !m_cursorOn;
    update();
  });

  retarget(true);
}

QFont MaterialSearchBox::labelFont() const {
  QFont f = m_font;
  f.setPixelSize(qRound(kFontPx));
  f.setWeight(QFont::Weight(m_fontWeight));
  f.setHintingPreference(QFont::PreferNoHinting);
  f.setStyleStrategy(
      QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality));
  return f;
}

bool MaterialSearchBox::leadingShown() const {
  return m_showSearchIcon || !m_leadingIconText.isEmpty() ||
         !m_leadingIconSource.isEmpty();
}

bool MaterialSearchBox::trailingShown() const {
  return !m_trailingIconText.isEmpty() || !m_trailingIconSource.isEmpty();
}

bool MaterialSearchBox::clearVisible() const {
  return m_clearable && !m_readOnly && !m_text.isEmpty();
}

qreal MaterialSearchBox::leadingCx() const { return kPad + kIcon / 2.0; }

qreal MaterialSearchBox::trailingCx() const {
  return width() - kPad - kIcon / 2.0;
}

qreal MaterialSearchBox::clearCx() const {
  return trailingShown() ? trailingCx() - kHit : trailingCx();
}

qreal MaterialSearchBox::textLeft() const {
  return leadingShown() ? kPad + kIcon + kGap : kPad;
}

qreal MaterialSearchBox::textRight() const {
  qreal r = width() - kPad;
  if (m_clearable)
    r = clearCx() - kIcon / 2.0 - kGap / 2.0;
  else if (trailingShown())
    r = trailingCx() - kIcon / 2.0 - kGap / 2.0;
  return std::max(textLeft(), r);
}

int MaterialSearchBox::zoneAt(const QPointF &pt) const {
  const qreal cy = height() / 2.0;
  const qreal half = kHit / 2.0;
  if (std::abs(pt.y() - cy) <= half) {
    if (leadingShown() && std::abs(pt.x() - leadingCx()) <= half)
      return Leading;
    if (trailingShown() && std::abs(pt.x() - trailingCx()) <= half)
      return Trailing;
    if (clearVisible() && std::abs(pt.x() - clearCx()) <= half)
      return Clear;
  }
  const QRectF bar(0.0, (height() - kHeight) / 2.0, width(), kHeight);
  return bar.contains(pt) ? Text : None;
}

QString MaterialSearchBox::displayText() const {
  if (m_preedit.isEmpty())
    return m_text;
  return m_text.left(m_cursor) + m_preedit + m_text.mid(m_cursor);
}

qreal MaterialSearchBox::cursorX() const {
  return QFontMetricsF(labelFont())
      .horizontalAdvance(m_text.left(m_cursor) + m_preedit);
}

int MaterialSearchBox::indexAt(qreal x) const {
  const qreal local = x - textLeft() + m_scroll;
  const QFontMetricsF fm(labelFont());
  int best = 0;
  qreal bestDist = std::abs(local);
  for (int i = 1; i <= m_text.size(); ++i) {
    const qreal d = std::abs(local - fm.horizontalAdvance(m_text.left(i)));
    if (d < bestDist) {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

int MaterialSearchBox::wordLeft(int i) const {
  while (i > 0 && m_text.at(i - 1).isSpace())
    --i;
  while (i > 0 && !m_text.at(i - 1).isSpace())
    --i;
  return i;
}

int MaterialSearchBox::wordRight(int i) const {
  const int n = m_text.size();
  while (i < n && !m_text.at(i).isSpace())
    ++i;
  while (i < n && m_text.at(i).isSpace())
    ++i;
  return i;
}

QString MaterialSearchBox::selectedText() const {
  return m_text.mid(selStart(), selEnd() - selStart());
}

const QImage &MaterialSearchBox::icon(IconCache &c, const QUrl &src, qreal size,
                                      qreal dpr) {
  const int px = qRound(size * dpr);
  if (!c.dirty && c.px == px && c.color == m_iconColor)
    return c.img;
  c.dirty = false;
  c.px = px;
  c.color = m_iconColor;
  c.img = QImage();
  if (px <= 0 || src.isEmpty())
    return c.img;

  const QString path = resolvePath(qmlContext(this), src);
  if (path.isEmpty()) {
    qWarning("MaterialSearchBox: could not resolve icon '%s'",
             qPrintable(src.toString()));
    return c.img;
  }

  QImageReader r(path);
  QSize s = r.size();
  if (s.isValid()) {
    s.scale(px, px, Qt::KeepAspectRatio);
    r.setScaledSize(s);
  }
  const QImage in = r.read();
  if (in.isNull()) {
    qWarning("MaterialSearchBox: failed to load icon '%s' (%s)",
             qPrintable(path), qPrintable(r.errorString()));
    return c.img;
  }

  QImage out(px, px, QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  QPainter q(&out);
  q.setRenderHint(QPainter::SmoothPixmapTransform, true);
  QSize d = in.size();
  d.scale(px, px, Qt::KeepAspectRatio);
  q.drawImage(
      QRect((px - d.width()) / 2, (px - d.height()) / 2, d.width(), d.height()),
      in);
  q.setCompositionMode(QPainter::CompositionMode_SourceIn);
  q.fillRect(out.rect(), m_iconColor);
  q.end();
  out.setDevicePixelRatio(dpr);
  c.img = std::move(out);
  return c.img;
}

void MaterialSearchBox::commit(const QString &t, int cur, bool user) {
  const bool changed = t != m_text;
  m_text = t;
  const int c = std::clamp(cur, 0, int(m_text.size()));
  const bool cursorMoved = c != m_cursor;
  m_cursor = m_anchor = c;
  if (changed) {
    emit textChanged();
    if (user)
      emit textEdited();
  }
  if (cursorMoved)
    emit cursorPositionChanged();
  retarget();
  afterCursor();
}

void MaterialSearchBox::insertText(QString s) {
  if (m_readOnly || s.isEmpty())
    return;
  QString t = m_text;
  int cur = m_cursor;
  if (hasSelection()) {
    const int a = selStart();
    t.remove(a, selEnd() - a);
    cur = a;
  }
  if (m_maxLength > 0) {
    const int room = m_maxLength - int(t.size());
    if (room <= 0)
      return;
    s = s.left(room);
  }
  t.insert(cur, s);
  commit(t, cur + int(s.size()), true);
}

bool MaterialSearchBox::deleteSelection() {
  if (m_readOnly || !hasSelection())
    return false;
  QString t = m_text;
  const int a = selStart();
  t.remove(a, selEnd() - a);
  commit(t, a, true);
  return true;
}

void MaterialSearchBox::moveCursor(int pos, bool select) {
  pos = std::clamp(pos, 0, int(m_text.size()));
  const bool changed = pos != m_cursor;
  m_cursor = pos;
  if (!select)
    m_anchor = pos;
  if (changed)
    emit cursorPositionChanged();
  afterCursor();
}

void MaterialSearchBox::afterCursor() {
  ensureVisible();
  resetBlink();
  notifyInputMethod();
  update();
}

void MaterialSearchBox::resetBlink() {
  m_cursorOn = true;
  if (m_focused)
    m_blink->start();
}

void MaterialSearchBox::notifyInputMethod() {
  if (!m_focused)
    return;
  QGuiApplication::inputMethod()->update(
      Qt::ImCursorRectangle | Qt::ImCursorPosition | Qt::ImSurroundingText |
      Qt::ImCurrentSelection | Qt::ImAnchorPosition);
}

void MaterialSearchBox::ensureVisible() {
  const qreal avail = std::max(0.0, textRight() - textLeft());
  const qreal cx = cursorX();
  const qreal total =
      QFontMetricsF(labelFont()).horizontalAdvance(displayText());
  if (cx - m_scroll > avail)
    m_scroll = cx - avail;
  if (cx - m_scroll < 0.0)
    m_scroll = cx;
  m_scroll = std::clamp(m_scroll, 0.0, std::max(0.0, total + 2.0 - avail));
}

void MaterialSearchBox::relayout() {
  ensureVisible();
  update();
}

void MaterialSearchBox::setHovered(bool v) {
  if (m_hovered == v)
    return;
  m_hovered = v;
  emit hoveredChanged();
  retarget();
  update();
}

void MaterialSearchBox::updateCursorShape(int zone) {
  if (zone == Text)
    setCursor(QCursor(Qt::IBeamCursor));
  else if (zone == None)
    setCursor(QCursor(Qt::ArrowCursor));
  else
    setCursor(QCursor(Qt::PointingHandCursor));
}

void MaterialSearchBox::retarget(bool immediate) {
  m_hov.t = m_hovered ? 1.0 : 0.0;
  m_foc.t = m_focused ? 1.0 : 0.0;
  m_clr.t = clearVisible() ? 1.0 : 0.0;
  if (immediate) {
    for (Spring *s : {&m_hov, &m_foc, &m_clr}) {
      s->x = s->t;
      s->v = 0.0;
    }
    return;
  }
  ensureDriver();
}

void MaterialSearchBox::ensureDriver() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

void MaterialSearchBox::tick(qreal dt) {
  if (dt <= 0.0)
    return;
  bool active = false;
  active |=
      stepSpring(m_hov.x, m_hov.v, m_hov.t, m_stiffness, m_damping, dt, 0.001);
  active |=
      stepSpring(m_foc.x, m_foc.v, m_foc.t, m_stiffness, m_damping, dt, 0.001);
  active |= stepSpring(m_clr.x, m_clr.v, m_clr.t, m_stiffness * 1.2, m_damping,
                       dt, 0.001);
  update();
  if (!active)
    m_driver->stop();
}

void MaterialSearchBox::setText(const QString &v) {
  QString t = v;
  if (m_maxLength > 0)
    t = t.left(m_maxLength);
  if (t == m_text)
    return;
  m_preedit.clear();
  commit(t, int(t.size()), false);
}

void MaterialSearchBox::setPlaceholderText(const QString &v) {
  assign(m_placeholder, v, &MaterialSearchBox::placeholderTextChanged);
}

void MaterialSearchBox::setFont(const QFont &v) {
  if (assign(m_font, v, &MaterialSearchBox::fontChanged))
    relayout();
}

void MaterialSearchBox::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &MaterialSearchBox::fontWeightChanged))
    relayout();
}

void MaterialSearchBox::setIconFont(const QFont &v) {
  assign(m_iconFont, v, &MaterialSearchBox::iconFontChanged);
}

void MaterialSearchBox::setLeadingIconText(const QString &v) {
  if (assign(m_leadingIconText, v, &MaterialSearchBox::leadingIconTextChanged))
    relayout();
}

void MaterialSearchBox::setTrailingIconText(const QString &v) {
  if (assign(m_trailingIconText, v,
             &MaterialSearchBox::trailingIconTextChanged))
    relayout();
}

void MaterialSearchBox::setLeadingIconSource(const QUrl &v) {
  if (assign(m_leadingIconSource, v,
             &MaterialSearchBox::leadingIconSourceChanged)) {
    m_leadingCache.dirty = true;
    relayout();
  }
}

void MaterialSearchBox::setTrailingIconSource(const QUrl &v) {
  if (assign(m_trailingIconSource, v,
             &MaterialSearchBox::trailingIconSourceChanged)) {
    m_trailingCache.dirty = true;
    relayout();
  }
}

void MaterialSearchBox::setClearIconSource(const QUrl &v) {
  if (assign(m_clearIconSource, v, &MaterialSearchBox::clearIconSourceChanged))
    m_clearCache.dirty = true;
}

void MaterialSearchBox::setShowSearchIcon(bool v) {
  if (assign(m_showSearchIcon, v, &MaterialSearchBox::showSearchIconChanged))
    relayout();
}

void MaterialSearchBox::setClearable(bool v) {
  if (assign(m_clearable, v, &MaterialSearchBox::clearableChanged)) {
    retarget();
    relayout();
  }
}

void MaterialSearchBox::setReadOnly(bool v) {
  if (assign(m_readOnly, v, &MaterialSearchBox::readOnlyChanged))
    retarget();
}

void MaterialSearchBox::setMaxLength(int v) {
  v = std::max(0, v);
  if (!assign(m_maxLength, v, &MaterialSearchBox::maxLengthChanged))
    return;
  if (v > 0 && m_text.size() > v)
    commit(m_text.left(v), std::min(m_cursor, v), false);
}

void MaterialSearchBox::setColor(const QColor &v) {
  assign(m_color, v, &MaterialSearchBox::colorChanged);
}

void MaterialSearchBox::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialSearchBox::textColorChanged);
}

void MaterialSearchBox::setPlaceholderColor(const QColor &v) {
  assign(m_placeholderColor, v, &MaterialSearchBox::placeholderColorChanged);
}

void MaterialSearchBox::setIconColor(const QColor &v) {
  assign(m_iconColor, v, &MaterialSearchBox::iconColorChanged);
}

void MaterialSearchBox::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialSearchBox::borderColorChanged);
}

void MaterialSearchBox::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialSearchBox::borderWidthChanged);
}

void MaterialSearchBox::setCursorColor(const QColor &v) {
  assign(m_cursorColor, v, &MaterialSearchBox::cursorColorChanged);
}

void MaterialSearchBox::setSelectionColor(const QColor &v) {
  assign(m_selectionColor, v, &MaterialSearchBox::selectionColorChanged);
}

void MaterialSearchBox::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialSearchBox::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
}

void MaterialSearchBox::clear() {
  if (m_text.isEmpty())
    return;
  m_preedit.clear();
  commit(QString(), 0, true);
  emit cleared();
}

void MaterialSearchBox::selectAll() {
  m_anchor = 0;
  moveCursor(int(m_text.size()), true);
}

QVariant MaterialSearchBox::inputMethodQuery(Qt::InputMethodQuery q) const {
  switch (q) {
  case Qt::ImEnabled:
    return isEnabled() && !m_readOnly;
  case Qt::ImCursorRectangle: {
    const QFontMetricsF fm(labelFont());
    return QRectF(textLeft() + cursorX() - m_scroll,
                  height() / 2.0 - fm.height() / 2.0, 1.0, fm.height());
  }
  case Qt::ImFont:
    return labelFont();
  case Qt::ImCursorPosition:
    return m_cursor;
  case Qt::ImSurroundingText:
    return m_text;
  case Qt::ImCurrentSelection:
    return selectedText();
  case Qt::ImAnchorPosition:
    return m_anchor;
  default:
    return QQuickPaintedItem::inputMethodQuery(q);
  }
}

void MaterialSearchBox::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickPaintedItem::geometryChange(n, o);
  if (n.size() != o.size())
    relayout();
}

void MaterialSearchBox::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }
  const int z = zoneAt(e->position());
  if (z == None) {
    e->ignore();
    return;
  }
  forceActiveFocus(Qt::MouseFocusReason);
  m_pressZone = z;
  if (z == Text)
    moveCursor(indexAt(e->position().x()), e->modifiers() & Qt::ShiftModifier);
  update();
  e->accept();
}

void MaterialSearchBox::mouseMoveEvent(QMouseEvent *e) {
  if (m_pressZone != Text)
    return;
  moveCursor(indexAt(e->position().x()), true);
  e->accept();
}

void MaterialSearchBox::mouseReleaseEvent(QMouseEvent *e) {
  if (m_pressZone == None)
    return;
  const int pressed = m_pressZone;
  m_pressZone = None;
  const bool inside = zoneAt(e->position()) == pressed;
  update();
  if (inside) {
    if (pressed == Leading)
      emit leadingClicked();
    else if (pressed == Trailing)
      emit trailingClicked();
    else if (pressed == Clear)
      clear();
  }
  e->accept();
}

void MaterialSearchBox::mouseDoubleClickEvent(QMouseEvent *e) {
  if (zoneAt(e->position()) == Text) {
    selectAll();
    e->accept();
    return;
  }
  e->ignore();
}

void MaterialSearchBox::mouseUngrabEvent() {
  m_pressZone = None;
  update();
}

void MaterialSearchBox::hoverEnterEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverEnterEvent(e);
  if (!isEnabled())
    return;
  setHovered(true);
  m_hoverZone = zoneAt(e->position());
  updateCursorShape(m_hoverZone);
  update();
}

void MaterialSearchBox::hoverMoveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverMoveEvent(e);
  if (!isEnabled())
    return;
  const int z = zoneAt(e->position());
  if (z != m_hoverZone) {
    m_hoverZone = z;
    updateCursorShape(z);
    update();
  }
}

void MaterialSearchBox::hoverLeaveEvent(QHoverEvent *e) {
  QQuickPaintedItem::hoverLeaveEvent(e);
  m_hoverZone = None;
  setHovered(false);
  update();
}

void MaterialSearchBox::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled()) {
    e->ignore();
    return;
  }

  const bool shift = e->modifiers() & Qt::ShiftModifier;
  const bool ctrl = e->modifiers() & Qt::ControlModifier;

  if (e->matches(QKeySequence::SelectAll)) {
    selectAll();
  } else if (e->matches(QKeySequence::Copy)) {
    if (hasSelection())
      QGuiApplication::clipboard()->setText(selectedText());
  } else if (e->matches(QKeySequence::Cut)) {
    if (!m_readOnly && hasSelection()) {
      QGuiApplication::clipboard()->setText(selectedText());
      deleteSelection();
    }
  } else if (e->matches(QKeySequence::Paste)) {
    QString s = QGuiApplication::clipboard()->text();
    s.replace(QLatin1Char('\r'), QLatin1Char(' '))
        .replace(QLatin1Char('\n'), QLatin1Char(' '));
    insertText(s);
  } else {
    switch (e->key()) {
    case Qt::Key_Left:
      if (hasSelection() && !shift)
        moveCursor(selStart(), false);
      else
        moveCursor(ctrl ? wordLeft(m_cursor) : m_cursor - 1, shift);
      break;
    case Qt::Key_Right:
      if (hasSelection() && !shift)
        moveCursor(selEnd(), false);
      else
        moveCursor(ctrl ? wordRight(m_cursor) : m_cursor + 1, shift);
      break;
    case Qt::Key_Home:
      moveCursor(0, shift);
      break;
    case Qt::Key_End:
      moveCursor(int(m_text.size()), shift);
      break;
    case Qt::Key_Backspace:
      if (!deleteSelection() && !m_readOnly && m_cursor > 0) {
        const int from = ctrl ? wordLeft(m_cursor) : m_cursor - 1;
        QString t = m_text;
        t.remove(from, m_cursor - from);
        commit(t, from, true);
      }
      break;
    case Qt::Key_Delete:
      if (!deleteSelection() && !m_readOnly && m_cursor < m_text.size()) {
        const int to = ctrl ? wordRight(m_cursor) : m_cursor + 1;
        QString t = m_text;
        t.remove(m_cursor, to - m_cursor);
        commit(t, m_cursor, true);
      }
      break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      emit accepted();
      break;
    default: {
      const QString s = e->text();
      if (!s.isEmpty() && s.at(0).isPrint() &&
          !(e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) {
        insertText(s);
      } else {
        e->ignore();
        return;
      }
    }
    }
  }
  e->accept();
}

void MaterialSearchBox::inputMethodEvent(QInputMethodEvent *e) {
  if (m_readOnly || !isEnabled()) {
    e->ignore();
    return;
  }

  if (e->replacementLength() > 0) {
    const int start =
        std::clamp(m_cursor + e->replacementStart(), 0, int(m_text.size()));
    const int len =
        std::min(e->replacementLength(), int(m_text.size()) - start);
    if (len > 0) {
      QString t = m_text;
      t.remove(start, len);
      commit(t, start, true);
    }
  }

  if (!e->commitString().isEmpty()) {
    m_preedit.clear();
    insertText(e->commitString());
  }

  m_preedit = e->preeditString();
  ensureVisible();
  notifyInputMethod();
  update();
  e->accept();
}

void MaterialSearchBox::focusInEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusInEvent(e);
  m_focused = true;
  resetBlink();
  retarget();
  update();
}

void MaterialSearchBox::focusOutEvent(QFocusEvent *e) {
  QQuickPaintedItem::focusOutEvent(e);
  m_focused = false;
  m_blink->stop();
  m_cursorOn = false;
  m_preedit.clear();
  retarget();
  update();
}

void MaterialSearchBox::paint(QPainter *p) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  if (!isEnabled())
    p->setOpacity(0.38);

  const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
  const QRectF bar(0.0, (h - kHeight) / 2.0, w, kHeight);
  const qreal cy = h / 2.0;
  const qreal full = std::min(bar.width(), bar.height()) / 2.0;

  QPainterPath path;
  path.addRoundedRect(bar, full, full);

  p->setPen(Qt::NoPen);
  p->setBrush(m_color);
  p->drawPath(path);

  const qreal hov = std::clamp(m_hov.x, 0.0, 1.0);
  if (hov > 0.001) {
    QColor l = m_textColor;
    l.setAlphaF(0.08 * hov);
    p->setBrush(l);
    p->drawPath(path);
  }

  const qreal foc = std::clamp(m_foc.x, 0.0, 1.0);
  if (m_borderWidth > 0.0) {
    const QColor bc = mix(m_borderColor, m_cursorColor, foc);
    if (bc.alpha() > 0) {
      const qreal half = m_borderWidth / 2.0;
      const qreal br = std::max(0.0, full - half);
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(bc, m_borderWidth));
      p->drawRoundedRect(bar.adjusted(half, half, -half, -half), br, br);
      p->setPen(Qt::NoPen);
    }
  }

  auto layer = [&](int zone, qreal cx, qreal scale) {
    qreal a = 0.0;
    if (m_hoverZone == zone)
      a = 0.08;
    if (m_pressZone == zone)
      a = 0.10;
    if (a <= 0.0)
      return;
    QColor l = m_iconColor;
    l.setAlphaF(a);
    p->setBrush(l);
    p->setPen(Qt::NoPen);
    p->drawEllipse(QPointF(cx, cy), kHit / 2.0 * scale, kHit / 2.0 * scale);
  };

  if (leadingShown()) {
    const qreal cx = leadingCx();
    layer(Leading, cx, 1.0);
    if (!m_leadingIconSource.isEmpty()) {
      const QImage &img = icon(m_leadingCache, m_leadingIconSource, kIcon, dpr);
      if (!img.isNull())
        p->drawImage(QPointF(cx - kIcon / 2.0, cy - kIcon / 2.0), img);
    } else if (!m_leadingIconText.isEmpty()) {
      drawGlyph(p, m_iconFont, m_leadingIconText, QPointF(cx, cy), kIcon,
                m_iconColor);
    } else {
      p->save();
      p->setBrush(Qt::NoBrush);
      p->setPen(
          QPen(m_iconColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p->drawEllipse(QPointF(cx - 2.0, cy - 2.0), 6.5, 6.5);
      p->drawLine(QPointF(cx + 2.7, cy + 2.7), QPointF(cx + 8.0, cy + 8.0));
      p->restore();
    }
  }

  if (trailingShown()) {
    const qreal cx = trailingCx();
    layer(Trailing, cx, 1.0);
    if (!m_trailingIconSource.isEmpty()) {
      const QImage &img =
          icon(m_trailingCache, m_trailingIconSource, kIcon, dpr);
      if (!img.isNull())
        p->drawImage(QPointF(cx - kIcon / 2.0, cy - kIcon / 2.0), img);
    } else {
      drawGlyph(p, m_iconFont, m_trailingIconText, QPointF(cx, cy), kIcon,
                m_iconColor);
    }
  }

  const qreal csRaw = std::clamp(m_clr.x, 0.0, 1.4);
  const qreal cs = std::min(csRaw, 1.0);
  if (csRaw > 0.01) {
    const qreal cx = clearCx();
    layer(Clear, cx, csRaw);
    p->save();
    p->translate(cx, cy);
    p->scale(csRaw, csRaw);
    if (!m_clearIconSource.isEmpty()) {
      const QImage &img =
          icon(m_clearCache, m_clearIconSource, kClearIcon, dpr);
      if (!img.isNull()) {
        p->setOpacity(p->opacity() * cs);
        p->drawImage(QPointF(-kClearIcon / 2.0, -kClearIcon / 2.0), img);
      }
    } else {
      QColor c = m_iconColor;
      c.setAlphaF(c.alphaF() * cs);
      p->setBrush(Qt::NoBrush);
      p->setPen(QPen(c, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p->drawLine(QPointF(-kClearHalf, -kClearHalf),
                  QPointF(kClearHalf, kClearHalf));
      p->drawLine(QPointF(-kClearHalf, kClearHalf),
                  QPointF(kClearHalf, -kClearHalf));
    }
    p->restore();
  }

  const qreal tl = textLeft();
  const qreal tr = textRight();
  const QFont lf = labelFont();
  const QFontMetricsF fm(lf);
  const qreal base = cy + (fm.ascent() - fm.descent()) / 2.0;
  const QString disp = displayText();

  p->save();
  p->setClipRect(QRectF(tl, bar.top(), tr - tl, bar.height()));
  p->translate(tl - m_scroll, 0.0);
  p->setPen(Qt::NoPen);

  if (m_preedit.isEmpty() && hasSelection()) {
    const qreal x0 = fm.horizontalAdvance(m_text.left(selStart()));
    const qreal x1 = fm.horizontalAdvance(m_text.left(selEnd()));
    p->setBrush(m_selectionColor);
    p->drawRect(QRectF(x0, cy - fm.height() / 2.0, x1 - x0, fm.height()));
  }

  const bool showPlaceholder = disp.isEmpty();
  const QString shown = showPlaceholder ? m_placeholder : disp;
  if (!shown.isEmpty()) {
    QPainterPath tp;
    tp.addText(QPointF(0.0, base), lf, shown);
    p->setBrush(showPlaceholder ? m_placeholderColor : m_textColor);
    p->drawPath(tp);
  }

  if (!m_preedit.isEmpty()) {
    const qreal x0 = fm.horizontalAdvance(m_text.left(m_cursor));
    const qreal x1 = cursorX();
    p->setPen(QPen(m_textColor, 1.0));
    p->drawLine(QPointF(x0, base + 3.0), QPointF(x1, base + 3.0));
    p->setPen(Qt::NoPen);
  }

  if (m_focused && m_cursorOn && !m_readOnly) {
    p->setBrush(m_cursorColor);
    p->drawRect(
        QRectF(cursorX() - 1.0, cy - fm.height() / 2.0, 2.0, fm.height()));
  }
  p->restore();
}
