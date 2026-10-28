#include "./materialsearchbox.hpp"
#include <QClipboard>
#include <QCursor>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethod>
#include <QKeySequence>
#include <QPainter>
#include <QPen>
#include <QQmlEngine>
#include <QStringList>
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
constexpr int kBlinkMs = 530;
constexpr int kSlotLeading = 0;
constexpr int kSlotTrailing = 1;
constexpr int kSlotClear = 2;
constexpr int kSlotText = 3;
} // namespace

MaterialSearchBox::MaterialSearchBox(QQuickItem *parent)
    : SgMaterialItem(parent, 520.0, 0.6) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setFlag(ItemAcceptsInputMethod, true);
  setCursor(QCursor(Qt::IBeamCursor));
  setImplicitWidth(kDefaultW);
  setImplicitHeight(kHeight);

  m_blink = new QTimer(this);
  m_blink->setInterval(kBlinkMs);
  connect(m_blink, &QTimer::timeout, this, [this] {
    m_cursorOn = !m_cursorOn;
    update();
  });

  retarget(true);
}

QFont MaterialSearchBox::labelFont() const {
  return Ryu::styledFont(m_font, kFontPx, m_fontWeight);
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
  const QRectF bar(0.0, std::round((height() - kHeight) / 2.0), width(),
                   kHeight);
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

QImage MaterialSearchBox::icon(IconSlot &s, const QUrl &src, qreal size,
                               const QColor &color, qreal dpr) {
  if (s.path.isEmpty())
    s.path = Ryu::resolveUrl(qmlContext(this), src);
  return s.cache.get(s.path, size, dpr, color);
}

void MaterialSearchBox::iconLayer(Ryu::Mesh &m, int slot, IconSlot &s,
                                  const QUrl &src, qreal cx, qreal cy,
                                  qreal size) {
  const qreal dpr = m.dpr();
  const QColor col = Ryu::withAlpha(m_iconColor, m_iconColor.alphaF() * fade());
  const QString key =
      QStringList{"i", src.toString(), col.name(QColor::HexArgb),
                  QString::number(size), QString::number(dpr)}
          .join(QLatin1Char('|'));
  useLayer(slot, key,
           QRectF(m.snap(cx - size / 2.0), m.snap(cy - size / 2.0), size, size),
           [&] { return icon(s, src, size, col, dpr); });
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
  m_hov.go(m_hovered ? 1.0 : 0.0, immediate);
  m_foc.go(m_focused ? 1.0 : 0.0, immediate);
  m_clr.go(clearVisible() ? 1.0 : 0.0, immediate);
  if (!immediate)
    animate();
}

bool MaterialSearchBox::advance(qreal dt) {
  bool active = step(m_hov, dt);
  active |= step(m_foc, dt);
  active |= step(m_clr, dt, 0.001, 1.2);
  return active;
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
    m_leading.path.clear();
    relayout();
  }
}

void MaterialSearchBox::setTrailingIconSource(const QUrl &v) {
  if (assign(m_trailingIconSource, v,
             &MaterialSearchBox::trailingIconSourceChanged)) {
    m_trailing.path.clear();
    relayout();
  }
}

void MaterialSearchBox::setClearIconSource(const QUrl &v) {
  if (assign(m_clearIconSource, v, &MaterialSearchBox::clearIconSourceChanged))
    m_clearSlot.path.clear();
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
    return QQuickItem::inputMethodQuery(q);
  }
}

void MaterialSearchBox::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickItem::geometryChange(n, o);
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
  QQuickItem::hoverEnterEvent(e);
  if (!isEnabled())
    return;
  setHovered(true);
  m_hoverZone = zoneAt(e->position());
  updateCursorShape(m_hoverZone);
  update();
}

void MaterialSearchBox::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
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
  QQuickItem::hoverLeaveEvent(e);
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
  QQuickItem::focusInEvent(e);
  m_focused = true;
  resetBlink();
  retarget();
  update();
}

void MaterialSearchBox::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_focused = false;
  m_blink->stop();
  m_cursorOn = false;
  m_preedit.clear();
  retarget();
  update();
}

void MaterialSearchBox::build(Ryu::Mesh &m) {
  const qreal w = width(), h = height();
  if (w <= 0.0 || h <= 0.0)
    return;

  const qreal dpr = m.dpr();
  const QRectF bar(0.0, std::round((h - kHeight) / 2.0), w, kHeight);
  const qreal cy = h / 2.0;
  const qreal full = std::min(bar.width(), bar.height()) / 2.0;

  m.fillRoundedRect(bar, full, full, full, full, m_color);

  const qreal hov = std::clamp(m_hov.x, 0.0, 1.0);
  if (hov > 0.001)
    m.fillRoundedRect(bar, full, full, full, full,
                      Ryu::withAlpha(m_textColor, Ryu::kHoverLayer * hov));

  const qreal foc = std::clamp(m_foc.x, 0.0, 1.0);
  if (m_borderWidth > 0.0) {
    const QColor bc = Ryu::mix(m_borderColor, m_cursorColor, foc);
    if (bc.alpha() > 0)
      m.strokeRoundedRect(bar, full, full, full, full,
                          m.snapStroke(m_borderWidth), bc);
  }

  auto zoneLayer = [&](int zone, qreal cx, qreal scale) {
    const qreal a = Ryu::stateLayer(m_hoverZone == zone, m_pressZone == zone);
    if (a <= 0.0)
      return;
    m.fillCircle(QPointF(cx, cy), kHit / 2.0 * scale,
                 Ryu::withAlpha(m_iconColor, a));
  };

  if (leadingShown()) {
    const qreal cx = leadingCx();
    zoneLayer(Leading, cx, 1.0);
    if (!m_leadingIconSource.isEmpty()) {
      iconLayer(m, kSlotLeading, m_leading, m_leadingIconSource, cx, cy, kIcon);
    } else if (!m_leadingIconText.isEmpty()) {
      glyphLayer(kSlotLeading, m_iconFont, m_leadingIconText, m_iconColor,
                 QPointF(cx, cy), kIcon);
    } else {
      m.strokeCircle(QPointF(cx - 2.0, cy - 2.0), 7.5, m.snapStroke(2.0),
                     m_iconColor);
      m.line(QPointF(cx + 2.7, cy + 2.7), QPointF(cx + 8.0, cy + 8.0),
             m.snapStroke(2.0), m_iconColor);
    }
  }

  if (trailingShown()) {
    const qreal cx = trailingCx();
    zoneLayer(Trailing, cx, 1.0);
    if (!m_trailingIconSource.isEmpty())
      iconLayer(m, kSlotTrailing, m_trailing, m_trailingIconSource, cx, cy,
                kIcon);
    else
      glyphLayer(kSlotTrailing, m_iconFont, m_trailingIconText, m_iconColor,
                 QPointF(cx, cy), kIcon);
  }

  const qreal csRaw = std::clamp(m_clr.x, 0.0, 1.4);
  const qreal cs = std::min(csRaw, 1.0);
  if (csRaw > 0.01) {
    const qreal cx = clearCx();
    zoneLayer(Clear, cx, csRaw);
    if (!m_clearIconSource.isEmpty()) {
      if (cs > 0.5)
        iconLayer(m, kSlotClear, m_clearSlot, m_clearIconSource, cx, cy,
                  kClearIcon);
    } else {
      const qreal half = kClearHalf * csRaw;
      const QColor col = Ryu::withAlpha(m_iconColor, m_iconColor.alphaF() * cs);
      const qreal lw = m.snapStroke(2.0 * csRaw);
      m.line(QPointF(cx - half, cy - half), QPointF(cx + half, cy + half), lw,
             col);
      m.line(QPointF(cx - half, cy + half), QPointF(cx + half, cy - half), lw,
             col);
    }
  }

  const qreal tl = textLeft();
  const qreal tr = textRight();
  const QFont lf = labelFont();
  const QFontMetricsF fm(lf);
  const qreal iw = tr - tl;

  if (iw >= 1.0) {
    const QString disp = displayText();
    const bool ph = disp.isEmpty();
    const QString txt = ph ? m_placeholder : disp;
    const QColor tc = ph ? m_placeholderColor : m_textColor;
    const bool sel = m_preedit.isEmpty() && hasSelection();
    const int sa = sel ? selStart() : 0;
    const int sb = sel ? selEnd() : 0;
    const qreal rw = std::ceil(iw * dpr) / dpr;
    const qreal rh = bar.height();
    const qreal fd = fade();

    const QString key = QStringList{"s",
                                    txt,
                                    lf.key(),
                                    tc.name(QColor::HexArgb),
                                    QString::number(m_scroll, 'f', 2),
                                    QString::number(dpr),
                                    QString::number(rw),
                                    QString::number(rh),
                                    QString::number(sa),
                                    QString::number(sb),
                                    m_selectionColor.name(QColor::HexArgb),
                                    m_preedit,
                                    QString::number(m_cursor),
                                    QString::number(fd)}
                            .join(QLatin1Char('|'));

    useLayer(
        kSlotText, key, QRectF(m.snap(tl), m.snap(bar.top()), rw, rh), [&] {
          QImage img = Ryu::newImage(rw, rh, dpr);
          QPainter p(&img);
          p.setRenderHint(QPainter::Antialiasing, true);
          p.setRenderHint(QPainter::TextAntialiasing, true);
          p.setOpacity(fd);
          const qreal mid = rh / 2.0;
          p.setPen(Qt::NoPen);
          if (sel) {
            const qreal x0 = fm.horizontalAdvance(m_text.left(sa)) - m_scroll;
            const qreal x1 = fm.horizontalAdvance(m_text.left(sb)) - m_scroll;
            p.setBrush(m_selectionColor);
            p.drawRect(
                QRectF(x0, mid - fm.height() / 2.0, x1 - x0, fm.height()));
          }
          Ryu::drawLabel(&p, lf, txt, -m_scroll, mid, tc);
          if (!m_preedit.isEmpty()) {
            const qreal base = mid + (fm.ascent() - fm.descent()) / 2.0;
            const qreal x0 =
                fm.horizontalAdvance(m_text.left(m_cursor)) - m_scroll;
            const qreal x1 = cursorX() - m_scroll;
            p.setPen(QPen(m_textColor, 1.0));
            p.drawLine(QPointF(x0, base + 3.0), QPointF(x1, base + 3.0));
          }
          p.end();
          return img;
        });
  }

  if (m_focused && m_cursorOn && !m_readOnly) {
    const qreal x = tl + cursorX() - m_scroll;
    if (x >= tl - 1.0 && x <= tr + 1.0)
      m.fillRoundedRect(
          QRectF(m.snap(x - 1.0), cy - fm.height() / 2.0, 2.0, fm.height()),
          0.0, 0.0, 0.0, 0.0, m_cursorColor);
  }
}
