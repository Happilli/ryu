#include "./sgitem.hpp"
#include "./paintutil.hpp"
#include <QCursor>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QSGVertexColorMaterial>
#include <QStringList>
#include <cstring>

namespace {

inline qreal dot(const QPointF &a, const QPointF &b) {
  return a.x() * b.x() + a.y() * b.y();
}

inline QPointF unit(const QPointF &d) {
  const qreal l = std::hypot(d.x(), d.y());
  return l > 1e-9 ? d / l : QPointF();
}

} // namespace

namespace Ryu {

void Mesh::begin(qreal dpr, qreal opacity) {
  m_v.clear();
  m_i.clear();
  m_origin = QPointF();
  m_dpr = std::max(0.25, dpr);
  m_opacity = opacity;
}

Mesh::Rgba Mesh::pack(const QColor &c) const {
  const qreal a = c.alphaF() * m_opacity;
  auto q = [](qreal v) {
    return quint8(std::clamp(int(std::lround(v * 255.0)), 0, 255));
  };
  return {q(c.redF() * a), q(c.greenF() * a), q(c.blueF() * a), q(a)};
}

int Mesh::push(const QPointF &p, Rgba c) {
  QSGGeometry::ColoredPoint2D v;
  v.set(float(p.x() + m_origin.x()), float(p.y() + m_origin.y()), c.r, c.g, c.b,
        c.a);
  m_v.push_back(v);
  return int(m_v.size()) - 1;
}

void Mesh::tri(int a, int b, int c) {
  m_i.push_back(quint32(a));
  m_i.push_back(quint32(b));
  m_i.push_back(quint32(c));
}

void Mesh::quad(int a, int b, int c, int d) {
  tri(a, b, c);
  tri(a, c, d);
}

int Mesh::steps(qreal r, qreal span) const {
  const qreal R = std::max(r * m_dpr, 0.5);
  const qreal x = std::clamp(1.0 - 0.05 / R, -1.0, 1.0);
  const qreal step = 2.0 * std::acos(x);
  return std::clamp(int(std::ceil(span / std::max(step, 1e-3))), 2, 96);
}

Mesh::Outline Mesh::roundedOutline(const QRectF &r, qreal tl, qreal tr,
                                   qreal br, qreal bl) const {
  Outline o;
  if (r.width() <= 0.0 || r.height() <= 0.0)
    return o;

  const qreal maxR = std::min(r.width(), r.height()) / 2.0;
  tl = std::clamp(tl, 0.0, maxR);
  tr = std::clamp(tr, 0.0, maxR);
  br = std::clamp(br, 0.0, maxR);
  bl = std::clamp(bl, 0.0, maxR);

  auto add = [&](const QPointF &p) {
    if (!o.isEmpty() && (o.last() - p).manhattanLength() < 1e-6)
      return;
    o.append(p);
  };

  auto corner = [&](const QPointF &c, qreal rad, qreal a0) {
    if (rad < 1e-6) {
      add(c);
      return;
    }
    const int n = steps(rad, M_PI / 2.0);
    for (int i = 0; i <= n; ++i) {
      const qreal a = a0 + (M_PI / 2.0) * i / n;
      add(c + QPointF(std::cos(a), std::sin(a)) * rad);
    }
  };

  corner(QPointF(r.right() - tr, r.top() + tr), tr, -M_PI / 2.0);
  corner(QPointF(r.right() - br, r.bottom() - br), br, 0.0);
  corner(QPointF(r.left() + bl, r.bottom() - bl), bl, M_PI / 2.0);
  corner(QPointF(r.left() + tl, r.top() + tl), tl, M_PI);

  if (o.size() > 1 && (o.first() - o.last()).manhattanLength() < 1e-6)
    o.removeLast();
  return o;
}

Mesh::Outline Mesh::circleOutline(const QPointF &c, qreal r) const {
  Outline o;
  if (r <= 0.0)
    return o;
  const int n = std::max(steps(r, 2.0 * M_PI), 12);
  o.reserve(n);
  for (int i = 0; i < n; ++i) {
    const qreal a = 2.0 * M_PI * i / n;
    o.append(c + QPointF(std::cos(a), std::sin(a)) * r);
  }
  return o;
}

QList<QPointF> Mesh::miters(const Outline &o) const {
  const int n = o.size();
  qreal area = 0.0;
  for (int i = 0; i < n; ++i) {
    const QPointF &a = o[i];
    const QPointF &b = o[(i + 1) % n];
    area += a.x() * b.y() - b.x() * a.y();
  }
  const qreal s = area > 0.0 ? 1.0 : -1.0;

  QList<QPointF> en(n);
  for (int i = 0; i < n; ++i) {
    const QPointF d = o[(i + 1) % n] - o[i];
    const QPointF u = unit(d);
    en[i] = QPointF(u.y(), -u.x()) * s;
  }

  QList<QPointF> m(n);
  for (int i = 0; i < n; ++i) {
    const QPointF n1 = en[(i + n - 1) % n];
    const QPointF n2 = en[i];
    QPointF mv = n1 + n2;
    const qreal l = std::hypot(mv.x(), mv.y());
    if (l < 1e-6) {
      m[i] = n1;
      continue;
    }
    mv /= l;
    m[i] = mv / std::max(dot(mv, n1), 0.25);
  }
  return m;
}

int Mesh::ring(const Outline &o, const QList<QPointF> &mv, qreal d, Rgba c) {
  const int base = int(m_v.size());
  for (int i = 0; i < o.size(); ++i)
    push(o[i] + mv[i] * d, c);
  return base;
}

void Mesh::fillOutline(const Outline &o, const QColor &col) {
  const int n = o.size();
  if (n < 3 || col.alphaF() <= 0.0)
    return;

  const QList<QPointF> mv = miters(o);
  const qreal f = 1.0 / m_dpr;

  qreal x0 = o[0].x(), x1 = x0, y0 = o[0].y(), y1 = y0;
  QPointF cen;
  for (const QPointF &p : o) {
    x0 = std::min(x0, p.x());
    x1 = std::max(x1, p.x());
    y0 = std::min(y0, p.y());
    y1 = std::max(y1, p.y());
    cen += p;
  }
  cen /= n;

  const qreal half = std::min(x1 - x0, y1 - y0) / 2.0;
  const qreal inset = std::min(f / 2.0, half * 0.5);

  const Rgba solid = pack(col);
  const Rgba clear{0, 0, 0, 0};

  const int c = push(cen, solid);
  const int in = ring(o, mv, -inset, solid);
  const int out = ring(o, mv, f / 2.0, clear);

  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    tri(c, in + i, in + j);
    quad(in + i, in + j, out + j, out + i);
  }
}

void Mesh::strokeOutline(const Outline &o, qreal w, const QColor &col) {
  const int n = o.size();
  if (n < 3 || col.alphaF() <= 0.0)
    return;

  const qreal f = 1.0 / m_dpr;
  w = std::max(w, f);

  const QList<QPointF> mv = miters(o);
  const Rgba solid = pack(col);
  const Rgba clear{0, 0, 0, 0};

  const int r0 = ring(o, mv, f / 2.0, clear);
  const int r1 = ring(o, mv, -f / 2.0, solid);
  const int r2 = ring(o, mv, -(w - f / 2.0), solid);
  const int r3 = ring(o, mv, -(w + f / 2.0), clear);

  for (int i = 0; i < n; ++i) {
    const int j = (i + 1) % n;
    quad(r0 + i, r0 + j, r1 + j, r1 + i);
    quad(r1 + i, r1 + j, r2 + j, r2 + i);
    quad(r2 + i, r2 + j, r3 + j, r3 + i);
  }
}

void Mesh::fillRoundedRect(const QRectF &r, qreal tl, qreal tr, qreal br,
                           qreal bl, const QColor &c) {
  fillOutline(roundedOutline(r, tl, tr, br, bl), c);
}

void Mesh::strokeRoundedRect(const QRectF &r, qreal tl, qreal tr, qreal br,
                             qreal bl, qreal w, const QColor &c) {
  strokeOutline(roundedOutline(r, tl, tr, br, bl), w, c);
}

void Mesh::fillCircle(const QPointF &c, qreal r, const QColor &col) {
  fillOutline(circleOutline(c, r), col);
}

void Mesh::strokeCircle(const QPointF &c, qreal r, qreal w, const QColor &col) {
  strokeOutline(circleOutline(c, r), w, col);
}

void Mesh::line(const QPointF &a, const QPointF &b, qreal w, const QColor &c) {
  polyline({a, b}, w, c);
}

void Mesh::polyline(const QList<QPointF> &in, qreal w, const QColor &col) {
  QList<QPointF> pts;
  pts.reserve(in.size());
  for (const QPointF &p : in)
    if (pts.isEmpty() || (pts.last() - p).manhattanLength() > 1e-6)
      pts.append(p);

  const int n = pts.size();
  if (n < 2 || col.alphaF() <= 0.0)
    return;

  const qreal f = 1.0 / m_dpr;
  w = std::max(w, f);
  const qreal h = w / 2.0;

  const Rgba solid = pack(col);
  const Rgba clear{0, 0, 0, 0};

  QList<QPointF> nr(n - 1);
  for (int i = 0; i < n - 1; ++i) {
    const QPointF u = unit(pts[i + 1] - pts[i]);
    nr[i] = QPointF(-u.y(), u.x());
  }

  const qreal off[4] = {h + f / 2.0, h - f / 2.0, -(h - f / 2.0),
                        -(h + f / 2.0)};
  const Rgba cols[4] = {clear, solid, solid, clear};

  QList<int> base(n);
  for (int i = 0; i < n; ++i) {
    QPointF mv;
    if (i == 0) {
      mv = nr[0];
    } else if (i == n - 1) {
      mv = nr[n - 2];
    } else {
      QPointF s = nr[i - 1] + nr[i];
      const qreal l = std::hypot(s.x(), s.y());
      if (l < 1e-6) {
        mv = nr[i - 1];
      } else {
        s /= l;
        mv = s / std::max(dot(s, nr[i - 1]), 0.25);
      }
    }
    base[i] = int(m_v.size());
    for (int k = 0; k < 4; ++k)
      push(pts[i] + mv * off[k], cols[k]);
  }

  for (int i = 0; i < n - 1; ++i)
    for (int k = 0; k < 3; ++k)
      quad(base[i] + k, base[i] + k + 1, base[i + 1] + k + 1, base[i + 1] + k);

  auto cap = [&](const QPointF &p, const QPointF &t, const QPointF &nn) {
    const int c = push(p, solid);
    const int K = steps(h, M_PI);
    int prevIn = -1, prevOut = -1;
    for (int j = 0; j <= K; ++j) {
      const qreal th = M_PI * j / K;
      const QPointF dir = nn * std::cos(th) + t * std::sin(th);
      const int pin = push(p + dir * (h - f / 2.0), solid);
      const int pout = push(p + dir * (h + f / 2.0), clear);
      if (j > 0) {
        tri(c, prevIn, pin);
        quad(prevIn, pin, pout, prevOut);
      }
      prevIn = pin;
      prevOut = pout;
    }
  };

  cap(pts[0], unit(pts[0] - pts[1]), nr[0]);
  cap(pts[n - 1], unit(pts[n - 1] - pts[n - 2]), nr[n - 2]);
}

void Mesh::upload(QSGGeometry *g) const {
  g->allocate(int(m_v.size()), int(m_i.size()));
  if (!m_v.empty())
    std::memcpy(g->vertexData(), m_v.data(),
                m_v.size() * sizeof(QSGGeometry::ColoredPoint2D));
  if (!m_i.empty())
    std::memcpy(g->indexData(), m_i.data(), m_i.size() * sizeof(quint32));
}

} // namespace Ryu

SgItem::SgItem(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

SgItem::~SgItem() { setHand(false); }

qreal SgItem::dpr() const {
  return window() ? window()->effectiveDevicePixelRatio() : 1.0;
}

void SgItem::setHand(bool on) {
  if (on == m_handOn)
    return;
  m_handOn = on;
  if (on)
    QGuiApplication::setOverrideCursor(QCursor(Qt::PointingHandCursor));
  else
    QGuiApplication::restoreOverrideCursor();
}

void SgItem::itemChange(ItemChange change, const ItemChangeData &data) {
  QQuickItem::itemChange(change, data);
  if (change == ItemVisibleHasChanged && !data.boolValue)
    setHand(false);
  if (change == ItemEnabledHasChanged) {
    if (!isEnabled())
      setHand(false);
    update();
  }
  if (change == ItemSceneChange) {
    if (m_screenConn) {
      disconnect(m_screenConn);
      m_screenConn = {};
    }
    if (data.window)
      m_screenConn = connect(data.window, &QWindow::screenChanged, this,
                             [this] { update(); });
    update();
  }
}

QSGNode *SgItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
  auto *node = static_cast<QSGGeometryNode *>(old);
  if (!node) {
    node = new QSGGeometryNode;
    auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),
                              0, 0, QSGGeometry::UnsignedIntType);
    g->setDrawingMode(QSGGeometry::DrawTriangles);
    node->setGeometry(g);
    node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(new QSGVertexColorMaterial);
    node->setFlag(QSGNode::OwnsMaterial);
    for (Ryu::Layer &l : m_layers) {
      l.node = nullptr;
      l.shown.clear();
    }
  }

  for (Ryu::Layer &l : m_layers)
    l.on = false;

  m_mesh.begin(dpr(), isEnabled() ? 1.0 : Ryu::kDisabledOpacity);
  build(m_mesh);
  m_mesh.upload(node->geometry());
  node->markDirty(QSGNode::DirtyGeometry);
  syncLayers(node);
  return node;
}

qreal SgItem::fade() const { return isEnabled() ? 1.0 : Ryu::kDisabledOpacity; }

void SgItem::useLayer(int slot, const QString &key, const QRectF &rect,
                      const std::function<QImage()> &render) {
  if (slot >= int(m_layers.size()))
    m_layers.resize(slot + 1);
  Ryu::Layer &l = m_layers[slot];
  if (l.key != key) {
    l.key = key;
    l.img = render();
  }
  l.rect = rect;
  l.on = true;
}

void SgItem::textLayer(int slot, const QFont &font, const QString &text,
                       const QColor &color, qreal x, qreal cy) {
  if (text.isEmpty() || color.alpha() == 0)
    return;
  const qreal dpr = m_mesh.dpr();
  const QColor c = Ryu::withAlpha(color, color.alphaF() * fade());
  const QFontMetricsF fm(font);
  constexpr qreal pad = 1.0;
  const qreal iw = std::ceil(fm.horizontalAdvance(text)) + 2.0 * pad;
  const qreal ih = std::ceil(fm.height()) + 2.0 * pad;
  const QString key = QStringList{"t", QString::number(dpr),
                                  c.name(QColor::HexArgb), font.key(), text}
                          .join(QLatin1Char('|'));
  useLayer(slot, key,
           QRectF(m_mesh.snap(x - pad), m_mesh.snap(cy - ih / 2.0), iw, ih),
           [&] { return Ryu::renderLabel(font, text, c, iw, ih, pad, dpr); });
}

void SgItem::glyphLayer(int slot, const QFont &font, const QString &glyph,
                        const QColor &color, const QPointF &center,
                        qreal size) {
  if (glyph.isEmpty() || size <= 0.0 || color.alpha() == 0)
    return;
  const qreal dpr = m_mesh.dpr();
  const QColor c = Ryu::withAlpha(color, color.alphaF() * fade());
  const QString key =
      QStringList{"g",        QString::number(dpr),  c.name(QColor::HexArgb),
                  font.key(), QString::number(size), glyph}
          .join(QLatin1Char('|'));
  useLayer(slot, key,
           QRectF(m_mesh.snap(center.x() - size / 2.0),
                  m_mesh.snap(center.y() - size / 2.0), size, size),
           [&] { return Ryu::renderGlyph(font, glyph, c, size, dpr); });
}

void SgItem::syncLayers(QSGNode *root) {
  for (Ryu::Layer &l : m_layers) {
    if (!l.on || l.img.isNull() || !window()) {
      if (l.node) {
        root->removeChildNode(l.node);
        delete l.node;
        l.node = nullptr;
      }
      continue;
    }

    if (!l.node) {
      l.node = new QSGSimpleTextureNode;
      l.node->setOwnsTexture(true);
      l.node->setFiltering(QSGTexture::Linear);
      root->appendChildNode(l.node);
      l.shown.clear();
    }

    if (l.shown != l.key) {
      l.node->setTexture(window()->createTextureFromImage(l.img));
      l.shown = l.key;
    }
    l.node->setRect(l.rect);
  }

  while (!m_layers.empty() && !m_layers.back().node && !m_layers.back().on)
    m_layers.pop_back();
}

SgMaterialItem::SgMaterialItem(QQuickItem *parent, qreal stiffness,
                               qreal damping)
    : SgItem(parent), m_stiffness(stiffness), m_damping(damping),
      m_driver(new Ryu::SpringDriver(this)) {
  m_driver->tick = [this](qreal dt) {
    if (dt <= 0.0)
      return;
    const bool running = advance(dt);
    update();
    if (!running)
      m_driver->stop();
  };
}

void SgMaterialItem::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void SgMaterialItem::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
}

void SgMaterialItem::animate() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

bool SgMaterialItem::step(Spring &s, qreal dt, qreal eps,
                          qreal stiffnessScale) const {
  return Ryu::stepSpring(s, m_stiffness * stiffnessScale, m_damping, dt, eps);
}
