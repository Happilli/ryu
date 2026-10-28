#pragma once
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFont>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>
#include <QVariant>
#include <QVector>

class MaterialMenu : public SgMaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
  Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
  Q_PROPERTY(bool closeOnTrigger READ closeOnTrigger WRITE setCloseOnTrigger
                 NOTIFY closeOnTriggerChanged)

  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(qreal itemHeight READ itemHeight WRITE setItemHeight NOTIFY
                 itemHeightChanged)
  Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
  Q_PROPERTY(qreal minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY
                 minimumWidthChanged)
  Q_PROPERTY(qreal edgeMargin READ edgeMargin WRITE setEdgeMargin NOTIFY
                 edgeMarginChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor shortcutColor READ shortcutColor WRITE setShortcutColor
                 NOTIFY shortcutColorChanged)
  Q_PROPERTY(QColor dangerColor READ dangerColor WRITE setDangerColor NOTIFY
                 dangerColorChanged)
  Q_PROPERTY(QColor separatorColor READ separatorColor WRITE setSeparatorColor
                 NOTIFY separatorColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)

public:
  explicit MaterialMenu(QQuickItem *parent = nullptr);

  QVariantList model() const { return m_raw; }
  bool isOpen() const { return m_open; }
  int currentIndex() const { return m_current; }
  bool closeOnTrigger() const { return m_closeOnTrigger; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  QFont iconFont() const { return m_iconFont; }
  qreal itemHeight() const { return m_itemH; }
  qreal radius() const { return m_radius; }
  qreal minimumWidth() const { return m_minWidth; }
  qreal edgeMargin() const { return m_edgeMargin; }
  qreal borderWidth() const { return m_borderWidth; }
  QColor color() const { return m_color; }
  QColor textColor() const { return m_textColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor shortcutColor() const { return m_shortcutColor; }
  QColor dangerColor() const { return m_dangerColor; }
  QColor separatorColor() const { return m_separatorColor; }
  QColor borderColor() const { return m_borderColor; }

  void setModel(const QVariantList &v);
  void setOpen(bool v);
  void setCloseOnTrigger(bool v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setIconFont(const QFont &v);
  void setItemHeight(qreal v);
  void setRadius(qreal v);
  void setMinimumWidth(qreal v);
  void setEdgeMargin(qreal v);
  void setBorderWidth(qreal v);
  void setColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setShortcutColor(const QColor &v);
  void setDangerColor(const QColor &v);
  void setSeparatorColor(const QColor &v);
  void setBorderColor(const QColor &v);

  Q_INVOKABLE void popup(qreal x, qreal y);
  Q_INVOKABLE void close() { setOpen(false); }

signals:
  void modelChanged();
  void openChanged();
  void currentIndexChanged();
  void closeOnTriggerChanged();
  void fontChanged();
  void fontWeightChanged();
  void iconFontChanged();
  void itemHeightChanged();
  void radiusChanged();
  void minimumWidthChanged();
  void edgeMarginChanged();
  void borderWidthChanged();
  void colorChanged();
  void textColorChanged();
  void iconColorChanged();
  void shortcutColorChanged();
  void dangerColorChanged();
  void separatorColorChanged();
  void borderColorChanged();
  void triggered(int index, const QVariant &item);
  void opened();
  void closed();

protected:
  void build(Ryu::Mesh &m) override;
  bool advance(qreal dt) override;
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;

private:
  struct Entry {
    QString label;
    QString icon;
    QString shortcut;
    bool separator = false;
    bool enabled = true;
    bool danger = false;
  };

  QFont labelFont() const;
  QFont shortcutFont() const;
  void ensureMetrics() const;
  qreal naturalWidth() const;
  qreal naturalHeight() const;
  void layoutItems();
  void structureChanged();
  bool selectable(int i) const;
  int rowAt(const QPointF &pt) const;
  int step(int from, int dir) const;
  void setCurrent(int i);
  void trigger(int i);
  void retarget();
  void applyPop();

  QVariantList m_raw;
  QVector<Entry> m_items;

  QFont m_font;
  int m_fontWeight = 500;
  QFont m_iconFont;
  qreal m_itemH = 40.0;
  qreal m_radius = 16.0;
  qreal m_minWidth = 200.0;
  qreal m_edgeMargin = 8.0;
  qreal m_borderWidth = 0.0;
  bool m_closeOnTrigger = true;

  QColor m_color = QColor("#F3EDF7");
  QColor m_textColor = QColor("#1D1B20");
  QColor m_iconColor;
  QColor m_shortcutColor = QColor("#49454F");
  QColor m_dangerColor = QColor("#B3261E");
  QColor m_separatorColor = QColor("#CAC4D0");
  QColor m_borderColor = QColor("#79747E");
  QColor m_hlBase = QColor("#1D1B20");

  bool m_open = false;
  int m_current = -1;
  int m_pressIndex = -1;

  QVector<QRectF> m_rects;
  bool m_layoutDirty = true;

  mutable QVector<qreal> m_labelW;
  mutable QVector<qreal> m_shortW;
  mutable qreal m_maxLabelW = 0.0;
  mutable qreal m_maxShortW = 0.0;
  mutable bool m_hasIcon = false;
  mutable bool m_metricsDirty = true;

  Spring m_pop;
  Spring m_hlY;
  Spring m_hlH;
  Spring m_hlA;
};
