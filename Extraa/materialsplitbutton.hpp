#pragma once
#include "../Core/ryuitem.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>

class MaterialSplitButton : public MaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
  Q_PROPERTY(
      QString iconText READ iconText WRITE setIconText NOTIFY iconTextChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(SizeStyle sizeStyle READ sizeStyle WRITE setSizeStyle NOTIFY
                 sizeStyleChanged)
  Q_PROPERTY(
      bool menuOpen READ menuOpen WRITE setMenuOpen NOTIFY menuOpenChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor contentColor READ contentColor WRITE setContentColor NOTIFY
                 contentColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

public:
  enum Type { Filled, Tonal, Elevated, Outlined };
  Q_ENUM(Type)
  enum SizeStyle { ExtraSmall, Small, Medium, Large, ExtraLarge };
  Q_ENUM(SizeStyle)

  explicit MaterialSplitButton(QQuickItem *parent = nullptr);

  QString label() const { return m_label; }
  QString iconText() const { return m_iconText; }
  QFont iconFont() const { return m_iconFont; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  Type type() const { return m_type; }
  SizeStyle sizeStyle() const { return m_sizeStyle; }
  bool menuOpen() const { return m_menuOpen; }
  QColor color() const { return m_color; }
  QColor contentColor() const { return m_contentColor; }
  QColor borderColor() const { return m_borderColor; }
  qreal borderWidth() const { return m_borderWidth; }

  void setLabel(const QString &v);
  void setIconText(const QString &v);
  void setIconFont(const QFont &v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setType(Type v);
  void setSizeStyle(SizeStyle v);
  void setMenuOpen(bool v);
  void setColor(const QColor &v);
  void setContentColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setBorderWidth(qreal v);

  void paint(QPainter *p) override;

signals:
  void labelChanged();
  void iconTextChanged();
  void iconFontChanged();
  void fontChanged();
  void fontWeightChanged();
  void typeChanged();
  void sizeStyleChanged();
  void menuOpenChanged();
  void colorChanged();
  void contentColorChanged();
  void borderColorChanged();
  void borderWidthChanged();
  void clicked();
  void menuClicked();

protected:
  bool advance(qreal dt) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  enum Zone { None = -1, Lead = 0, Trail = 1 };

  QFont labelFont() const;
  qreal textWidth() const;
  qreal leadWidth() const;
  QColor containerColor() const;
  QColor foregroundColor() const;
  void updateImplicit();
  int hit(const QPointF &pt) const;
  void setPressZone(int z);
  void setHoverZone(int z);
  void activate(int zone);
  void retarget(bool immediate = false);

  QString m_label;
  QString m_iconText;
  QFont m_iconFont;
  QFont m_font;
  int m_fontWeight = 500;
  Type m_type = Filled;
  SizeStyle m_sizeStyle = Small;
  bool m_menuOpen = false;

  QColor m_color;
  QColor m_contentColor;
  QColor m_borderColor = QColor("#79747E");
  qreal m_borderWidth = 1.0;

  int m_pressZone = None;
  int m_hoverZone = None;
  bool m_keyFocus = false;

  Spring m_roundLead;
  Spring m_roundTrail;
  Spring m_chev;
};
