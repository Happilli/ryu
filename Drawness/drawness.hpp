#pragma once
#include <QColor>
#include <QPointF>
#include <QQuickItem>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QVector>

struct DrawnessPoint {
  float x;
  float y;
};

struct DrawnessStroke {
  QVector<DrawnessPoint> points;
  QColor color;
  qreal width = 4.0;
};

class Drawness : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QColor drawColor READ drawColor WRITE setDrawColor NOTIFY
                 drawColorChanged)
  Q_PROPERTY(
      qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
  Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged)
  Q_PROPERTY(bool canRedo READ canRedo NOTIFY stateChanged)

public:
  explicit Drawness(QQuickItem *parent = nullptr);

  QColor drawColor() const { return m_drawColor; }
  void setDrawColor(const QColor &c);

  qreal brushSize() const { return m_brushSize; }
  void setBrushSize(qreal s);
  bool canUndo() const { return !m_strokes.isEmpty(); }
  bool canRedo() const { return !m_redoStack.isEmpty(); }

public slots:
  void startStroke(qreal x, qreal y);
  void addPoint(qreal x, qreal y);
  void finishStroke();
  void undo();
  void redo();
  void clear();

signals:
  void drawColorChanged();
  void brushSizeChanged();
  void stateChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
  QSGGeometry *createStrokeGeometry(const DrawnessStroke &stroke) const;
  QSGGeometryNode *buildStrokeNode(const DrawnessStroke &stroke,
                                   QSGGeometryNode *existing) const;

  QColor m_drawColor = QColor("#000000");
  qreal m_brushSize = 4.0;
  QVector<DrawnessStroke> m_strokes;
  QVector<DrawnessStroke> m_redoStack;
  QVector<QSGGeometryNode *> m_strokeNodes;
  bool m_isDrawing = false;
  bool m_lastStrokeDirty = false;
};
