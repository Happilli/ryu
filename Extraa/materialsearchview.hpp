#pragma once
#include "../Core/ryuitem.hpp"
#include <QColor>
#include <QFont>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <QWheelEvent>

class MaterialSearchView : public MaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(int count READ count NOTIFY modelChanged)
  Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY
                 currentIndexChanged)
  Q_PROPERTY(bool autoSelect READ autoSelect WRITE setAutoSelect NOTIFY
                 autoSelectChanged)
  Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY contentHeightChanged)
  Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY
                 emptyTextChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(qreal itemHeight READ itemHeight WRITE setItemHeight NOTIFY
                 itemHeightChanged)
  Q_PROPERTY(int maxVisibleItems READ maxVisibleItems WRITE setMaxVisibleItems
                 NOTIFY maxVisibleItemsChanged)
  Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor supportingColor READ supportingColor WRITE
                 setSupportingColor NOTIFY supportingColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor highlightColor READ highlightColor WRITE setHighlightColor
                 NOTIFY highlightColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)

public:
  explicit MaterialSearchView(QQuickItem *parent = nullptr);

  QVariantList model() const { return m_raw; }
  int count() const { return m_items.size(); }
  bool isOpen() const { return m_open; }
  int currentIndex() const { return m_current; }
  bool autoSelect() const { return m_autoSelect; }
  qreal contentHeight() const;
  QString emptyText() const { return m_emptyText; }
  QFont font() const { return m_font; }
  QFont iconFont() const { return m_iconFont; }
  qreal itemHeight() const { return m_itemH; }
  int maxVisibleItems() const { return m_maxVisible; }
  qreal radius() const { return m_radius; }
  qreal borderWidth() const { return m_borderWidth; }
  QColor color() const { return m_color; }
  QColor textColor() const { return m_textColor; }
  QColor supportingColor() const { return m_supportingColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor highlightColor() const { return m_highlightColor; }
  QColor borderColor() const { return m_borderColor; }

  void setModel(const QVariantList &v);
  void setOpen(bool v);
  void setCurrentIndex(int i);
  void setAutoSelect(bool v);
  void setEmptyText(const QString &v);
  void setFont(const QFont &v);
  void setIconFont(const QFont &v);
  void setItemHeight(qreal v);
  void setMaxVisibleItems(int v);
  void setRadius(qreal v);
  void setBorderWidth(qreal v);
  void setColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setSupportingColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setHighlightColor(const QColor &v);
  void setBorderColor(const QColor &v);

  Q_INVOKABLE void moveCurrent(int dir);
  Q_INVOKABLE void triggerCurrent();
  Q_INVOKABLE void close() { setOpen(false); }

  void paint(QPainter *p) override;

signals:
  void modelChanged();
  void openChanged();
  void currentIndexChanged();
  void autoSelectChanged();
  void contentHeightChanged();
  void emptyTextChanged();
  void fontChanged();
  void iconFontChanged();
  void itemHeightChanged();
  void maxVisibleItemsChanged();
  void radiusChanged();
  void borderWidthChanged();
  void colorChanged();
  void textColorChanged();
  void supportingColorChanged();
  void iconColorChanged();
  void highlightColorChanged();
  void borderColorChanged();
  void triggered(int index, const QVariant &item);
  void opened();
  void closed();

protected:
  bool advance(qreal dt) override;
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;

private:
  struct Entry {
    QString headline;
    QString supporting;
    QString trailingText;
    QString leadingIcon;
    bool enabled = true;
  };

  qreal viewport() const;
  qreal maxScroll() const;
  bool selectable(int i) const;
  int rowAt(const QPointF &pt) const;
  int nextSelectable(int from, int dir) const;
  void setCurrentInternal(int i, bool reveal);
  void structureChanged();
  void retarget(bool reveal);
  void applyPop();
  void trigger(int i);

  QVariantList m_raw;
  QVector<Entry> m_items;

  bool m_open = false;
  bool m_autoSelect = true;
  int m_current = -1;
  int m_pressIndex = -1;
  QString m_emptyText = QStringLiteral("No results");

  QFont m_font;
  QFont m_iconFont;
  qreal m_itemH = 64.0;
  int m_maxVisible = 6;
  qreal m_radius = 28.0;
  qreal m_borderWidth = 0.0;

  QColor m_color = QColor("#ECE6F0");
  QColor m_textColor = QColor("#1D1B20");
  QColor m_supportingColor = QColor("#49454F");
  QColor m_iconColor = QColor("#49454F");
  QColor m_highlightColor = QColor("#E8DEF8");
  QColor m_borderColor = QColor("#79747E");

  Spring m_pop;
  Spring m_hlY;
  Spring m_hlA;
  Spring m_scroll;
};
