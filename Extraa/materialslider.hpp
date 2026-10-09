#pragma once
#include <QColor>
#include <QFont>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickPaintedItem>
#include <QString>
#include <QUrl>
#include <QVariantAnimation>
#include <QtQmlIntegration/qqmlintegration.h>

class MaterialSlider : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(qreal from READ from WRITE setFrom NOTIFY fromChanged)
  Q_PROPERTY(qreal to READ to WRITE setTo NOTIFY toChanged)
  Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
  Q_PROPERTY(
      qreal stepSize READ stepSize WRITE setStepSize NOTIFY stepSizeChanged)
  Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation
                 NOTIFY orientationChanged)
  Q_PROPERTY(SizeStyle sizeStyle READ sizeStyle WRITE setSizeStyle NOTIFY
                 sizeStyleChanged)
  Q_PROPERTY(
      bool showStops READ showStops WRITE setShowStops NOTIFY showStopsChanged)
  Q_PROPERTY(bool showValueIndicator READ showValueIndicator WRITE
                 setShowValueIndicator NOTIFY showValueIndicatorChanged)
  Q_PROPERTY(
      int decimals READ decimals WRITE setDecimals NOTIFY decimalsChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)

  Q_PROPERTY(qreal trackHeight READ trackHeight WRITE setTrackHeight NOTIFY
                 trackHeightChanged)
  Q_PROPERTY(qreal handleWidth READ handleWidth WRITE setHandleWidth NOTIFY
                 handleWidthChanged)
  Q_PROPERTY(qreal handleHeight READ handleHeight WRITE setHandleHeight NOTIFY
                 handleHeightChanged)
  Q_PROPERTY(
      qreal handleGap READ handleGap WRITE setHandleGap NOTIFY handleGapChanged)
  Q_PROPERTY(qreal innerRadius READ innerRadius WRITE setInnerRadius NOTIFY
                 innerRadiusChanged)
  Q_PROPERTY(
      qreal stopSize READ stopSize WRITE setStopSize NOTIFY stopSizeChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

  Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY
                 iconSourceChanged)
  Q_PROPERTY(
      QString iconText READ iconText WRITE setIconText NOTIFY iconTextChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(
      qreal iconSize READ iconSize WRITE setIconSize NOTIFY iconSizeChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY
                 trackColorChanged)
  Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor NOTIFY
                 handleColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(QColor indicatorColor READ indicatorColor WRITE setIndicatorColor
                 NOTIFY indicatorColorChanged)
  Q_PROPERTY(QColor indicatorTextColor READ indicatorTextColor WRITE
                 setIndicatorTextColor NOTIFY indicatorTextColorChanged)
  Q_PROPERTY(qreal outerRadius READ outerRadius WRITE setOuterRadius NOTIFY
                 outerRadiusChanged)

public:
  enum Orientation { Horizontal, Vertical };
  Q_ENUM(Orientation)

  enum SizeStyle { ExtraSmall, Small, Medium, Large, ExtraLarge };
  Q_ENUM(SizeStyle)

  explicit MaterialSlider(QQuickItem *parent = nullptr);

  qreal from() const { return m_from; }
  qreal to() const { return m_to; }
  qreal value() const { return m_value; }
  qreal stepSize() const { return m_step; }
  Orientation orientation() const { return m_orientation; }
  SizeStyle sizeStyle() const { return m_sizeStyle; }
  bool showStops() const { return m_showStops; }
  bool showValueIndicator() const { return m_showIndicator; }
  int decimals() const { return m_decimals; }
  bool pressed() const { return m_pressed; }

  qreal trackHeight() const;
  qreal handleWidth() const;
  qreal handleHeight() const;
  qreal handleGap() const;
  qreal innerRadius() const;
  qreal stopSize() const;
  qreal outerRadius() const;
  qreal borderWidth() const { return m_borderWidth; }

  QUrl iconSource() const { return m_iconSource; }
  QString iconText() const { return m_iconText; }
  QFont iconFont() const { return m_iconFont; }
  QColor iconColor() const { return m_iconColor; }
  qreal iconSize() const { return m_iconSize; }

  QColor color() const { return m_color; }
  QColor trackColor() const { return m_trackColor; }
  QColor handleColor() const { return m_handleColor; }
  QColor borderColor() const { return m_borderColor; }
  QColor indicatorColor() const { return m_indicatorColor; }
  QColor indicatorTextColor() const { return m_indicatorTextColor; }

  void setFrom(qreal v);
  void setTo(qreal v);
  void setValue(qreal v);
  void setStepSize(qreal v);
  void setOrientation(Orientation v);
  void setSizeStyle(SizeStyle v);
  void setShowStops(bool v);
  void setShowValueIndicator(bool v);
  void setDecimals(int v);
  void setOuterRadius(qreal v);

  void setTrackHeight(qreal v);
  void setHandleWidth(qreal v);
  void setHandleHeight(qreal v);
  void setHandleGap(qreal v);
  void setInnerRadius(qreal v);
  void setStopSize(qreal v);
  void setBorderWidth(qreal v);

  void setIconSource(const QUrl &v);
  void setIconText(const QString &v);
  void setIconFont(const QFont &v);
  void setIconColor(const QColor &v);
  void setIconSize(qreal v);

  void setColor(const QColor &v);
  void setTrackColor(const QColor &v);
  void setHandleColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setIndicatorColor(const QColor &v);
  void setIndicatorTextColor(const QColor &v);

  void paint(QPainter *p) override;

signals:
  void fromChanged();
  void toChanged();
  void valueChanged();
  void stepSizeChanged();
  void orientationChanged();
  void sizeStyleChanged();
  void showStopsChanged();
  void showValueIndicatorChanged();
  void decimalsChanged();
  void pressedChanged();
  void trackHeightChanged();
  void handleWidthChanged();
  void handleHeightChanged();
  void handleGapChanged();
  void innerRadiusChanged();
  void stopSizeChanged();
  void borderWidthChanged();
  void iconSourceChanged();
  void iconTextChanged();
  void iconFontChanged();
  void iconColorChanged();
  void iconSizeChanged();
  void colorChanged();
  void trackColorChanged();
  void handleColorChanged();
  void borderColorChanged();
  void indicatorColorChanged();
  void indicatorTextColorChanged();
  void moved();
  void outerRadiusChanged();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void keyPressEvent(QKeyEvent *e) override;

private:
  template <typename T, typename Sig>
  bool assign(T &field, const T &v, Sig sig) {
    if (field == v)
      return false;
    field = v;
    (this->*sig)();
    update();
    return true;
  }

  void setOverride(qreal &field, qreal v, void (MaterialSlider::*sig)());

  bool horizontal() const { return m_orientation == Horizontal; }
  qreal indicatorSpace() const;
  qreal length() const;
  qreal crossCenter() const;
  int resolvedDecimals() const;
  QString valueText() const;
  qreal snap(qreal v) const;
  bool commit(qreal v);
  void reclamp();
  void updateFromPos(const QPointF &pt);
  void updateImplicit();
  void setPressed(bool v);
  void buildIcon(qreal dpr);

  qreal m_from = 0.0;
  qreal m_to = 1.0;
  qreal m_value = 0.0;
  qreal m_step = 0.0;
  Orientation m_orientation = Horizontal;
  SizeStyle m_sizeStyle = ExtraSmall;
  bool m_showStops = true;
  bool m_showIndicator = true;
  int m_decimals = -1;
  bool m_pressed = false;

  qreal m_trackH = -1.0;
  qreal m_handleW = -1.0;
  qreal m_handleH = -1.0;
  qreal m_handleGap = -1.0;
  qreal m_innerRadius = -1.0;
  qreal m_stopSize = -1.0;
  qreal m_outerRadius = -1.0;
  qreal m_borderWidth = 0.0;

  QUrl m_iconSource;
  QString m_iconText;
  QFont m_iconFont;
  QColor m_iconColor = QColor("#FFFFFF");
  qreal m_iconSize = 24.0;
  QImage m_icon;
  int m_iconPx = 0;
  bool m_iconDirty = true;

  QColor m_color = QColor("#6750A4");
  QColor m_trackColor = QColor("#E8DEF8");
  QColor m_handleColor = QColor("#6750A4");
  QColor m_borderColor = Qt::transparent;
  QColor m_indicatorColor = QColor("#322F35");
  QColor m_indicatorTextColor = QColor("#F5EFF7");

  qreal m_press = 0.0;
  QVariantAnimation m_pressAnim;
};
