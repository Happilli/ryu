#pragma once
#include "./spring.hpp"
#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QSGGeometry>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace Ryu {

class Mesh {
public:
  void begin(qreal dpr, qreal opacity);
  void setOrigin(const QPointF &o) { m_origin = o; }
  qreal dpr() const { return m_dpr; }
  qreal snap(qreal v) const { return std::round(v * m_dpr) / m_dpr; }
  qreal snapStroke(qreal w) const {
    return std::max(1.0 / m_dpr, std::round(w * m_dpr) / m_dpr);
  }

  void fillRoundedRect(const QRectF &r, qreal tl, qreal tr, qreal br, qreal bl,
                       const QColor &c);
  void strokeRoundedRect(const QRectF &r, qreal tl, qreal tr, qreal br,
                         qreal bl, qreal w, const QColor &c);
  void fillCircle(const QPointF &c, qreal r, const QColor &col);
  void strokeCircle(const QPointF &c, qreal r, qreal w, const QColor &col);
  void line(const QPointF &a, const QPointF &b, qreal w, const QColor &c);
  void polyline(const QList<QPointF> &pts, qreal w, const QColor &c);
  void fillPolygon(const QList<QPointF> &pts, const QColor &c) {
    fillOutline(pts, c);
  }
  void strokePolygon(const QList<QPointF> &pts, qreal w, const QColor &c) {
    strokeOutline(pts, w, c);
  }
  void upload(QSGGeometry *g) const;

private:
  using Outline = QList<QPointF>;

  struct Rgba {
    quint8 r, g, b, a;
  };

  Rgba pack(const QColor &c) const;
  int push(const QPointF &p, Rgba c);
  void tri(int a, int b, int c);
  void quad(int a, int b, int c, int d);
  int steps(qreal r, qreal span) const;
  Outline roundedOutline(const QRectF &r, qreal tl, qreal tr, qreal br,
                         qreal bl) const;
  Outline circleOutline(const QPointF &c, qreal r) const;
  QList<QPointF> miters(const Outline &o) const;
  int ring(const Outline &o, const QList<QPointF> &mv, qreal d, Rgba c);
  void fillOutline(const Outline &o, const QColor &c);
  void strokeOutline(const Outline &o, qreal w, const QColor &c);

  std::vector<QSGGeometry::ColoredPoint2D> m_v;
  std::vector<quint32> m_i;
  QPointF m_origin;
  qreal m_dpr = 1.0;
  qreal m_opacity = 1.0;
};

} // namespace Ryu

class QSGSimpleTextureNode;
namespace Ryu {
struct Layer {
  QImage img;
  QRectF rect;
  QString key;
  QString shown;
  bool on = false;
  QSGSimpleTextureNode *node = nullptr;
};
} // namespace Ryu
class SgItem : public QQuickItem {
  Q_OBJECT
  QML_ANONYMOUS

public:
  explicit SgItem(QQuickItem *parent = nullptr);
  ~SgItem() override;

protected:
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
  void itemChange(ItemChange change, const ItemChangeData &data) override;

  virtual void build(Ryu::Mesh &) {}
  virtual void propertyAssigned() {}

  template <typename D, typename T>
  bool assign(T &field, const T &v, void (D::*sig)()) {
    if (field == v)
      return false;
    field = v;
    (static_cast<D *>(this)->*sig)();
    propertyAssigned();
    update();
    return true;
  }

  qreal dpr() const;
  qreal fade() const;
  void setHand(bool on);

  void useLayer(int slot, const QString &key, const QRectF &rect,
                const std::function<QImage()> &render);
  void textLayer(int slot, const QFont &font, const QString &text,
                 const QColor &color, qreal x, qreal cy);
  void glyphLayer(int slot, const QFont &font, const QString &glyph,
                  const QColor &color, const QPointF &center, qreal size);

private:
  void syncLayers(QSGNode *root);

  bool m_handOn = false;
  QMetaObject::Connection m_screenConn;
  Ryu::Mesh m_mesh;
  std::vector<Ryu::Layer> m_layers;
};

class SgMaterialItem : public SgItem {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(qreal springStiffness READ springStiffness WRITE setSpringStiffness
                 NOTIFY springStiffnessChanged FINAL)
  Q_PROPERTY(qreal springDamping READ springDamping WRITE setSpringDamping
                 NOTIFY springDampingChanged FINAL)

public:
  explicit SgMaterialItem(QQuickItem *parent = nullptr, qreal stiffness = 520.0,
                          qreal damping = 0.6);

  qreal springStiffness() const { return m_stiffness; }
  qreal springDamping() const { return m_damping; }
  void setSpringStiffness(qreal v);
  void setSpringDamping(qreal v);

signals:
  void springStiffnessChanged();
  void springDampingChanged();

protected:
  using Spring = Ryu::Spring;

  virtual bool advance(qreal) { return false; }
  void animate();
  bool step(Spring &s, qreal dt, qreal eps = 0.001,
            qreal stiffnessScale = 1.0) const;

private:
  qreal m_stiffness;
  qreal m_damping;
  Ryu::SpringDriver *m_driver;
};
