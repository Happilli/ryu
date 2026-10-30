#pragma once
#include "../Core/paintutil.hpp"
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>
#include <QUrl>

class MaterialButton : public SgMaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
  Q_PROPERTY(
      QString iconText READ iconText WRITE setIconText NOTIFY iconTextChanged)
  Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY
                 iconSourceChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(
      qreal iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)
  Q_PROPERTY(IconPosition iconPosition READ iconPosition WRITE setIconPosition
                 NOTIFY iconPositionChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)

  Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(SizeStyle sizeStyle READ sizeStyle WRITE setSizeStyle NOTIFY
                 sizeStyleChanged)
  Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY shapeChanged)
  Q_PROPERTY(
      bool checkable READ checkable WRITE setCheckable NOTIFY checkableChanged)
  Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
  Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)

  Q_PROPERTY(qreal horizontalPadding READ horizontalPadding WRITE
                 setHorizontalPadding NOTIFY horizontalPaddingChanged)
  Q_PROPERTY(qreal iconSpacing READ iconSpacing WRITE setIconSpacing NOTIFY
                 iconSpacingChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor checkedColor READ checkedColor WRITE setCheckedColor NOTIFY
                 checkedColorChanged)
  Q_PROPERTY(QColor contentColor READ contentColor WRITE setContentColor NOTIFY
                 contentColorChanged)
  Q_PROPERTY(QColor checkedContentColor READ checkedContentColor WRITE
                 setCheckedContentColor NOTIFY checkedContentColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

public:
  enum Type { Filled, Tonal, Elevated, Outlined, Text };
  Q_ENUM(Type)
  enum SizeStyle { ExtraSmall, Small, Medium, Large, ExtraLarge };
  Q_ENUM(SizeStyle)
  enum Shape { Round, Square };
  Q_ENUM(Shape)
  enum IconPosition { Leading, Trailing };
  Q_ENUM(IconPosition)

  explicit MaterialButton(QQuickItem *parent = nullptr);

  QString label() const { return m_label; }
  QString iconText() const { return m_iconText; }
  QUrl iconSource() const { return m_iconSource; }
  QFont iconFont() const { return m_iconFont; }
  qreal iconSize() const { return m_iconSize; }
  IconPosition iconPosition() const { return m_iconPosition; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  Type type() const { return m_type; }
  SizeStyle sizeStyle() const { return m_sizeStyle; }
  Shape shape() const { return m_shape; }
  bool checkable() const { return m_checkable; }
  bool checked() const { return m_checked; }
  bool pressed() const { return m_pressed; }
  bool hovered() const { return m_hovered; }
  qreal horizontalPadding() const { return m_padX; }
  qreal iconSpacing() const { return m_iconGap; }
  QColor color() const { return m_color; }
  QColor checkedColor() const { return m_checkedColor; }
  QColor contentColor() const { return m_contentColor; }
  QColor checkedContentColor() const { return m_checkedContentColor; }
  QColor borderColor() const { return m_borderColor; }
  qreal borderWidth() const { return m_borderWidth; }

  void setLabel(const QString &v);
  void setIconText(const QString &v);
  void setIconSource(const QUrl &v);
  void setIconFont(const QFont &v);
  void setIconSize(qreal v);
  void setIconPosition(IconPosition v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setType(Type v);
  void setSizeStyle(SizeStyle v);
  void setShape(Shape v);
  void setCheckable(bool v);
  void setChecked(bool v);
  void setHorizontalPadding(qreal v);
  void setIconSpacing(qreal v);
  void setColor(const QColor &v);
  void setCheckedColor(const QColor &v);
  void setContentColor(const QColor &v);
  void setCheckedContentColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setBorderWidth(qreal v);

  Q_INVOKABLE void click();

signals:
  void labelChanged();
  void iconTextChanged();
  void iconSourceChanged();
  void iconFontChanged();
  void iconSizeChanged();
  void iconPositionChanged();
  void fontChanged();
  void fontWeightChanged();
  void typeChanged();
  void sizeStyleChanged();
  void shapeChanged();
  void checkableChanged();
  void checkedChanged();
  void pressedChanged();
  void hoveredChanged();
  void horizontalPaddingChanged();
  void iconSpacingChanged();
  void colorChanged();
  void checkedColorChanged();
  void contentColorChanged();
  void checkedContentColorChanged();
  void borderColorChanged();
  void borderWidthChanged();
  void clicked();
  void toggled();

protected:
  void build(Ryu::Mesh &m) override;
  bool advance(qreal dt) override;
  void componentComplete() override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  QFont labelFont() const;
  bool hasIcon() const;
  qreal textWidth() const;
  qreal resolvedIconSize() const;
  qreal resolvedPadding() const;
  qreal resolvedIconGap() const;
  qreal resolvedBorderWidth() const;
  QColor containerColor() const;
  QColor foregroundColor() const;
  qreal targetRound() const;
  void updateImplicit();
  void retarget(bool immediate = false);
  void setPressed(bool v);
  void setHovered(bool v);
  void activate();

  QString m_label;
  QString m_iconText;
  QUrl m_iconSource;
  QString m_iconPath;
  QFont m_iconFont;
  qreal m_iconSize = -1.0;
  IconPosition m_iconPosition = Leading;
  QFont m_font;
  int m_fontWeight = 500;

  Type m_type = Filled;
  SizeStyle m_sizeStyle = Small;
  Shape m_shape = Round;
  bool m_checkable = false;
  bool m_checked = false;
  bool m_pressed = false;
  bool m_hovered = false;
  bool m_keyFocus = false;
  bool m_keyDown = false;

  qreal m_padX = -1.0;
  qreal m_iconGap = -1.0;

  QColor m_color;
  QColor m_checkedColor;
  QColor m_contentColor;
  QColor m_checkedContentColor;
  QColor m_borderColor = QColor("#79747E");
  qreal m_borderWidth = -1.0;

  qreal m_textW = 0.0;
  Ryu::IconCache m_iconCache;

  Spring m_round;
  Spring m_press;
};
