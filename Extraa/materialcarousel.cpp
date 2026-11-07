#include "./materialcarousel.hpp"
#include <QFontMetricsF>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGOpacityNode>
#include <QSGTexture>
#include <QSGTextureMaterial>
#include <QSGVertexColorMaterial>
#include <QThreadPool>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace {
constexpr qreal kSlop = 6.0;
constexpr qreal kPad = 16.0;
constexpr qreal kHeadLh = 24.0;
constexpr qreal kSupLh = 20.0;
constexpr qreal kScrimH = 96.0;
constexpr qreal kFadeStart = 72.0;
constexpr qreal kFadeSpan = 40.0;
constexpr qreal kRubber = 0.35;
constexpr qreal kFlingTime = 0.22;
constexpr int kSettleMs = 140;

inline qreal mixv(qreal a, qreal b, qreal t) { return a + (b - a) * t; }

int arcSteps(qreal rad, qreal dpr) {
  const qreal R = std::max(rad * dpr, 0.5);
  const qreal x = std::clamp(1.0 - 0.05 / R, -1.0, 1.0);
  const qreal step = 2.0 * std::acos(x);
  return std::clamp(int(std::ceil((M_PI / 2.0) / std::max(step, 1e-3))), 2, 48);
}

QList<QPointF> roundedPoly(const QRectF &r, qreal rad, qreal dpr) {
  QList<QPointF> o;
  const int n = arcSteps(rad, dpr);
  o.reserve((n + 1) * 4);
  auto corner = [&](const QPointF &c, qreal a0) {
    for (int i = 0; i <= n; ++i) {
      const qreal a = a0 + (M_PI / 2.0) * i / n;
      o.append(c + QPointF(std::cos(a), std::sin(a)) * rad);
    }
  };
  corner(QPointF(r.right() - rad, r.top() + rad), -M_PI / 2.0);
  corner(QPointF(r.right() - rad, r.bottom() - rad), 0.0);
  corner(QPointF(r.left() + rad, r.bottom() - rad), M_PI / 2.0);
  corner(QPointF(r.left() + rad, r.top() + rad), M_PI);
  return o;
}

QImage loadImage(const QString &path, const QSize &box) {
  QImageReader r(path);
  const QSize s = r.size();
  if (s.isValid()) {
    QSize t = s;
    t.scale(box, Qt::KeepAspectRatioByExpanding);
    if (t.width() < s.width() || t.height() < s.height())
      r.setScaledSize(t);
  }
  const QImage in = r.read();
  if (in.isNull()) {
    qWarning("MaterialCarousel: failed to load '%s' (%s)", qPrintable(path),
             qPrintable(r.errorString()));
    return {};
  }
  return in.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

qreal labelAlpha(qreal size) {
  const qreal a = std::clamp((size - kFadeStart) / kFadeSpan, 0.0, 1.0);
  return std::round(a * 8.0) / 8.0;
}
} // namespace

class CarouselImageNode : public QSGOpacityNode {
public:
  CarouselImageNode() {
    m_geo = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0,
                            0, QSGGeometry::UnsignedIntType);
    m_geo->setDrawingMode(QSGGeometry::DrawTriangles);
    m_geo->setVertexDataPattern(QSGGeometry::DynamicPattern);
    m_geo->setIndexDataPattern(QSGGeometry::DynamicPattern);
    m_mat = new QSGTextureMaterial;
    m_mat->setFiltering(QSGTexture::Linear);
    m_mat->setMipmapFiltering(QSGTexture::Linear);
    m_node = new QSGGeometryNode;
    m_node->setGeometry(m_geo);
    m_node->setFlag(QSGNode::OwnsGeometry);
    m_node->setMaterial(m_mat);
    m_node->setFlag(QSGNode::OwnsMaterial);
    appendChildNode(m_node);
  }

  const QString &key() const { return m_key; }

  void setTexture(QSGTexture *t, const QString &key) {
    m_tex.reset(t);
    m_mat->setTexture(t);
    m_key = key;
    m_node->markDirty(QSGNode::DirtyMaterial);
  }

  void clear() {
    if (!m_shown)
      return;
    m_geo->allocate(0, 0);
    m_node->markDirty(QSGNode::DirtyGeometry);
    m_shown = false;
  }

  void setShape(const QRectF &r, qreal rad, qreal dpr, qreal u0, qreal v0,
                qreal u1, qreal v1) {
    const QList<QPointF> o = roundedPoly(r, rad, dpr);
    const int n = o.size();
    m_geo->allocate(n + 1, n * 3);
    QSGGeometry::TexturedPoint2D *v = m_geo->vertexDataAsTexturedPoint2D();
    auto put = [&](QSGGeometry::TexturedPoint2D &vx, const QPointF &p) {
      const qreal fx = r.width() > 0.0 ? (p.x() - r.left()) / r.width() : 0.5;
      const qreal fy = r.height() > 0.0 ? (p.y() - r.top()) / r.height() : 0.5;
      vx.set(float(p.x()), float(p.y()), float(u0 + fx * (u1 - u0)),
             float(v0 + fy * (v1 - v0)));
    };
    put(v[0], r.center());
    for (int i = 0; i < n; ++i)
      put(v[1 + i], o[i]);
    quint32 *ix = m_geo->indexDataAsUInt();
    for (int i = 0; i < n; ++i) {
      ix[i * 3] = 0;
      ix[i * 3 + 1] = quint32(1 + i);
      ix[i * 3 + 2] = quint32(1 + (i + 1) % n);
    }
    m_node->markDirty(QSGNode::DirtyGeometry);
    m_shown = true;
  }

private:
  QSGGeometry *m_geo = nullptr;
  QSGGeometryNode *m_node = nullptr;
  QSGTextureMaterial *m_mat = nullptr;
  std::unique_ptr<QSGTexture> m_tex;
  QString m_key;
  bool m_shown = false;
};

MaterialCarousel::MaterialCarousel(QQuickItem *parent)
    : SgMaterialItem(parent, 260.0, 0.9) {
  setAcceptedMouseButtons(Qt::LeftButton);
  setAcceptHoverEvents(true);
  setActiveFocusOnTab(true);
  setClip(true);
  updateImplicit();
  m_pos.go(0.0, true);

  m_settle = new QTimer(this);
  m_settle->setSingleShot(true);
  m_settle->setInterval(kSettleMs);
  connect(m_settle, &QTimer::timeout, this, [this] { settle(); });
}

void MaterialCarousel::componentComplete() {
  SgMaterialItem::componentComplete();
  layoutItems();
  update();
}

void MaterialCarousel::geometryChange(const QRectF &n, const QRectF &o) {
  QQuickItem::geometryChange(n, o);
  if (n.size() != o.size()) {
    m_layoutDirty = true;
    update();
  }
}

void MaterialCarousel::updateImplicit() {
  if (horizontal()) {
    setImplicitWidth(360.0);
    setImplicitHeight(221.0);
  } else {
    setImplicitWidth(221.0);
    setImplicitHeight(360.0);
  }
}

void MaterialCarousel::structureChanged() {
  m_layoutDirty = true;
  update();
}

QRectF MaterialCarousel::rectFor(const Slot &s) const {
  return horizontal() ? QRectF(s.pos, 0.0, s.size, m_cross)
                      : QRectF(0.0, s.pos, m_cross, s.size);
}

qreal MaterialCarousel::radiusFor(const QRectF &r) const {
  return std::max(0.0,
                  std::min(m_radius, std::min(r.width(), r.height()) / 2.0));
}

void MaterialCarousel::layoutItems() {
  m_layoutDirty = false;
  const int n = m_items.size();
  const qreal avail = std::max(1.0, length());
  const qreal g = m_spacing;
  m_cross = horizontal() ? height() : width();
  m_hasStart = false;
  m_hasEnd = false;
  m_pivot = 0;
  m_largeCount = 1;
  m_start = Profile();
  m_mid = Profile();
  m_end = Profile();

  const qreal lo = std::min(m_minSmall, m_maxSmall);
  const qreal hi = std::max(m_minSmall, m_maxSmall);
  const qreal S = std::clamp(avail * 0.14, lo, hi);

  auto build = [g](const QVector<qreal> &sz, qreal x0) {
    Profile p;
    p.sz = sz;
    p.x.resize(sz.size());
    qreal x = x0;
    for (int k = 0; k < sz.size(); ++k) {
      p.x[k] = x;
      x += sz[k] + g;
    }
    return p;
  };

  auto single = [&](qreal size) {
    m_mid = build(QVector<qreal>{size}, 0.0);
    m_pivot = 0;
    m_large = size;
    m_pitch = size + g;
    m_largeCount = 1;
    m_maxF = std::max<qreal>(0.0, n - 1);
  };

  auto multi = [&]() -> bool {
    const qreal pref = std::max(m_itemSize, S * 2.0);
    const int maxL =
        std::max(1, int(std::ceil(avail / std::max(1.0, pref * 0.5))));
    bool found = false;
    int bl = 1, bm = 0;
    qreal bLs = 0.0, bLm = 0.0, bc = 0.0;
    for (int l = 1; l <= maxL; ++l) {
      for (const int m : {1, 0, 2}) {
        const qreal half = l + m * 0.5;
        const qreal Ls = (avail - (l + m) * g - S * (1.0 + m * 0.5)) / half;
        const qreal Lm = (avail - (l + m + 1) * g - S * (2.0 + m * 0.5)) / half;
        if (Lm < S * 1.5)
          continue;
        const qreal cost =
            std::abs(Ls - pref) / pref + (m == 1 ? 0.0 : (m == 0 ? 0.2 : 0.08));
        if (!found || cost < bc) {
          found = true;
          bc = cost;
          bl = l;
          bm = m;
          bLs = Ls;
          bLm = Lm;
        }
      }
    }
    if (!found)
      return false;
    const qreal Ms = (bLs + S) / 2.0;
    const qreal Mm = (bLm + S) / 2.0;
    QVector<qreal> ss{S}, sm{S};
    for (int i = 0; i < bl; ++i) {
      ss.append(bLs);
      sm.append(bLm);
    }
    for (int i = 0; i < bm; ++i) {
      ss.append(Ms);
      sm.append(Mm);
    }
    ss.append(S);
    sm.append(S);
    m_start = build(ss, -(S + g));
    m_mid = build(sm, 0.0);
    m_hasStart = true;
    m_pivot = 1;
    m_large = bLs;
    m_largeCount = bl;
    m_pitch = bLs + g;
    m_maxF = std::max<qreal>(0.0, n - bl);
    return true;
  };

  auto hero = [&]() -> bool {
    const int sc = avail >= 280.0 ? 2 : 1;
    const qreal Ls = avail - sc * (S + g);
    const qreal Lm = avail - (sc + 1) * (S + g);
    if (Lm < S * 1.5)
      return false;
    QVector<qreal> ss{S, Ls}, sm{S, Lm};
    for (int i = 0; i < sc; ++i) {
      ss.append(S);
      sm.append(S);
    }
    m_start = build(ss, -(S + g));
    m_mid = build(sm, 0.0);
    m_hasStart = true;
    m_pivot = 1;
    m_large = Ls;
    m_largeCount = 1;
    m_pitch = Ls + g;
    m_maxF = std::max<qreal>(0.0, n - 1);
    return true;
  };

  auto centered = [&]() -> bool {
    const qreal L = avail - 2.0 * (S + g);
    if (L < S * 1.5)
      return false;
    m_mid = build(QVector<qreal>{S, L, S}, 0.0);
    m_pivot = 1;
    m_large = L;
    m_largeCount = 1;
    m_pitch = L + g;
    m_maxF = std::max<qreal>(0.0, n - 1);
    return true;
  };

  bool ok = false;
  switch (m_type) {
  case MultiBrowse:
    ok = multi();
    break;
  case Hero:
    ok = hero();
    break;
  case CenteredHero:
    ok = centered();
    break;
  case Uncontained: {
    const qreal I = std::max(8.0, m_itemSize);
    single(I);
    const qreal content = n * (I + g) - g;
    m_maxF = std::max(0.0, (content - avail) / (I + g));
    ok = true;
    break;
  }
  case FullScreen:
    single(avail);
    ok = true;
    break;
  }
  if (!ok)
    single(avail);

  if (m_hasStart && m_maxF > 0.0) {
    const int K = m_start.sz.size() - 1;
    m_end.sz.resize(K + 1);
    m_end.x.resize(K + 1);
    for (int k = 0; k <= K; ++k) {
      m_end.sz[k] = m_start.sz[K - k];
      m_end.x[k] = avail - (m_start.x[K - k] + m_start.sz[K - k]);
    }
    m_hasEnd = true;
  }

  if (!m_dragging) {
    const qreal c = std::clamp(m_pos.x, 0.0, m_maxF);
    m_pos.x = c;
    m_pos.t = std::clamp(m_pos.t, 0.0, m_maxF);
  }
  updateCurrent();
}

void MaterialCarousel::evalProfile(const Profile &p, qreal k, qreal &size,
                                   qreal &pos) const {
  const int K = p.sz.size() - 1;
  const qreal g = m_spacing;
  if (K < 0) {
    size = 0.0;
    pos = 0.0;
    return;
  }
  if (k <= 0.0) {
    size = p.sz[0];
    pos = p.x[0] + k * (p.sz[0] + g);
    return;
  }
  if (k >= K) {
    size = p.sz[K];
    pos = p.x[K] + (k - K) * (p.sz[K] + g);
    return;
  }
  const int j = int(std::floor(k));
  const qreal fr = k - j;
  size = mixv(p.sz[j], p.sz[j + 1], fr);
  pos = mixv(p.x[j], p.x[j + 1], fr);
}

void MaterialCarousel::itemGeometry(int i, qreal f, qreal &size,
                                    qreal &pos) const {
  const int n = m_items.size();
  const qreal kA = i - f + m_pivot;
  evalProfile(m_mid, kA, size, pos);

  if (m_hasStart) {
    qreal sS = 0.0, pS = 0.0;
    evalProfile(m_start, kA, sS, pS);
    const qreal w = std::clamp(f, 0.0, 1.0);
    size = mixv(sS, size, w);
    pos = mixv(pS, pos, w);
  }

  if (m_hasEnd) {
    const qreal e = m_maxF - f;
    const qreal we = std::clamp(e, 0.0, 1.0);
    const int K = m_end.sz.size() - 1;
    const qreal kB = (K - 1) - (n - 1 - i) + e;
    qreal sB = 0.0, pB = 0.0;
    evalProfile(m_end, kB, sB, pB);
    size = mixv(sB, size, we);
    pos = mixv(pB, pos, we);
  }
}

QVector<MaterialCarousel::Slot> MaterialCarousel::visibleSlots() {
  QVector<Slot> out;
  const int n = m_items.size();
  if (n == 0)
    return out;
  if (m_layoutDirty)
    layoutItems();
  const qreal avail = length();
  const qreal f = m_pos.x;
  for (int i = 0; i < n; ++i) {
    qreal size = 0.0, pos = 0.0;
    itemGeometry(i, f, size, pos);
    if (size < 0.5 || pos + size <= 0.0 || pos >= avail)
      continue;
    out.append(Slot{i, pos, size});
  }
  return out;
}

int MaterialCarousel::itemAt(const QPointF &pt) {
  const bool hz = horizontal();
  const qreal a = hz ? pt.x() : pt.y();
  const qreal c = hz ? pt.y() : pt.x();
  if (m_layoutDirty)
    layoutItems();
  if (c < 0.0 || c > m_cross)
    return -1;
  const QVector<Slot> vis = visibleSlots();
  for (const Slot &s : vis)
    if (a >= s.pos && a <= s.pos + s.size)
      return s.index;
  return -1;
}

qreal MaterialCarousel::snapF(qreal x) const {
  qreal best = std::clamp(std::round(x), 0.0, m_maxF);
  if (std::abs(x - m_maxF) < std::abs(x - best))
    best = m_maxF;
  return best;
}

qreal MaterialCarousel::rubber(qreal raw) const {
  if (raw < 0.0)
    return raw * kRubber;
  if (raw > m_maxF)
    return m_maxF + (raw - m_maxF) * kRubber;
  return raw;
}

void MaterialCarousel::updateCurrent() {
  const int n = m_items.size();
  const int i = n == 0 ? 0 : std::clamp(int(std::lround(m_pos.t)), 0, n - 1);
  if (i == m_current)
    return;
  m_current = i;
  emit currentIndexChanged();
}

void MaterialCarousel::goTo(qreal target, bool immediate) {
  if (m_layoutDirty)
    layoutItems();
  m_pos.go(std::clamp(target, 0.0, m_maxF), immediate);
  updateCurrent();
  if (immediate)
    emit scrollPositionChanged();
  else
    animate();
  update();
}

void MaterialCarousel::next() {
  if (m_layoutDirty)
    layoutItems();
  goTo(std::round(m_pos.t) + 1.0);
}

void MaterialCarousel::previous() {
  if (m_layoutDirty)
    layoutItems();
  goTo(std::round(m_pos.t) - 1.0);
}

void MaterialCarousel::scrollTo(int index, bool animate) {
  goTo(index, !animate);
}

void MaterialCarousel::setCurrentIndex(int i) { scrollTo(i, true); }

void MaterialCarousel::settle() {
  if (m_dragging)
    return;
  if (m_snap)
    goTo(snapF(m_pos.x));
  else
    goTo(m_pos.x);
}

void MaterialCarousel::fling() {
  qreal v = std::abs(m_vel) < 0.2 ? 0.0 : m_vel;
  const qreal target = m_pos.x + v * kFlingTime;
  m_pos.t = m_pos.x;
  goTo(m_snap ? snapF(target) : std::clamp(target, 0.0, m_maxF));
}

void MaterialCarousel::activate(int i) {
  if (i < 0 || i >= m_items.size() || !m_items[i].enabled)
    return;
  const QVariant item = m_raw.value(i);
  if (m_type != Uncontained && i != m_current)
    goTo(i);
  QMetaObject::invokeMethod(
      this, [this, i, item] { emit clicked(i, item); }, Qt::QueuedConnection);
}

bool MaterialCarousel::advance(qreal dt) {
  if (m_dragging)
    return true;
  const bool active = step(m_pos, dt, 0.0005);
  emit scrollPositionChanged();
  return active;
}

void MaterialCarousel::setModel(const QVariantList &v) {
  m_raw = v;
  m_items.clear();
  m_items.reserve(v.size());
  ++m_gen;
  m_resetNodes = true;

  for (const QVariant &it : v) {
    Entry e;
    if (it.metaType().id() == QMetaType::QString) {
      e.headline = it.toString();
    } else {
      const QVariantMap m = it.toMap();
      e.headline = m.value("headline").toString();
      e.supporting = m.value("supporting").toString();
      e.enabled = m.contains("enabled") ? m.value("enabled").toBool() : true;
      const QVariant cv = m.value("color");
      if (cv.metaType().id() == QMetaType::QString)
        e.color = QColor(cv.toString());
      else if (cv.canConvert<QColor>())
        e.color = cv.value<QColor>();
      const QString img = m.value("image").toString();
      if (!img.isEmpty())
        e.path = Ryu::resolveUrl(qmlContext(this), QUrl(img));
    }
    m_items.append(e);
  }

  m_pressIndex = -1;
  m_hoverIndex = -1;
  m_layoutDirty = true;
  m_pos.go(0.0, true);
  m_current = 0;
  update();
  emit modelChanged();
  emit currentIndexChanged();
  emit scrollPositionChanged();
}

void MaterialCarousel::setType(Type v) {
  if (assign(m_type, v, &MaterialCarousel::typeChanged))
    structureChanged();
}

void MaterialCarousel::setOrientation(Orientation v) {
  if (assign(m_orientation, v, &MaterialCarousel::orientationChanged)) {
    updateImplicit();
    structureChanged();
  }
}

void MaterialCarousel::setSnap(bool v) {
  assign(m_snap, v, &MaterialCarousel::snapChanged);
}

void MaterialCarousel::setItemSize(qreal v) {
  if (assign(m_itemSize, std::max(8.0, v), &MaterialCarousel::itemSizeChanged))
    structureChanged();
}

void MaterialCarousel::setMinSmallItemSize(qreal v) {
  if (assign(m_minSmall, std::max(0.0, v),
             &MaterialCarousel::minSmallItemSizeChanged))
    structureChanged();
}

void MaterialCarousel::setMaxSmallItemSize(qreal v) {
  if (assign(m_maxSmall, std::max(0.0, v),
             &MaterialCarousel::maxSmallItemSizeChanged))
    structureChanged();
}

void MaterialCarousel::setItemSpacing(qreal v) {
  if (assign(m_spacing, std::max(0.0, v),
             &MaterialCarousel::itemSpacingChanged))
    structureChanged();
}

void MaterialCarousel::setItemRadius(qreal v) {
  assign(m_radius, std::max(0.0, v), &MaterialCarousel::itemRadiusChanged);
}

void MaterialCarousel::setFont(const QFont &v) {
  assign(m_font, v, &MaterialCarousel::fontChanged);
}

void MaterialCarousel::setFontWeight(int v) {
  v = qBound(1, v, 1000);
  assign(m_fontWeight, v, &MaterialCarousel::fontWeightChanged);
}

void MaterialCarousel::setItemColor(const QColor &v) {
  assign(m_itemColor, v, &MaterialCarousel::itemColorChanged);
}

void MaterialCarousel::setTextColor(const QColor &v) {
  assign(m_textColor, v, &MaterialCarousel::textColorChanged);
}

void MaterialCarousel::setImageTextColor(const QColor &v) {
  assign(m_imageTextColor, v, &MaterialCarousel::imageTextColorChanged);
}

void MaterialCarousel::setScrimColor(const QColor &v) {
  assign(m_scrimColor, v, &MaterialCarousel::scrimColorChanged);
}

void MaterialCarousel::setStateLayerColor(const QColor &v) {
  assign(m_stateLayerColor, v, &MaterialCarousel::stateLayerColorChanged);
}

void MaterialCarousel::setBorderColor(const QColor &v) {
  assign(m_borderColor, v, &MaterialCarousel::borderColorChanged);
}

void MaterialCarousel::setBorderWidth(qreal v) {
  assign(m_borderWidth, std::max(0.0, v),
         &MaterialCarousel::borderWidthChanged);
}

void MaterialCarousel::mousePressEvent(QMouseEvent *e) {
  if (!isEnabled() || m_items.isEmpty()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  m_keyFocus = false;
  forceActiveFocus(Qt::MouseFocusReason);
  m_settle->stop();
  m_pressed = true;
  m_moved = false;
  m_pressPt = e->position();
  m_pressIndex = itemAt(m_pressPt);
  m_pos.t = m_pos.x;
  m_pos.v = 0.0;
  m_dragStartF = m_pos.x;
  m_vel = 0.0;
  m_lastAlong = horizontal() ? m_pressPt.x() : m_pressPt.y();
  m_clock.restart();
  m_lastMs = 0;
  update();
  e->accept();
}

void MaterialCarousel::mouseMoveEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  const bool hz = horizontal();
  const qreal along = hz ? e->position().x() : e->position().y();
  const qreal start = hz ? m_pressPt.x() : m_pressPt.y();
  const qreal dx = along - start;

  if (!m_moved && std::abs(dx) > kSlop) {
    m_moved = true;
    m_dragging = true;
    setKeepMouseGrab(true);
    emit draggingChanged();
  }

  if (m_dragging) {
    const qreal raw = m_dragStartF - dx / m_pitch;
    const qreal x = rubber(raw);
    m_pos.x = x;
    m_pos.t = x;
    m_pos.v = 0.0;

    const qint64 now = m_clock.elapsed();
    const qreal dt = (now - m_lastMs) / 1000.0;
    if (dt > 0.001) {
      const qreal inst = -(along - m_lastAlong) / m_pitch / dt;
      m_vel = 0.7 * m_vel + 0.3 * inst;
      m_lastMs = now;
      m_lastAlong = along;
    }
    emit scrollPositionChanged();
    update();
  }
  e->accept();
}

void MaterialCarousel::endDrag() {
  const bool was = m_dragging;
  m_dragging = false;
  setKeepMouseGrab(false);
  if (was)
    emit draggingChanged();
}

void MaterialCarousel::mouseReleaseEvent(QMouseEvent *e) {
  if (!m_pressed)
    return;
  m_pressed = false;
  const int idx = m_pressIndex;
  m_pressIndex = -1;
  const bool wasDragging = m_dragging;
  endDrag();

  if (wasDragging) {
    fling();
  } else {
    if (m_snap)
      goTo(snapF(m_pos.x));
    if (idx >= 0 && itemAt(e->position()) == idx)
      activate(idx);
  }
  update();
  e->accept();
}

void MaterialCarousel::mouseUngrabEvent() {
  if (!m_pressed && !m_dragging)
    return;
  m_pressed = false;
  m_pressIndex = -1;
  const bool wasDragging = m_dragging;
  endDrag();
  if (wasDragging)
    fling();
  update();
}

void MaterialCarousel::hoverEnterEvent(QHoverEvent *e) {
  QQuickItem::hoverEnterEvent(e);
  hoverMoveEvent(e);
}

void MaterialCarousel::hoverMoveEvent(QHoverEvent *e) {
  QQuickItem::hoverMoveEvent(e);
  if (!isEnabled() || m_items.isEmpty())
    return;
  const int i = itemAt(e->position());
  const int h = (i >= 0 && m_items[i].enabled) ? i : -1;
  setHand(h >= 0);
  if (h != m_hoverIndex) {
    m_hoverIndex = h;
    update();
  }
}

void MaterialCarousel::hoverLeaveEvent(QHoverEvent *e) {
  QQuickItem::hoverLeaveEvent(e);
  m_hoverIndex = -1;
  setHand(false);
  update();
}

void MaterialCarousel::wheelEvent(QWheelEvent *e) {
  if (!isEnabled() || m_items.isEmpty()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();

  const QPoint px = e->pixelDelta();
  const QPoint ang = e->angleDelta();
  const bool hz = horizontal();

  if (!px.isNull()) {
    const qreal d = hz ? (px.x() != 0 ? px.x() : px.y()) : px.y();
    const qreal x = std::clamp(m_pos.x - d / m_pitch, 0.0, m_maxF);
    m_pos.x = x;
    m_pos.t = x;
    m_pos.v = 0.0;
    updateCurrent();
    m_settle->start();
    emit scrollPositionChanged();
    update();
  } else {
    const int a = hz ? (ang.x() != 0 ? ang.x() : ang.y()) : ang.y();
    if (a != 0) {
      const qreal dir = a > 0 ? -1.0 : 1.0;
      goTo(std::round(m_pos.t) + dir);
    }
  }
  e->accept();
}

void MaterialCarousel::keyPressEvent(QKeyEvent *e) {
  if (!isEnabled() || m_items.isEmpty()) {
    e->ignore();
    return;
  }
  if (m_layoutDirty)
    layoutItems();
  const int prevKey = horizontal() ? Qt::Key_Left : Qt::Key_Up;
  const int nextKey = horizontal() ? Qt::Key_Right : Qt::Key_Down;
  const int k = e->key();

  if (k == prevKey) {
    previous();
  } else if (k == nextKey) {
    next();
  } else if (k == Qt::Key_Home) {
    goTo(0.0);
  } else if (k == Qt::Key_End) {
    goTo(m_maxF);
  } else if (k == Qt::Key_Space || k == Qt::Key_Return || k == Qt::Key_Enter) {
    if (!e->isAutoRepeat())
      activate(m_current);
  } else {
    e->ignore();
    return;
  }
  m_keyFocus = true;
  update();
  e->accept();
}

void MaterialCarousel::focusOutEvent(QFocusEvent *e) {
  QQuickItem::focusOutEvent(e);
  m_keyFocus = false;
  update();
}

void MaterialCarousel::requestImages() {
  const qreal dp = dpr();
  auto b = [](qreal v) { return int(std::ceil(v / 50.0)) * 50; };
  const qreal along = std::max(m_large, 1.0);
  const qreal cross = std::max(m_cross, 1.0);
  const QSize box = horizontal() ? QSize(b(along * dp), b(cross * dp))
                                 : QSize(b(cross * dp), b(along * dp));
  const int n = m_items.size();
  const int span = m_mid.sz.size() + m_largeCount + 3;
  const int lo = std::max(0, m_current - span);
  const int hi = std::min(n - 1, m_current + span);

  for (int i = 0; i < n; ++i) {
    Entry &e = m_items[i];
    if (e.path.isEmpty())
      continue;

    if (i < lo || i > hi) {
      if (!e.loading && !e.image.isNull()) {
        e.image = QImage();
        e.want = QSize();
      }
      continue;
    }

    if (e.loading || e.want == box)
      continue;

    e.loading = true;
    e.want = box;
    QPointer<MaterialCarousel> self(this);
    const int gen = m_gen;
    const QString path = e.path;
    QThreadPool::globalInstance()->start([self, path, box, gen, i] {
      QImage img = loadImage(path, box);
      if (!self)
        return;
      QMetaObject::invokeMethod(
          self.data(),
          [self, img, gen, i] {
            if (!self || self->m_gen != gen || i >= self->m_items.size())
              return;
            Entry &e = self->m_items[i];
            e.loading = false;
            e.image = img;
            ++e.rev;
            self->update();
          },
          Qt::QueuedConnection);
    });
  }
}
void MaterialCarousel::syncImages() {
  if (!m_imgGroup || !window())
    return;
  const int n = m_items.size();

  if (m_resetNodes) {
    for (CarouselImageNode *nd : std::as_const(m_imgNodes)) {
      if (!nd)
        continue;
      m_imgGroup->removeChildNode(nd);
      delete nd;
    }
    m_imgNodes = QVector<CarouselImageNode *>(n, nullptr);
    m_resetNodes = false;
  }
  if (m_imgNodes.size() != n)
    m_imgNodes.resize(n);

  m_imgGroup->setOpacity(fade());

  const qreal dp = dpr();
  const int texSide =
      std::clamp(int(std::max(m_large, m_cross) * dp * 1.5), 256, 2048);
  const bool hz = horizontal();
  QVector<char> shown(n, 0);

  for (const Slot &s : std::as_const(m_vis)) {
    const Entry &e = m_items[s.index];
    if (e.image.isNull())
      continue;
    const QString key = QString::number(m_gen) + QLatin1Char('|') +
                        QString::number(texSide) + QLatin1Char('|') +
                        QString::number(e.rev);

    CarouselImageNode *node = m_imgNodes[s.index];
    if (!node || node->key() != key) {
      QImage img = e.image;
      if (std::max(img.width(), img.height()) > texSide)
        img = img.scaled(texSide, texSide, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
      QSGTexture *t = window()->createTextureFromImage(
          img, QQuickWindow::TextureHasMipmaps);
      if (!t)
        continue;
      if (!node) {
        node = new CarouselImageNode;
        m_imgGroup->appendChildNode(node);
        m_imgNodes[s.index] = node;
      }
      node->setTexture(t, key);
    }

    const QRectF r = rectFor(s);
    const qreal rad = radiusFor(r);
    const qreal iw = e.image.width();
    const qreal ih = e.image.height();
    const qreal refW = hz ? std::max(r.width(), m_large) : r.width();
    const qreal refH = hz ? r.height() : std::max(r.height(), m_large);
    const qreal sc = std::max(refW / iw, refH / ih);
    const qreal hu = r.width() / (2.0 * sc * iw);
    const qreal hv = r.height() / (2.0 * sc * ih);

    node->setOpacity(e.enabled ? 1.0 : Ryu::kDisabledOpacity);
    node->setShape(r, rad, dp, 0.5 - hu, 0.5 - hv, 0.5 + hu, 0.5 + hv);
    shown[s.index] = 1;
  }

  const int keep = m_mid.sz.size() + 3;
  for (int i = 0; i < n; ++i) {
    CarouselImageNode *nd = m_imgNodes[i];
    if (!nd || shown[i])
      continue;
    if (std::abs(i - m_current) > keep) {
      m_imgGroup->removeChildNode(nd);
      delete nd;
      m_imgNodes[i] = nullptr;
    } else {
      nd->clear();
    }
  }
}

void MaterialCarousel::syncScrim() {
  if (!m_scrim)
    return;
  std::vector<QSGGeometry::ColoredPoint2D> v;
  std::vector<quint32> ix;
  const qreal dp = dpr();
  const qreal fd = fade();

  for (const Slot &s : std::as_const(m_vis)) {
    const Entry &e = m_items[s.index];
    if (e.path.isEmpty() || (e.headline.isEmpty() && e.supporting.isEmpty()))
      continue;
    const qreal la = labelAlpha(s.size);
    if (la <= 0.0)
      continue;

    const QRectF r = rectFor(s);
    const qreal rad = radiusFor(r);
    const qreal dim = e.enabled ? 1.0 : Ryu::kDisabledOpacity;
    const qreal y1 = r.bottom();
    const qreal y0 = y1 - std::min(r.height(), kScrimH);
    const qreal yc = y1 - rad;

    std::vector<qreal> ys;
    for (qreal y = y0; y < y1; y += 4.0)
      ys.push_back(y);
    ys.push_back(y1);
    const int nc = arcSteps(rad, dp);
    for (int j = 1; j < nc; ++j) {
      const qreal y = yc + rad * std::sin((M_PI / 2.0) * j / nc);
      if (y > y0 && y < y1)
        ys.push_back(y);
    }
    std::sort(ys.begin(), ys.end());
    ys.erase(
        std::unique(ys.begin(), ys.end(),
                    [](qreal a, qreal b) { return std::abs(a - b) < 1e-3; }),
        ys.end());

    const quint32 base0 = quint32(v.size());
    for (size_t j = 0; j < ys.size(); ++j) {
      const qreal y = ys[j];
      const qreal dy = y - yc;
      const qreal dx = (dy > 0.0 && rad > 0.0)
                           ? rad - std::sqrt(std::max(0.0, rad * rad - dy * dy))
                           : 0.0;
      const qreal t = std::clamp((y - y0) / std::max(1e-6, y1 - y0), 0.0, 1.0);
      const qreal ease = t * t * (3.0 - 2.0 * t);
      const qreal a =
          std::clamp(m_scrimColor.alphaF() * ease * la * dim * fd, 0.0, 1.0);
      auto q = [](qreal c) {
        return quint8(std::clamp(int(std::lround(c * 255.0)), 0, 255));
      };
      const quint8 cr = q(m_scrimColor.redF() * a);
      const quint8 cg = q(m_scrimColor.greenF() * a);
      const quint8 cb = q(m_scrimColor.blueF() * a);
      const quint8 ca = q(a);

      QSGGeometry::ColoredPoint2D p0, p1;
      p0.set(float(r.left() + dx), float(y), cr, cg, cb, ca);
      p1.set(float(r.right() - dx), float(y), cr, cg, cb, ca);
      v.push_back(p0);
      v.push_back(p1);

      if (j > 0) {
        const quint32 b = base0 + quint32((j - 1) * 2);
        ix.push_back(b);
        ix.push_back(b + 1);
        ix.push_back(b + 3);
        ix.push_back(b);
        ix.push_back(b + 3);
        ix.push_back(b + 2);
      }
    }
  }

  QSGGeometry *g = m_scrim->geometry();
  g->allocate(int(v.size()), int(ix.size()));
  if (!v.empty())
    std::memcpy(g->vertexData(), v.data(),
                v.size() * sizeof(QSGGeometry::ColoredPoint2D));
  if (!ix.empty())
    std::memcpy(g->indexData(), ix.data(), ix.size() * sizeof(quint32));
  m_scrim->markDirty(QSGNode::DirtyGeometry);
}

QSGNode *MaterialCarousel::updatePaintNode(QSGNode *old,
                                           UpdatePaintNodeData *d) {
  QSGNode *top = old;
  const bool fresh = !top;

  if (fresh) {
    m_imgNodes = QVector<CarouselImageNode *>();
    m_resetNodes = false;
    top = new QSGNode;

    m_imgGroup = new QSGOpacityNode;
    top->appendChildNode(m_imgGroup);

    m_scrim = new QSGGeometryNode;
    auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),
                              0, 0, QSGGeometry::UnsignedIntType);
    g->setDrawingMode(QSGGeometry::DrawTriangles);
    g->setVertexDataPattern(QSGGeometry::DynamicPattern);
    g->setIndexDataPattern(QSGGeometry::DynamicPattern);
    m_scrim->setGeometry(g);
    m_scrim->setFlag(QSGNode::OwnsGeometry);
    m_scrim->setMaterial(new QSGVertexColorMaterial);
    m_scrim->setFlag(QSGNode::OwnsMaterial);
    top->appendChildNode(m_scrim);
  }

  if (m_layoutDirty)
    layoutItems();
  m_vis = visibleSlots();
  requestImages();
  syncImages();
  syncScrim();

  if (fresh) {
    QSGNode *sg = SgMaterialItem::updatePaintNode(nullptr, d);
    top->appendChildNode(sg);
  } else {
    SgMaterialItem::updatePaintNode(top->childAtIndex(2), d);
  }
  return top;
}

void MaterialCarousel::build(Ryu::Mesh &m) {
  const int n = m_items.size();
  if (n == 0 || width() <= 0.0 || height() <= 0.0)
    return;

  const QFont headF = Ryu::styledFont(m_font, 16.0, m_fontWeight);
  const QFont supF = Ryu::styledFont(m_font, 14.0, QFont::Normal);
  const QFontMetricsF headM(headF);
  const QFontMetricsF supM(supF);

  for (const Slot &s : std::as_const(m_vis)) {
    const Entry &e = m_items[s.index];
    const QRectF r = rectFor(s);
    const qreal rad = radiusFor(r);
    const qreal dim = e.enabled ? 1.0 : Ryu::kDisabledOpacity;
    auto d = [dim](const QColor &c) {
      return Ryu::withAlpha(c, c.alphaF() * dim);
    };
    const bool hasImg = !e.path.isEmpty();

    if (!hasImg) {
      const QColor bg = e.color.isValid() ? e.color : m_itemColor;
      if (bg.alpha() > 0)
        m.fillRoundedRect(r, rad, rad, rad, rad, d(bg));
    }

    if (m_borderWidth > 0.0 && m_borderColor.alpha() > 0)
      m.strokeRoundedRect(r, rad, rad, rad, rad, m.snapStroke(m_borderWidth),
                          d(m_borderColor));

    const bool pressed = m_pressIndex == s.index && !m_dragging;
    const bool focused = m_keyFocus && hasActiveFocus() && m_current == s.index;
    const qreal layer =
        pressed || focused
            ? 0.50
            : (m_hoverIndex == s.index && !m_dragging ? 0.30 : 0.0);
    if (layer > 0.0)
      m.fillRoundedRect(r, rad, rad, rad, rad,
                        d(Ryu::withAlpha(m_stateLayerColor, layer)));

    const bool hasH = !e.headline.isEmpty();
    const bool hasS = !e.supporting.isEmpty();
    const qreal la = labelAlpha(s.size);
    if (la <= 0.0 || (!hasH && !hasS))
      continue;

    const QColor base = hasImg ? m_imageTextColor : m_textColor;
    const QColor fg = Ryu::withAlpha(base, base.alphaF() * la * dim);
    const qreal availW =
        std::floor(std::max(0.0, r.width() - 2.0 * kPad) / 4.0) * 4.0;
    const qreal x = r.left() + kPad;
    qreal ty =
        r.bottom() - kPad - (hasH ? kHeadLh : 0.0) - (hasS ? kSupLh : 0.0);

    if (hasH) {
      const QString t = headM.elidedText(e.headline, Qt::ElideRight, availW);
      textLayer(s.index * 2, headF, t, fg, x, ty + kHeadLh / 2.0);
      ty += kHeadLh;
    }
    if (hasS) {
      const QString t = supM.elidedText(e.supporting, Qt::ElideRight, availW);
      textLayer(s.index * 2 + 1, supF, t, fg, x, ty + kSupLh / 2.0);
    }
  }
}
