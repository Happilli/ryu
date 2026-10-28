#include "./digitalclock.hpp"
#include "../Core/paintutil.hpp"
#include <QList>
#include <QPainter>
#include <QPen>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QTime>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace {
constexpr qreal kRefPx = 100.0;
constexpr qreal kBuildW = 1000.0;
constexpr qreal kPeriodScale = 0.30;
constexpr qreal kSepScale = 0.80;
constexpr qreal kMinCut = 0.01;
constexpr int kSlackMs = 20;

using Group = QList<QPainterPath>;

QPainterPath textPath(const QFont &font, const QString &s) {
  QPainterPath p;
  p.setFillRule(Qt::WindingFill);
  p.addText(QPointF(0.0, 0.0), font, s);
  return p;
}

Group buildGroup(const QString &s, const QFont &font, qreal ov, qreal sp,
                 qreal x0, qreal &end) {
  Group g;
  qreal x = x0;
  end = x0;
  for (const QChar c : s) {
    QPainterPath p = textPath(font, QString(c));
    const QRectF r = p.boundingRect();
    if (r.isEmpty())
      continue;
    p.translate(x - r.left(), 0.0);
    g.append(p);
    end = p.boundingRect().right();
    x = end - ov + sp;
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

void mapGroup(Group &g, const QTransform &tf) {
  for (QPainterPath &p : g)
    p = tf.map(p);
}
} // namespace

DigitalClock::DigitalClock(QQuickItem *parent) : SgItem(parent) {
  m_timer.setSingleShot(true);
  m_timer.setTimerType(Qt::CoarseTimer);
  connect(&m_timer, &QTimer::timeout, this, &DigitalClock::refresh);
  refresh();
}

void DigitalClock::componentComplete() {
  SgItem::componentComplete();
  syncTimer();
}

void DigitalClock::itemChange(ItemChange change, const ItemChangeData &data) {
  SgItem::itemChange(change, data);
  if (change == ItemEnabledHasChanged || change == ItemSceneChange)
    m_imgStale = true;
  if (change == ItemVisibleHasChanged || change == ItemSceneChange)
    syncTimer();
}

void DigitalClock::geometryChange(const QRectF &n, const QRectF &o) {
  SgItem::geometryChange(n, o);
  if (n.size() != o.size()) {
    m_imgStale = true;
    update();
  }
}

void DigitalClock::propertyAssigned() { m_imgStale = true; }

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
    m_imgStale = true;
    emit timeChanged();
    update();
  }

  if (timerWanted())
    m_timer.start((60 - t.second()) * 1000 - t.msec() + kSlackMs);
  else
    m_timer.stop();
}

bool DigitalClock::stale() const {
  if (m_dirty || m_bw <= 0.0)
    return true;
  if (std::abs(height() / width() - m_aspect) > 1e-4)
    return true;
  return m_padding > 0.0 && std::abs(width() - m_builtW) > 0.5;
}

void DigitalClock::rebuildVertical(const QFont &f) {
  QPainterPath zero;
  zero.addText(QPointF(0.0, 0.0), f, QStringLiteral("0"));
  const qreal H = zero.boundingRect().height();
  if (H <= 0.0)
    return;

  const qreal ov = m_overlap * H;
  const qreal sp = m_spacing * kRefPx;

  qreal end = 0.0;
  Group hours = buildGroup(m_hours, f, ov, sp, 0.0, end);
  Group mins = buildGroup(m_minutes, f, ov, sp, 0.0, end);
  if (hours.isEmpty() || mins.isEmpty())
    return;

  const QRectF ra = groupBounds(hours);
  const QRectF rb = groupBounds(mins);
  shiftGroup(hours, -ra.center().x(), -ra.top());
  shiftGroup(mins, -rb.center().x(),
             ra.height() * (1.0 - m_overlap) - rb.top());

  const QRectF A = groupBounds(hours);
  const QRectF B = groupBounds(mins);

  Group period;
  if (wantsPeriod()) {
    QFont pf = f;
    pf.setPixelSize(qRound(kRefPx * kPeriodScale));
    qreal pend = 0.0;
    period = buildGroup(m_period, pf, ov * kPeriodScale, sp * kPeriodScale, 0.0,
                        pend);
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

  const qreal pad = m_padding * m_bw / width();
  const qreal w = m_bw - 2.0 * pad;
  const qreal h = m_bh - 2.0 * pad;
  QRectF total = A.united(B);
  if (!period.isEmpty())
    total = total.united(groupBounds(period));
  if (w <= 0.0 || h <= 0.0 || total.isEmpty())
    return;

  const qreal k = std::min(w / total.width(), h / total.height());

  QTransform tf;
  tf.translate(m_bw / 2.0, m_bh / 2.0);
  tf.scale(k, k);
  tf.translate(-total.center().x(), -total.center().y());

  mapGroup(hours, tf);
  mapGroup(mins, tf);
  mapGroup(period, tf);

  const qreal cutW = 2.0 * m_gap * H * k;

  for (int i = 0; i < hours.size(); ++i)
    m_pieces.append({hours[i], i > 0 ? cutW : 0.0});
  for (const QPainterPath &m : std::as_const(mins))
    m_pieces.append({m, cutW});
  for (int i = 0; i < period.size(); ++i)
    m_pieces.append({period[i], i > 0 ? cutW * kPeriodScale : 0.0});
}

void DigitalClock::rebuildHorizontal(const QFont &f) {
  QPainterPath zero;
  zero.addText(QPointF(0.0, 0.0), f, QStringLiteral("0"));
  const qreal H = zero.boundingRect().height();
  if (H <= 0.0)
    return;

  const qreal ov = m_overlap * H;
  const qreal sp = m_spacing * kRefPx;
  const qreal gx = m_groupGap * H;

  qreal end = 0.0;
  Group hours = buildGroup(m_hours, f, ov, sp, 0.0, end);
  if (hours.isEmpty())
    return;

  const QRectF hb = groupBounds(hours);
  const qreal midY = hb.center().y();
  qreal x = end + gx;

  QPainterPath colon;
  if (m_showSep) {
    QFont sf = f;
    sf.setPixelSize(qRound(kRefPx * kSepScale));
    colon = textPath(sf, QStringLiteral(":"));
    const QRectF rs = colon.boundingRect();
    colon.translate(x - rs.left(), midY - rs.center().y());
    x = colon.boundingRect().right() + gx;
  }

  Group mins = buildGroup(m_minutes, f, ov, sp, x, end);
  if (mins.isEmpty())
    return;
  const QRectF mb = groupBounds(mins);

  Group period;
  if (wantsPeriod()) {
    QFont pf = f;
    pf.setPixelSize(qRound(kRefPx * kPeriodScale));
    qreal pend = 0.0;
    period = buildGroup(m_period, pf, ov * kPeriodScale, sp * kPeriodScale, 0.0,
                        pend);
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

  const qreal pad = m_padding * m_bw / width();
  const qreal w = m_bw - 2.0 * pad;
  const qreal h = m_bh - 2.0 * pad;
  if (w <= 0.0 || h <= 0.0 || total.isEmpty())
    return;

  const qreal k = std::min(w / total.width(), h / total.height());

  QTransform tf;
  tf.translate(m_bw / 2.0, m_bh / 2.0);
  tf.scale(k, k);
  tf.translate(-total.center().x(), -total.center().y());

  mapGroup(hours, tf);
  mapGroup(mins, tf);
  mapGroup(period, tf);
  colon = tf.map(colon);

  const qreal cutW = 2.0 * m_gap * H * k;

  for (int i = 0; i < hours.size(); ++i)
    m_pieces.append({hours[i], i > 0 ? cutW : 0.0});
  for (int i = 0; i < mins.size(); ++i)
    m_pieces.append({mins[i], i > 0 ? cutW : 0.0});
  if (!colon.isEmpty())
    m_pieces.append({colon, 0.0});
  for (int i = 0; i < period.size(); ++i)
    m_pieces.append({period[i], i > 0 ? cutW * kPeriodScale : 0.0});
}

void DigitalClock::rebuild() {
  m_dirty = false;
  m_pieces.clear();

  if (m_hours.isEmpty() || width() <= 0.0 || height() <= 0.0)
    return;

  m_bw = kBuildW;
  m_bh = kBuildW * height() / width();
  m_aspect = height() / width();
  m_builtW = width();

  const QFont f = Ryu::styledFont(m_font, kRefPx, m_fontWeight);

  if (m_orientation == Vertical)
    rebuildVertical(f);
  else
    rebuildHorizontal(f);
}

QImage DigitalClock::render(qreal dpr) {
  if (width() <= 0.0 || height() <= 0.0)
    return {};
  if (stale())
    rebuild();
  if (m_pieces.isEmpty())
    return {};

  const int pw = std::max(1, int(std::ceil(width() * dpr)));
  const int ph = std::max(1, int(std::ceil(height() * dpr)));
  QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);

  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (!isEnabled())
    p.setOpacity(Ryu::kDisabledOpacity);

  const qreal s = width() / m_bw;
  p.scale(s, s);

  for (const Piece &pc : std::as_const(m_pieces)) {
    if (pc.cut > kMinCut) {
      p.save();
      p.setCompositionMode(QPainter::CompositionMode_Clear);
      p.setOpacity(1.0);
      p.setBrush(Qt::black);
      p.setPen(
          QPen(Qt::black, pc.cut, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      p.drawPath(pc.path);
      p.restore();
    }
    p.setPen(Qt::NoPen);
    p.setBrush(m_color);
    p.drawPath(pc.path);
  }
  p.end();
  return img;
}

QSGNode *DigitalClock::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
  auto *node = static_cast<QSGSimpleTextureNode *>(old);

  if (width() <= 0.0 || height() <= 0.0 || !window()) {
    delete node;
    return nullptr;
  }

  const qreal dpr = window()->effectiveDevicePixelRatio();

  if (!node) {
    node = new QSGSimpleTextureNode;
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Linear);
    m_imgStale = true;
  }

  if (m_imgStale || dpr != m_imgDpr) {
    const QImage img = render(dpr);
    if (img.isNull()) {
      delete node;
      return nullptr;
    }
    node->setTexture(window()->createTextureFromImage(img));
    m_imgDpr = dpr;
    m_imgStale = false;
  }

  node->setRect(QRectF(0.0, 0.0, width(), height()));
  return node;
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
