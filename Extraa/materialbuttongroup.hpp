#pragma once
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QRectF>
#include <QStringList>
#include <QVector>
#include <vector>

class QSGSimpleTextureNode;

class MaterialButtonGroup : public SgMaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(
      QStringList labels READ labels WRITE setLabels NOTIFY labelsChanged)
  Q_PROPERTY(QStringList icons READ icons WRITE setIcons NOTIFY iconsChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(
      Variant variant READ variant WRITE setVariant NOTIFY variantChanged)
  Q_PROPERTY(SizeStyle sizeStyle READ sizeStyle WRITE setSizeStyle NOTIFY
                 sizeStyleChanged)
  Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE
                 setSelectionMode NOTIFY selectionModeChanged)
  Q_PROPERTY(QList<int> checkedIndices READ checkedIndices WRITE
                 setCheckedIndices NOTIFY checkedIndicesChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY
                 checkedIndicesChanged)
  Q_PROPERTY(qreal spacing READ spacing WRITE setSpacing NOTIFY spacingChanged)
  Q_PROPERTY(
      bool fillWidth READ fillWidth WRITE setFillWidth NOTIFY fillWidthChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor checkedColor READ checkedColor WRITE setCheckedColor NOTIFY
                 checkedColorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor checkedTextColor READ checkedTextColor WRITE
                 setCheckedTextColor NOTIFY checkedTextColorChanged)

public:
  enum Variant { Standard, Connected };
  Q_ENUM(Variant)
  enum SizeStyle { ExtraSmall, Small, Medium, Large, ExtraLarge };
  Q_ENUM(SizeStyle)
  enum SelectionMode { NoSelection, Single, Multiple };
  Q_ENUM(SelectionMode)

  explicit MaterialButtonGroup(QQuickItem *parent = nullptr);

  QStringList labels() const { return m_labels; }
  QStringList icons() const { return m_icons; }
  QFont font() const { return m_font; }
  QFont iconFont() const { return m_iconFont; }
  int fontWeight() const { return m_fontWeight; }
  Variant variant() const { return m_variant; }
  SizeStyle sizeStyle() const { return m_sizeStyle; }
  SelectionMode selectionMode() const { return m_mode; }
  QList<int> checkedIndices() const { return m_checked; }
  int currentIndex() const {
    return m_checked.isEmpty() ? -1 : m_checked.first();
  }
  qreal spacing() const { return m_spacing; }
  bool fillWidth() const { return m_fillWidth; }
  qreal borderWidth() const { return m_borderWidth; }
  QColor borderColor() const { return m_borderColor; }
  QColor color() const { return m_color; }
  QColor checkedColor() const { return m_checkedColor; }
  QColor textColor() const { return m_textColor; }
  QColor checkedTextColor() const { return m_checkedTextColor; }

  void setLabels(const QStringList &v);
  void setIcons(const QStringList &v);
  void setFont(const QFont &v);
  void setIconFont(const QFont &v);
  void setFontWeight(int v);
  void setVariant(Variant v);
  void setSizeStyle(SizeStyle v);
  void setSelectionMode(SelectionMode v);
  void setCheckedIndices(const QList<int> &v);
  void setCurrentIndex(int i);
  void setSpacing(qreal v);
  void setFillWidth(bool v);
  void setBorderWidth(qreal v);
  void setBorderColor(const QColor &v);
  void setColor(const QColor &v);
  void setCheckedColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setCheckedTextColor(const QColor &v);

  Q_INVOKABLE bool isChecked(int i) const { return m_checked.contains(i); }
  Q_INVOKABLE void setChecked(int i, bool on);

signals:
  void labelsChanged();
  void iconsChanged();
  void fontChanged();
  void iconFontChanged();
  void fontWeightChanged();
  void variantChanged();
  void sizeStyleChanged();
  void selectionModeChanged();
  void checkedIndicesChanged();
  void spacingChanged();
  void fillWidthChanged();
  void borderWidthChanged();
  void borderColorChanged();
  void colorChanged();
  void checkedColorChanged();
  void textColorChanged();
  void checkedTextColorChanged();
  void clicked(int index);
  void toggled(int index, bool checked);

protected:
  void build(Ryu::Mesh &m) override;
  bool advance(qreal dt) override;
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *d) override;
  void componentComplete() override;
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  struct Cell {
    QImage img;
    QRectF rect;
    QString key;
    QString shown;
    bool on = false;
    QSGSimpleTextureNode *node = nullptr;
  };

  int count() const { return qMax(m_labels.size(), m_icons.size()); }
  QFont labelFont() const;
  void ensureMetrics() const;
  qreal spacingValue() const;
  qreal itemWidth(int i) const;
  qreal naturalWidth() const;
  void layoutItems();
  void structureChanged();
  int hit(const QPointF &pt) const;
  void activate(int i);
  qreal targetRound(int i) const;
  qreal targetWidth(int i) const;
  void retarget(bool immediate = false);
  QVector<QRectF> displayRects() const;
  void syncCell(QSGNode *root, Cell &c);

  QStringList m_labels;
  QStringList m_icons;
  QFont m_font;
  QFont m_iconFont;
  int m_fontWeight = 400;
  Variant m_variant = Standard;
  SizeStyle m_sizeStyle = Small;
  SelectionMode m_mode = NoSelection;
  QList<int> m_checked;
  qreal m_spacing = -1.0;
  bool m_fillWidth = false;
  qreal m_borderWidth = 0.0;
  QColor m_borderColor = QColor("#79747E");

  QColor m_color = QColor("#E8DEF8");
  QColor m_checkedColor = QColor("#6750A4");
  QColor m_textColor = QColor("#1D192B");
  QColor m_checkedTextColor = QColor("#FFFFFF");

  QVector<QRectF> m_rects;
  bool m_layoutDirty = true;

  mutable QVector<qreal> m_textW;
  mutable bool m_metricsDirty = true;

  int m_pressIndex = -1;
  int m_focusIndex = 0;
  bool m_keyFocus = false;

  QVector<Spring> m_round;
  QVector<Spring> m_width;

  std::vector<Cell> m_cells;
};
