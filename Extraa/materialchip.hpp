#pragma once
#include "../Core/paintutil.hpp"
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QString>
#include <QUrl>

class MaterialChip : public SgMaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
  Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(
      bool elevated READ elevated WRITE setElevated NOTIFY elevatedChanged)
  Q_PROPERTY(bool rounded READ rounded WRITE setRounded NOTIFY roundedChanged)
  Q_PROPERTY(
      bool selected READ selected WRITE setSelected NOTIFY selectedChanged)
  Q_PROPERTY(
      bool checkable READ checkable WRITE setCheckable NOTIFY checkableChanged)
  Q_PROPERTY(
      bool closable READ closable WRITE setClosable NOTIFY closableChanged)
  Q_PROPERTY(
      bool showCheck READ showCheck WRITE setShowCheck NOTIFY showCheckChanged)
  Q_PROPERTY(bool pressed READ pressed NOTIFY pressedChanged)
  Q_PROPERTY(bool hovered READ hovered NOTIFY hoveredChanged)

  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(
      QString iconText READ iconText WRITE setIconText NOTIFY iconTextChanged)
  Q_PROPERTY(QUrl iconSource READ iconSource WRITE setIconSource NOTIFY
                 iconSourceChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)
  Q_PROPERTY(QString trailingIconText READ trailingIconText WRITE
                 setTrailingIconText NOTIFY trailingIconTextChanged)
  Q_PROPERTY(QUrl trailingIconSource READ trailingIconSource WRITE
                 setTrailingIconSource NOTIFY trailingIconSourceChanged)
  Q_PROPERTY(QString avatarText READ avatarText WRITE setAvatarText NOTIFY
                 avatarTextChanged)
  Q_PROPERTY(QUrl avatarSource READ avatarSource WRITE setAvatarSource NOTIFY
                 avatarSourceChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor
                 NOTIFY selectedColorChanged)
  Q_PROPERTY(QColor contentColor READ contentColor WRITE setContentColor NOTIFY
                 contentColorChanged)
  Q_PROPERTY(QColor selectedContentColor READ selectedContentColor WRITE
                 setSelectedContentColor NOTIFY selectedContentColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)
  Q_PROPERTY(QColor avatarColor READ avatarColor WRITE setAvatarColor NOTIFY
                 avatarColorChanged)
  Q_PROPERTY(QColor avatarTextColor READ avatarTextColor WRITE
                 setAvatarTextColor NOTIFY avatarTextColorChanged)
  Q_PROPERTY(bool autoToggle READ autoToggle WRITE setAutoToggle NOTIFY
                 autoToggleChanged)

public:
  enum Type { Assist, Filter, Input, Suggestion };
  Q_ENUM(Type)

  explicit MaterialChip(QQuickItem *parent = nullptr);

  QString label() const { return m_label; }
  Type type() const { return m_type; }
  bool elevated() const { return m_elevated; }
  bool rounded() const { return m_rounded; }
  bool selected() const { return m_selected; }
  bool checkable() const { return m_checkable; }
  bool closable() const { return m_closable; }
  bool showCheck() const { return m_showCheck; }
  bool pressed() const { return m_pressZone != None; }
  bool hovered() const { return m_hoverZone != None; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  QString iconText() const { return m_iconText; }
  QUrl iconSource() const { return m_iconSource; }
  QFont iconFont() const { return m_iconFont; }
  QString trailingIconText() const { return m_trailingIconText; }
  QUrl trailingIconSource() const { return m_trailingIconSource; }
  QString avatarText() const { return m_avatarText; }
  QUrl avatarSource() const { return m_avatarSource; }
  QColor color() const { return m_color; }
  QColor selectedColor() const { return m_selectedColor; }
  QColor contentColor() const { return m_contentColor; }
  QColor selectedContentColor() const { return m_selectedContentColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor borderColor() const { return m_borderColor; }
  qreal borderWidth() const { return m_borderWidth; }
  QColor avatarColor() const { return m_avatarColor; }
  QColor avatarTextColor() const { return m_avatarTextColor; }
  bool autoToggle() const { return m_autoToggle; }

  void setAutoToggle(bool v);
  void setLabel(const QString &v);
  void setType(Type v);
  void setElevated(bool v);
  void setRounded(bool v);
  void setSelected(bool v);
  void setCheckable(bool v);
  void setClosable(bool v);
  void setShowCheck(bool v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setIconText(const QString &v);
  void setIconSource(const QUrl &v);
  void setIconFont(const QFont &v);
  void setTrailingIconText(const QString &v);
  void setTrailingIconSource(const QUrl &v);
  void setAvatarText(const QString &v);
  void setAvatarSource(const QUrl &v);
  void setColor(const QColor &v);
  void setSelectedColor(const QColor &v);
  void setContentColor(const QColor &v);
  void setSelectedContentColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setBorderWidth(qreal v);
  void setAvatarColor(const QColor &v);
  void setAvatarTextColor(const QColor &v);

  Q_INVOKABLE void click();

signals:
  void labelChanged();
  void typeChanged();
  void elevatedChanged();
  void roundedChanged();
  void selectedChanged();
  void checkableChanged();
  void closableChanged();
  void showCheckChanged();
  void pressedChanged();
  void hoveredChanged();
  void autoToggleChanged();
  void fontChanged();
  void fontWeightChanged();
  void iconTextChanged();
  void iconSourceChanged();
  void iconFontChanged();
  void trailingIconTextChanged();
  void trailingIconSourceChanged();
  void avatarTextChanged();
  void avatarSourceChanged();
  void colorChanged();
  void selectedColorChanged();
  void contentColorChanged();
  void selectedContentColorChanged();
  void iconColorChanged();
  void borderColorChanged();
  void borderWidthChanged();
  void avatarColorChanged();
  void avatarTextColorChanged();
  void clicked();
  void toggled();
  void trailingClicked();
  void closeClicked();

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
  void keyReleaseEvent(QKeyEvent *e) override;
  void focusInEvent(QFocusEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  enum Zone { None = -1, Body = 0, Trail = 1 };

  QFont labelFont() const;
  qreal textWidth() const;
  bool hasIcon() const;
  bool hasAvatar() const;
  bool customTrailing() const;
  bool trailingShown() const;
  bool checkEligible() const;
  bool isToggle() const;
  qreal leadPad(qreal chk) const;
  qreal slotWidth(qreal chk) const;
  qreal gapWidth(qreal chk) const;
  qreal trailWidth() const;
  QColor containerBase() const;
  QColor containerSelected() const;
  QColor contentBase() const;
  QColor contentSelected() const;
  QColor leadIconBase() const;
  void updateImplicit();
  void retarget(bool immediate = false);
  int hit(const QPointF &pt) const;
  void setPressZone(int z);
  void setHoverZone(int z);
  void activate(int zone);
  void loadAvatar();
  void iconLayer(Ryu::Mesh &m, int slot, Ryu::IconCache &cache, QString &path,
                 const QUrl &src, const QString &glyph, const QColor &col,
                 const QPointF &c, qreal size);

  QString m_label;
  Type m_type = Assist;
  bool m_elevated = false;
  bool m_rounded = false;
  bool m_selected = false;
  bool m_checkable = false;
  bool m_closable = false;
  bool m_showCheck = true;
  bool m_autoToggle = true;

  QFont m_font;
  int m_fontWeight = 500;
  QString m_iconText;
  QUrl m_iconSource;
  QFont m_iconFont;
  QString m_trailingIconText;
  QUrl m_trailingIconSource;
  QString m_avatarText;
  QUrl m_avatarSource;

  QColor m_color;
  QColor m_selectedColor;
  QColor m_contentColor;
  QColor m_selectedContentColor;
  QColor m_iconColor;
  QColor m_borderColor = QColor("#79747E");
  qreal m_borderWidth = 1.0;
  QColor m_avatarColor = QColor("#EADDFF");
  QColor m_avatarTextColor = QColor("#21005D");

  int m_pressZone = None;
  int m_hoverZone = None;
  int m_focusZone = Body;
  bool m_keyFocus = false;
  bool m_keyDown = false;

  qreal m_textW = 0.0;

  Ryu::IconCache m_leadCache;
  Ryu::IconCache m_trailCache;
  QString m_leadPath;
  QString m_trailPath;

  QImage m_avatarImg;
  bool m_avatarDirty = true;
  int m_avatarGen = 0;

  Spring m_round;
  Spring m_press;
  Spring m_sel;
  Spring m_check;
  Spring m_hov;
  Spring m_tlayer;
  Spring m_tpress;
};
