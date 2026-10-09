#pragma once
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickPaintedItem>
#include <QString>
#include <QUrl>
#include <QtQmlIntegration/qqmlintegration.h>

class QAbstractAnimation;

class MaterialIconButton : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(SizeStyle sizeStyle READ sizeStyle WRITE setSizeStyle NOTIFY
                 sizeStyleChanged)
  Q_PROPERTY(WidthStyle widthStyle READ widthStyle WRITE setWidthStyle NOTIFY
                 widthStyleChanged)
  Q_PROPERTY(Shape shape READ shape WRITE setShape NOTIFY shapeChanged)
  Q_PROPERTY(
      bool checkable READ checkable WRITE setCheckable NOTIFY checkableChanged)
  Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
  Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)

  Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY
                 iconSourceChanged)
  Q_PROPERTY(
      QString iconText READ iconText WRITE setIconText NOTIFY iconTextChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(
      qreal iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor checkedColor READ checkedColor WRITE setCheckedColor NOTIFY
                 checkedColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor checkedIconColor READ checkedIconColor WRITE
                 setCheckedIconColor NOTIFY checkedIconColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

  Q_PROPERTY(qreal springStiffness READ springStiffness WRITE setSpringStiffness
                 NOTIFY springStiffnessChanged)
  Q_PROPERTY(qreal springDamping READ springDamping WRITE setSpringDamping
                 NOTIFY springDampingChanged)

public:
  enum Type { Filled, Tonal, Outlined, Standard };
  Q_ENUM(Type)
  enum SizeStyle { ExtraSmall, Small, Medium, Large, ExtraLarge };
  Q_ENUM(SizeStyle)
  enum WidthStyle { Default, Narrow, Wide };
  Q_ENUM(WidthStyle)
  enum Shape { Round, Square };
  Q_ENUM(Shape)

  explicit MaterialIconButton(QQuickItem *parent = nullptr);

  Type type() const { return m_type; }
  SizeStyle sizeStyle() const { return m_sizeStyle; }
  WidthStyle widthStyle() const { return m_widthStyle; }
  Shape shape() const { return m_shape; }
  bool checkable() const { return m_checkable; }
  bool checked() const { return m_checked; }
  bool pressed() const { return m_pressed; }
  bool hovered() const { return m_hovered; }

  QUrl iconSource() const { return m_iconSource; }
  QString iconText() const { return m_iconText; }
  QFont iconFont() const { return m_iconFont; }
  qreal iconSize() const { return m_iconSize; }

  QColor color() const { return m_color; }
  QColor checkedColor() const { return m_checkedColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor checkedIconColor() const { return m_checkedIconColor; }
  QColor borderColor() const { return m_borderColor; }
  qreal borderWidth() const { return m_borderWidth; }

  qreal springStiffness() const { return m_stiffness; }
  qreal springDamping() const { return m_damping; }

  void setType(Type v);
  void setSizeStyle(SizeStyle v);
  void setWidthStyle(WidthStyle v);
  void setShape(Shape v);
  void setCheckable(bool v);
  void setChecked(bool v);

  void setIconSource(const QUrl &v);
  void setIconText(const QString &v);
  void setIconFont(const QFont &v);
  void setIconSize(qreal v);

  void setColor(const QColor &v);
  void setCheckedColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setCheckedIconColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setBorderWidth(qreal v);

  void setSpringStiffness(qreal v);
  void setSpringDamping(qreal v);

  void paint(QPainter *p) override;

signals:
  void typeChanged();
  void sizeStyleChanged();
  void widthStyleChanged();
  void shapeChanged();
  void checkableChanged();
  void checkedChanged();
  void pressedChanged();
  void hoveredChanged();
  void iconSourceChanged();
  void iconTextChanged();
  void iconFontChanged();
  void iconSizeChanged();
  void colorChanged();
  void checkedColorChanged();
  void iconColorChanged();
  void checkedIconColorChanged();
  void borderColorChanged();
  void borderWidthChanged();
  void springStiffnessChanged();
  void springDampingChanged();
  void clicked();
  void toggled();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  struct Spring {
    qreal x = 0.0;
    qreal v = 0.0;
    qreal t = 0.0;
  };

  template <typename T, typename Sig>
  bool assign(T &field, const T &v, Sig sig) {
    if (field == v)
      return false;
    field = v;
    (this->*sig)();
    update();
    return true;
  }

  QColor containerColor() const;
  QColor contentColor() const;
  QColor outlineColor() const;
  qreal resolvedIconSize() const;
  qreal resolvedBorderWidth() const;
  qreal targetRound() const;
  void updateImplicit();
  void retarget(bool immediate = false);
  void ensureDriver();
  void tick(qreal dt);
  void setPressed(bool v);
  void setHovered(bool v);
  void activate();
  void buildIcon(qreal dpr, const QColor &c);

  Type m_type = Filled;
  SizeStyle m_sizeStyle = Small;
  WidthStyle m_widthStyle = Default;
  Shape m_shape = Round;
  bool m_checkable = false;
  bool m_checked = false;
  bool m_pressed = false;
  bool m_hovered = false;
  bool m_keyFocus = false;

  QUrl m_iconSource;
  QString m_iconText;
  QFont m_iconFont;
  qreal m_iconSize = -1.0;
  QImage m_icon;
  int m_iconPx = 0;
  QColor m_iconBuiltColor;
  bool m_iconDirty = true;

  QColor m_color;
  QColor m_checkedColor;
  QColor m_iconColor;
  QColor m_checkedIconColor;
  QColor m_borderColor = QColor("#79747E");
  qreal m_borderWidth = -1.0;

  qreal m_stiffness = 520.0;
  qreal m_damping = 0.5;

  Spring m_round;
  Spring m_press;
  QAbstractAnimation *m_driver = nullptr;
};
