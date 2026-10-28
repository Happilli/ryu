#pragma once
#include "./spring.hpp"
#include <QQuickPaintedItem>
#include <QtQmlIntegration/qqmlintegration.h>

class RyuItem : public QQuickPaintedItem {
  Q_OBJECT
  QML_ANONYMOUS

public:
  explicit RyuItem(QQuickItem *parent = nullptr);
  ~RyuItem() override;

protected:
  void itemChange(ItemChange change, const ItemChangeData &data) override;
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

  void preparePainter(QPainter *p) const;
  void setHand(bool on);

private:
  bool m_handOn = false;
};

class MaterialItem : public RyuItem {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(qreal springStiffness READ springStiffness WRITE setSpringStiffness
                 NOTIFY springStiffnessChanged FINAL)
  Q_PROPERTY(qreal springDamping READ springDamping WRITE setSpringDamping
                 NOTIFY springDampingChanged FINAL)

public:
  explicit MaterialItem(QQuickItem *parent = nullptr, qreal stiffness = 520.0,
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
