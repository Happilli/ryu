#pragma once
#include <QColor>
#include <QFocusEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickPaintedItem>
#include <QtQmlIntegration/qqmlintegration.h>

class QAbstractAnimation;

class MaterialSwitch : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)
  Q_PROPERTY(
      bool showIcons READ showIcons WRITE setShowIcons NOTIFY showIconsChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
  Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)

  Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor NOTIFY
                 trackColorChanged)
  Q_PROPERTY(QColor checkedTrackColor READ checkedTrackColor WRITE
                 setCheckedTrackColor NOTIFY checkedTrackColorChanged)
  Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor NOTIFY
                 handleColorChanged)
  Q_PROPERTY(QColor checkedHandleColor READ checkedHandleColor WRITE
                 setCheckedHandleColor NOTIFY checkedHandleColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor checkedIconColor READ checkedIconColor WRITE
                 setCheckedIconColor NOTIFY checkedIconColorChanged)

  Q_PROPERTY(qreal springStiffness READ springStiffness WRITE setSpringStiffness
                 NOTIFY springStiffnessChanged)
  Q_PROPERTY(qreal springDamping READ springDamping WRITE setSpringDamping
                 NOTIFY springDampingChanged)

public:
  explicit MaterialSwitch(QQuickItem *parent = nullptr);

  bool checked() const { return m_checked; }
  bool showIcons() const { return m_showIcons; }
  bool pressed() const { return m_pressed; }
  bool hovered() const { return m_hovered; }
  QColor trackColor() const { return m_trackColor; }
  QColor checkedTrackColor() const { return m_checkedTrackColor; }
  QColor handleColor() const { return m_handleColor; }
  QColor checkedHandleColor() const { return m_checkedHandleColor; }
  QColor borderColor() const { return m_borderColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor checkedIconColor() const { return m_checkedIconColor; }
  qreal springStiffness() const { return m_stiffness; }
  qreal springDamping() const { return m_damping; }

  void setChecked(bool v);
  void setShowIcons(bool v);
  void setTrackColor(const QColor &v);
  void setCheckedTrackColor(const QColor &v);
  void setHandleColor(const QColor &v);
  void setCheckedHandleColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setCheckedIconColor(const QColor &v);
  void setSpringStiffness(qreal v);
  void setSpringDamping(qreal v);

  void paint(QPainter *p) override;

signals:
  void checkedChanged();
  void showIconsChanged();
  void pressedChanged();
  void hoveredChanged();
  void trackColorChanged();
  void checkedTrackColorChanged();
  void handleColorChanged();
  void checkedHandleColorChanged();
  void borderColorChanged();
  void iconColorChanged();
  void checkedIconColorChanged();
  void springStiffnessChanged();
  void springDampingChanged();
  void toggled(); // user changed the state
  void clicked();

protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
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

  void setPressed(bool v);
  void setHovered(bool v);
  void retarget(bool immediate = false);
  void ensureDriver();
  void tick(qreal dt);
  void userSet(bool on);

  bool m_checked = false;
  bool m_showIcons = false;
  bool m_pressed = false;
  bool m_hovered = false;
  bool m_keyFocus = false;
  bool m_dragging = false;
  qreal m_pressX = 0.0;

  QColor m_trackColor = QColor("#E6E0E9");
  QColor m_checkedTrackColor = QColor("#6750A4");
  QColor m_handleColor = QColor("#79747E");
  QColor m_checkedHandleColor = QColor("#FFFFFF");
  QColor m_borderColor = QColor("#79747E");
  QColor m_iconColor = QColor("#E6E0E9");
  QColor m_checkedIconColor = QColor("#6750A4");

  qreal m_stiffness = 520.0;
  qreal m_damping = 0.55;

  Spring m_pos;
  Spring m_press;
  QAbstractAnimation *m_driver = nullptr;
};
