#include "drawness.hpp"
#include <QSGGeometry>
#include <cmath>

Drawness::Drawness(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(ItemHasContents, true);
}

void Drawness::setDrawColor(const QColor &c) {
  if (m_drawColor == c) {
    return;
  }
  m_drawColor = c;
  emit drawColorChanged();
}

void Drawness::setBrushSize(qreal s) {
  if (qFuzzyCompare(m_brushSize, s)) {
    return;
  }
  m_brushSize = s;
  emit brushSizeChanged();
}

void Drawness::startStroke(qreal x, qreal y) {
  m_redoStack.clear();
  DrawnessStroke stroke;
  stroke.color = m_drawColor;
  stroke.width = m_brushSize;
  stroke.points.append({float(x), float(y)});
  m_strokes.append(stroke);
  m_isDrawing = true;
  m_lastStrokeDirty = true;
  emit stateChanged();
  update();
}

void Drawness::addPoint(qreal x, qreal y) {
  if (!m_isDrawing || m_strokes.isEmpty()) {
    return;
  }
  m_strokes.last().points.append({float(x), float(y)});
  m_lastStrokeDirty = true;
  update();
}

void Drawness::finishStroke() { m_isDrawing = false; }

void Drawness::undo() {
  if (m_strokes.isEmpty())
    return;
  m_redoStack.append(m_strokes.takeLast());
  emit stateChanged();
  update();
}

void Drawness::redo() {
  if (m_redoStack.isEmpty())
    return;
  m_strokes.append(m_redoStack.takeLast());
  emit stateChanged();
  update();
}

void Drawness::clear() {
  m_strokes.clear();
  m_redoStack.clear();
  emit stateChanged();
  update();
}

QSGGeometry *
Drawness::createStrokeGeometry(const DrawnessStroke &stroke) const {
  const auto &pts = stroke.points;
  const int vertexCount = pts.size() * 2;
  auto *geometry =
      new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), vertexCount);
  geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
  auto *vertices = geometry->vertexDataAsPoint2D();

  const float hw = float(stroke.width) / 2.0f;

  for (int i = 0; i < pts.size(); ++i) {
    QPointF dir;
    if (i == 0) {
      dir = QPointF(pts[1].x - pts[0].x, pts[1].y - pts[0].y);
    } else if (i == pts.size() - 1) {
      dir = QPointF(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    } else {
      dir = QPointF(pts[i + 1].x - pts[i - 1].x, pts[i + 1].y - pts[i - 1].y);
    }
    const qreal len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    QPointF normal =
        (len > 0.0001) ? QPointF(-dir.y() / len, dir.x() / len) : QPointF(0, 0);

    vertices[i * 2].set(pts[i].x + normal.x() * hw, pts[i].y + normal.y() * hw);
    vertices[i * 2 + 1].set(pts[i].x - normal.x() * hw,
                            pts[i].y - normal.y() * hw);
  }

  return geometry;
}

QSGGeometryNode *Drawness::buildStrokeNode(const DrawnessStroke &stroke,
                                           QSGGeometryNode *existing) const {
  if (stroke.points.size() < 2) {
    return existing;
  }

  QSGGeometry *geometry = createStrokeGeometry(stroke);

  QSGGeometryNode *node = existing;
  if (!node) {
    node = new QSGGeometryNode();
    node->setFlag(QSGNode::OwnsGeometry);
    auto *material = new QSGFlatColorMaterial();
    material->setColor(stroke.color);
    node->setMaterial(material);
    node->setFlag(QSGNode::OwnsMaterial);
  }
  node->setGeometry(geometry);
  node->markDirty(QSGNode::DirtyGeometry);
  return node;
}

QSGNode *Drawness::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
  auto *root = oldNode ? oldNode : new QSGNode();

  while (m_strokeNodes.size() > m_strokes.size()) {
    QSGGeometryNode *node = m_strokeNodes.takeLast();
    if (node) {
      root->removeChildNode(node);
      delete node;
    }
  }

  for (int i = 0; i < m_strokes.size(); ++i) {
    const bool isNew = i >= m_strokeNodes.size();
    const bool isLast = (i == m_strokes.size() - 1);

    if (isNew) {
      m_strokeNodes.append(nullptr);
    }

    if (isNew || (isLast && m_lastStrokeDirty)) {
      QSGGeometryNode *existing = m_strokeNodes[i];
      QSGGeometryNode *node = buildStrokeNode(m_strokes[i], existing);
      if (node != existing) {
        if (node) {
          root->appendChildNode(node);
        }
        m_strokeNodes[i] = node;
      }
    }
  }

  m_lastStrokeDirty = false;
  return root;
}
