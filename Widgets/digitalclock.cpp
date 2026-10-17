#include "./digitalclock.hpp"
#include "../Core/paintutil.hpp"
#include <QList>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTime>
#include <QTransform>
#include <algorithm>

namespace {
constexpr qreal kRefPx = 100.0;
constexpr qreal kPeriodScale = 0.30;
constexpr qreal kSepScale = 0.80;
constexpr int kSlackMs = 20;

using Group = QList<QPainterPath>;

QPainterPath cutShape(const QPainterPath &shape, qreal width) {
  QPainterPathStroker stroker;
  stroker.setWidth(width);
  stroker.setJoinStyle(Qt::RoundJoin);
  stroker.setCapStyle(Qt::RoundCap);
  QPainterPath cut = stroker.createStroke(shape);
  cut.setFillRule(Qt::WindingFill);
  return cut.united(shape);
}

Group buildGroup(const QString &s, const QFont &font, qreal ov, qreal x0,
                 qreal &end) {
  Group g;
  qreal x = x0;
  end = x0;
  for (const QChar c : s) {
    QPainterPath p;
    p.addText(QPointF(0.0, 0.0), font, QString(c));
    const QRectF r = p.boundingRect();
    if (r.isEmpty())
      continue;
    p.translate(x - r.left(), 0.0);
    g.append(p);
    end = p.boundingRect().right();
    x = end - ov;
  }
  return g;
}

QRectF groupBounds(const Group &g) {
  QRectF r;
  for (const QPainterPath &p : g)
    r = r.united(p.boundingRect());
  return r;
}

void shiftGroup(Group &g, qreal dx, qreal dy) {
  for (QPainterPath &p : g)
    p.translate(dx, dy);
}
} // namespace

DigitalClock::DigitalClock(QQuickItem *parent) : RyuItem(parent) {
  m_timer.setSingleShot(true);
  m_timer.setTimerType(Qt::CoarseTimer);
  connect(&m_timer, &QTimer::timeout, this, &DigitalClock::refresh);
  refresh();
}

void DigitalClock::componentComplete() {
  RyuItem::componentComplete();
  syncTimer();
}

void DigitalClock::geometryChange(const QRectF &n, const QRectF &o) {
  RyuItem::geometryChange(n, o);
  if (n.size() != o.size())
    m_dirty = true;
}

void DigitalClock::itemChange(ItemChange change, const ItemChangeData &data) {
  RyuItem::itemChange(change, data);
  if (change == ItemVisibleHasChanged || change == ItemSceneChange)
    syncTimer();
}

bool DigitalClock::timerWanted() const {
  return m_running && m_testTime.isEmpty() && isComponentComplete() &&
         isVisible() && window();
}

void DigitalClock::syncTimer() {
  if (!timerWanted()) {
    m_timer.stop();
    return;
  }
  if (!m_timer.isActive())
    refresh();
}

void DigitalClock::refresh() {
  QTime t = QTime::currentTime();
  if (!m_testTime.isEmpty()) {
    const QTime p = QTime::fromString(m_testTime, QStringLiteral("HH:mm"));
    if (p.isValid())
      t = p;
  }

  int h = t.hour();
  const QString per = h >= 12 ? QStringLiteral("PM") : QStringLiteral("AM");
  if (!m_24h) {
    h %= 12;
    if (h == 0)
      h = 12;
  }
  const QString hh = QString::number(h).rightJustified(2, QLatin1Char('0'));
  const QString mm =
      QString::number(t.minute()).rightJustified(2, QLatin1Char('0'));

  if (hh != m_hours || mm != m_minutes || per != m_period) {
    m_hours = hh;
    m_minutes = mm;
    m_period = per;
    m_dirty = true;
    emit timeChanged();
    update();
  }

  if (timerWanted())
    m_timer.start((60 - t.second()) * 1000 - t.msec() + kSlackMs);
  else
    m_timer.stop();
}

void DigitalClock::rebuildVertical(const QFont &f) {
  QPainterPath a, b;
  a.addText(QPointF(0.0, 0.0), f, m_hours);
  b.addText(QPointF(0.0, 0.0), f, m_minutes);

  const QRectF ra = a.boundingRect();
  const QRectF rb = b.boundingRect();
  if (ra.isEmpty() || rb.isEmpty())
    return;

  QPainterPath zero;
  zero.addText(QPointF(0.0, 0.0), f, QStringLiteral("0"));
  const qreal H = zero.boundingRect().height();

  a.translate(-ra.center().x(), -ra.top());
  b.translate(-rb.center().x(), ra.height() * (1.0 - m_overlap) - rb.top());

  const QRectF A = a.boundingRect();
  const QRectF B = b.boundingRect();

  Group period;
  if (wantsPeriod()) {
    QFont pf = f;
    pf.setPixelSize(qRound(kRefPx * kPeriodScale));
    qreal pend = 0.0;
    period = buildGroup(m_period, pf, m_overlap * H * kPeriodScale, 0.0, pend);
    if (!period.isEmpty()) {
      const QRectF pb = groupBounds(period);
      const qreal pad = ra.height() * 0.10;
      switch (m_amPmStyle) {
      case Beside:
        shiftGroup(period, B.right() + pad - pb.left(),
                   B.center().y() - pb.center().y());
        break;
      case Below:
        shiftGroup(period, B.center().x() - pb.center().x(),
                   B.bottom() + pad - pb.top());
        break;
      case Superscript:
        shiftGroup(period, A.right() + pad - pb.left(), A.top() - pb.top());
        break;
      }
    }
  }

  const qreal w = width() - 2.0 * m_padding;
  const qreal h = height() - 2.0 * m_padding;
  QRectF total = A.united(B);
  if (!period.isEmpty())
    total = total.united(groupBounds(period));
  if (w <= 0.0 || h <= 0.0 || total.isEmpty())
    return;

  const qreal k = std::min(w / total.width(), h / total.height());

  QTransform tf;
  tf.translate(width() / 2.0, height() / 2.0);
  tf.scale(k, k);
  tf.translate(-total.center().x(), -total.center().y());

  a = tf.map(a);
  b = tf.map(b);
  for (QPainterPath &p : period)
    p = tf.map(p);

  // carve the AM/PM glyphs the same way horizontal does
  const qreal cutW = 2.0 * m_gap * H * kPeriodScale * k;
  for (int i = 1; i < period.size(); ++i) {
    const QPainterPath cut = cutShape(period[i], cutW);
    for (int j = 0; j < i; ++j)
      period[j] = period[j].subtracted(cut);
  }

  QPainterPath extra;
  for (const QPainterPath &p : period)
    extra.addPath(p);

  m_top = a.subtracted(cutShape(b, 2.0 * m_gap * rb.height() * k));
  m_bottom = b;
  m_extra = extra;
}
void DigitalClock::rebuildHorizontal(const QFont &f) {
  QPainterPath zero;
  zero.addText(QPointF(0.0, 0.0), f, QStringLiteral("0"));
  const qreal H = zero.boundingRect().height();
  if (H <= 0.0)
    return;

  const qreal ov = m_overlap * H;
  const qreal gx = m_groupGap * H;

  qreal end = 0.0;
  Group hours = buildGroup(m_hours, f, ov, 0.0, end);
  if (hours.isEmpty())
    return;

  const QRectF hb = groupBounds(hours);
  const qreal midY = hb.center().y();
  qreal x = end + gx;

  QPainterPath colon;
  if (m_showSep) {
    QFont sf = f;
    sf.setPixelSize(qRound(kRefPx * kSepScale));
    colon.addText(QPointF(0.0, 0.0), sf, QStringLiteral(":"));
    const QRectF rs = colon.boundingRect();
    colon.translate(x - rs.left(), midY - rs.center().y());
    x = colon.boundingRect().right() + gx;
  }

  Group mins = buildGroup(m_minutes, f, ov, x, end);
  if (mins.isEmpty())
    return;
  const QRectF mb = groupBounds(mins);

  Group period;
  if (wantsPeriod()) {
    QFont pf = f;
    pf.setPixelSize(qRound(kRefPx * kPeriodScale));
    pf.setLetterSpacing(QFont::AbsoluteSpacing, m_spacing * pf.pixelSize());
    const qreal pov = ov * kPeriodScale;
    qreal pend = 0.0;
    period = buildGroup(m_period, pf, pov, 0.0, pend);
    if (!period.isEmpty()) {
      const QRectF pb = groupBounds(period);
      const QRectF all = hb.united(mb);
      switch (m_amPmStyle) {
      case Beside:
        shiftGroup(period, mb.right() + gx - pb.left(),
                   mb.bottom() - pb.bottom());
        break;
      case Superscript:
        shiftGroup(period, mb.right() + gx - pb.left(), mb.top() - pb.top());
        break;
      case Below:
        shiftGroup(period, all.center().x() - pb.center().x(),
                   all.bottom() + gx * 0.7 - pb.top());
        break;
      }
    }
  }

  QRectF total = hb.united(mb);
  if (!colon.isEmpty())
    total = total.united(colon.boundingRect());
  if (!period.isEmpty())
    total = total.united(groupBounds(period));

  const qreal w = width() - 2.0 * m_padding;
  const qreal h = height() - 2.0 * m_padding;
  if (w <= 0.0 || h <= 0.0 || total.isEmpty())
    return;

  const qreal k = std::min(w / total.width(), h / total.height());

  QTransform tf;
  tf.translate(width() / 2.0, height() / 2.0);
  tf.scale(k, k);
  tf.translate(-total.center().x(), -total.center().y());

  auto apply = [&](Group &g) {
    for (QPainterPath &p : g)
      p = tf.map(p);
  };
  apply(hours);
  apply(mins);
  apply(period);
  colon = tf.map(colon);

  const qreal cutW = 2.0 * m_gap * H * k;
  auto carve = [&](Group &g) {
    for (int i = 1; i < g.size(); ++i) {
      const QPainterPath cut = cutShape(g[i], cutW);
      for (int j = 0; j < i; ++j)
        g[j] = g[j].subtracted(cut);
    }
  };
  carve(hours);
  carve(mins);
  carve(period);

  QPainterPath all;
  for (const Group *g : {&hours, &mins, &period})
    for (const QPainterPath &p : *g)
      all.addPath(p);
  all.addPath(colon);

  m_top = all;
}

void DigitalClock::rebuild() {
  m_dirty = false;
  m_top = QPainterPath();
  m_bottom = QPainterPath();
  m_extra = QPainterPath();

  if (m_hours.isEmpty() || width() - 2.0 * m_padding <= 0.0 ||
      height() - 2.0 * m_padding <= 0.0)
    return;

  QFont f = Ryu::styledFont(m_font, kRefPx, m_fontWeight);
  f.setLetterSpacing(QFont::AbsoluteSpacing, m_spacing * kRefPx);

  if (m_orientation == Vertical)
    rebuildVertical(f);
  else
    rebuildHorizontal(f);
}

void DigitalClock::paint(QPainter *p) {
  if (m_dirty)
    rebuild();
  if (m_top.isEmpty() && m_bottom.isEmpty())
    return;

  preparePainter(p);
  p->setPen(Qt::NoPen);
  p->setBrush(m_color);
  p->drawPath(m_top);
  p->drawPath(m_bottom);
  if (!m_extra.isEmpty())
    p->drawPath(m_extra);
}

void DigitalClock::setOrientation(Orientation v) {
  if (assign(m_orientation, v, &DigitalClock::orientationChanged))
    m_dirty = true;
}

void DigitalClock::setUse24Hour(bool v) {
  if (assign(m_24h, v, &DigitalClock::use24HourChanged)) {
    m_dirty = true;
    refresh();
  }
}

void DigitalClock::setShowAmPm(bool v) {
  if (assign(m_showAmPm, v, &DigitalClock::showAmPmChanged))
    m_dirty = true;
}

void DigitalClock::setAmPmStyle(AmPmStyle v) {
  if (assign(m_amPmStyle, v, &DigitalClock::amPmStyleChanged))
    m_dirty = true;
}

void DigitalClock::setShowSeparator(bool v) {
  if (assign(m_showSep, v, &DigitalClock::showSeparatorChanged))
    m_dirty = true;
}

void DigitalClock::setTestTime(const QString &v) {
  if (assign(m_testTime, v, &DigitalClock::testTimeChanged))
    refresh();
}

void DigitalClock::setRunning(bool v) {
  if (assign(m_running, v, &DigitalClock::runningChanged))
    syncTimer();
}

void DigitalClock::setColor(const QColor &v) {
  assign(m_color, v, &DigitalClock::colorChanged);
}

void DigitalClock::setFont(const QFont &v) {
  if (assign(m_font, v, &DigitalClock::fontChanged))
    m_dirty = true;
}

void DigitalClock::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  if (assign(m_fontWeight, v, &DigitalClock::fontWeightChanged))
    m_dirty = true;
}

void DigitalClock::setGap(qreal v) {
  if (assign(m_gap, std::max(0.0, v), &DigitalClock::gapChanged))
    m_dirty = true;
}

void DigitalClock::setOverlap(qreal v) {
  if (assign(m_overlap, std::clamp(v, -1.0, 1.0),
             &DigitalClock::overlapChanged))
    m_dirty = true;
}

void DigitalClock::setGroupGap(qreal v) {
  if (assign(m_groupGap, std::max(0.0, v), &DigitalClock::groupGapChanged))
    m_dirty = true;
}

void DigitalClock::setLetterSpacing(qreal v) {
  if (assign(m_spacing, v, &DigitalClock::letterSpacingChanged))
    m_dirty = true;
}

void DigitalClock::setPadding(qreal v) {
  if (assign(m_padding, std::max(0.0, v), &DigitalClock::paddingChanged))
    m_dirty = true;
}
