#pragma once
#include "../Core/sgitem.hpp"
#include "../Core/paintutil.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QRectF>
#include <QStringList>
#include <QUrl>
#include <QVector>

class MaterialSegmentedButton : public SgMaterialItem {
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
  Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE
                 setSelectionMode NOTIFY selectionModeChanged)
  Q_PROPERTY(QList<int> checkedIndices READ checkedIndices WRITE
                 setCheckedIndices NOTIFY checkedIndicesChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY
                 checkedIndicesChanged)
  Q_PROPERTY(QList<int> disabledIndices READ disabledIndices WRITE
                 setDisabledIndices NOTIFY disabledIndicesChanged)
  Q_PROPERTY(
      bool showCheck READ showCheck WRITE setShowCheck NOTIFY showCheckChanged)
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
  Q_PROPERTY(QUrl checkIconSource READ checkIconSource WRITE setCheckIconSource
                 NOTIFY checkIconSourceChanged)

public:
  enum SelectionMode { Single, Multiple };
  Q_ENUM(SelectionMode)

  explicit MaterialSegmentedButton(QQuickItem *parent = nullptr);

  QStringList labels() const { return m_labels; }
  QStringList icons() const { return m_icons; }
  QFont font() const { return m_font; }
  QFont iconFont() const { return m_iconFont; }
  int fontWeight() const { return m_fontWeight; }
  SelectionMode selectionMode() const { return m_mode; }
  QList<int> checkedIndices() const { return m_checked; }
  int currentIndex() const {
    return m_checked.isEmpty() ? -1 : m_checked.first();
  }
  QList<int> disabledIndices() const { return m_disabled; }
  bool showCheck() const { return m_showCheck; }
  bool fillWidth() const { return m_fillWidth; }
  qreal borderWidth() const { return m_borderWidth; }
  QColor borderColor() const { return m_borderColor; }
  QColor color() const { return m_color; }
  QColor checkedColor() const { return m_checkedColor; }
  QColor textColor() const { return m_textColor; }
  QColor checkedTextColor() const { return m_checkedTextColor; }
  QUrl checkIconSource() const { return m_checkIconSource; }

  void setCheckIconSource(const QUrl &v);
  void setLabels(const QStringList &v);
  void setIcons(const QStringList &v);
  void setFont(const QFont &v);
  void setIconFont(const QFont &v);
  void setFontWeight(int v);
  void setSelectionMode(SelectionMode v);
  void setCheckedIndices(const QList<int> &v);
  void setCurrentIndex(int i);
  void setDisabledIndices(const QList<int> &v);
  void setShowCheck(bool v);
  void setFillWidth(bool v);
  void setBorderWidth(qreal v);
  void setBorderColor(const QColor &v);
  void setColor(const QColor &v);
  void setCheckedColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setCheckedTextColor(const QColor &v);

  Q_INVOKABLE bool isChecked(int i) const { return m_checked.contains(i); }
  Q_INVOKABLE bool isDisabled(int i) const { return m_disabled.contains(i); }
  Q_INVOKABLE void setChecked(int i, bool on);

signals:
  void checkIconSourceChanged();
  void labelsChanged();
  void iconsChanged();
  void fontChanged();
  void iconFontChanged();
  void fontWeightChanged();
  void selectionModeChanged();
  void checkedIndicesChanged();
  void disabledIndicesChanged();
  void showCheckChanged();
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
  void componentComplete() override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  int count() const { return qMax(m_labels.size(), m_icons.size()); }
  QFont labelFont() const;
  void ensureMetrics() const;
  bool hasIcon(int i) const;
  qreal slotWidth(int i) const;
  qreal segmentWidth(int i) const;
  qreal naturalWidth() const;
  QVector<QRectF> layoutRects() const;
  void structureChanged();
  int hit(const QPointF &pt) const;
  int stepFocus(int from, int dir) const;
  void activate(int i);
  qreal targetWidth(int i) const;
  void kick(int i);
  void retarget(bool immediate = false);

  QStringList m_labels;
  QStringList m_icons;
  QFont m_font;
  QFont m_iconFont;
  int m_fontWeight = 500;
  SelectionMode m_mode = Single;
  QList<int> m_checked;
  QList<int> m_disabled;
  bool m_showCheck = true;
  bool m_fillWidth = false;
  qreal m_borderWidth = 1.0;
  QColor m_borderColor = QColor("#79747E");

  QUrl m_checkIconSource;
  QString m_checkPath;
  Ryu::IconCache m_checkCache;

  QColor m_color = QColor(Qt::transparent);
  QColor m_checkedColor = QColor("#E8DEF8");
  QColor m_textColor = QColor("#1D1B20");
  QColor m_checkedTextColor = QColor("#1D192B");

  mutable QVector<qreal> m_textW;
  mutable bool m_metricsDirty = true;

  int m_pressIndex = -1;
  int m_hoverIndex = -1;
  int m_focusIndex = 0;
  bool m_keyFocus = false;

  QVector<Spring> m_fill;
  QVector<Spring> m_check;
  QVector<Spring> m_width;
};
