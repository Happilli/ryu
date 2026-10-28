#pragma once
#include "../Core/ryuitem.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QString>

class MaterialRadioButton : public MaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(bool checked READ checked WRITE setChecked NOTIFY checkedChanged)
  Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
  Q_PROPERTY(bool error READ error WRITE setError NOTIFY errorChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
  Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)

  Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor uncheckedColor READ uncheckedColor WRITE setUncheckedColor
                 NOTIFY uncheckedColorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor errorColor READ errorColor WRITE setErrorColor NOTIFY
                 errorColorChanged)

public:
  explicit MaterialRadioButton(QQuickItem *parent = nullptr);

  bool checked() const { return m_checked; }
  QString group() const { return m_group; }
  bool error() const { return m_error; }
  bool pressed() const { return m_pressed; }
  bool hovered() const { return m_hovered; }
  QString text() const { return m_text; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  QColor color() const { return m_color; }
  QColor uncheckedColor() const { return m_uncheckedColor; }
  QColor textColor() const { return m_textColor; }
  QColor errorColor() const { return m_errorColor; }

  void setChecked(bool v);
  void setGroup(const QString &v);
  void setError(bool v);
  void setText(const QString &v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setColor(const QColor &v);
  void setUncheckedColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setErrorColor(const QColor &v);

  void paint(QPainter *p) override;

signals:
  void checkedChanged();
  void groupChanged();
  void errorChanged();
  void pressedChanged();
  void hoveredChanged();
  void textChanged();
  void fontChanged();
  void fontWeightChanged();
  void colorChanged();
  void uncheckedColorChanged();
  void textColorChanged();
  void errorColorChanged();
  void toggled();
  void clicked();

protected:
  bool advance(qreal dt) override;
  void componentComplete() override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;

private:
  bool overCircle(const QPointF &pt) const;
  QFont labelFont() const;
  qreal textWidth() const;
  qreal targetSelection() const;
  void updateImplicit();
  void retarget(bool immediate = false);
  void setPressed(bool v);
  void setHovered(bool v);
  bool applyChecked(bool v, bool user);
  void uncheckPeers();
  QList<MaterialRadioButton *> peers() const;
  void moveFocus(int dir);
  void activate();

  bool m_checked = false;
  QString m_group;
  bool m_error = false;
  bool m_pressed = false;
  bool m_hovered = false;
  bool m_keyFocus = false;

  QString m_text;
  QFont m_font;
  int m_fontWeight = 400;

  QColor m_color = QColor("#6750A4");
  QColor m_uncheckedColor = QColor("#49454F");
  QColor m_textColor = QColor("#1D1B20");
  QColor m_errorColor = QColor("#B3261E");

  Spring m_sel;
  Spring m_press;
};
