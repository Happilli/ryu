#include "./ryuitem.hpp"
#include "./paintutil.hpp"
#include <QPainter>
#include <algorithm>

RyuItem::RyuItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
  setAntialiasing(true);
}

void RyuItem::preparePainter(QPainter *p) const {
  p->setRenderHint(QPainter::Antialiasing, true);
  p->setRenderHint(QPainter::TextAntialiasing, true);
  p->setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (!isEnabled())
    p->setOpacity(Ryu::kDisabledOpacity);
}

MaterialItem::MaterialItem(QQuickItem *parent, qreal stiffness, qreal damping)
    : RyuItem(parent), m_stiffness(stiffness), m_damping(damping),
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

void MaterialItem::setSpringStiffness(qreal v) {
  v = std::max(1.0, v);
  if (qFuzzyCompare(m_stiffness, v))
    return;
  m_stiffness = v;
  emit springStiffnessChanged();
}

void MaterialItem::setSpringDamping(qreal v) {
  v = qBound(0.05, v, 2.0);
  if (qFuzzyCompare(m_damping, v))
    return;
  m_damping = v;
  emit springDampingChanged();
}

void MaterialItem::animate() {
  if (m_driver->state() != QAbstractAnimation::Running)
    m_driver->start();
}

bool MaterialItem::step(Spring &s, qreal dt, qreal eps,
                        qreal stiffnessScale) const {
  return Ryu::stepSpring(s, m_stiffness * stiffnessScale, m_damping, dt, eps);
}
