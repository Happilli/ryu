#pragma once
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtQmlIntegration/qqmlintegration.h>

class QAbstractAnimation;

class MaterialList : public QQuickPaintedItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(int count READ count NOTIFY modelChanged)
  Q_PROPERTY(
      Variant variant READ variant WRITE setVariant NOTIFY variantChanged)
  Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE
                 setSelectionMode NOTIFY selectionModeChanged)
  Q_PROPERTY(QList<int> checkedIndices READ checkedIndices WRITE
                 setCheckedIndices NOTIFY checkedIndicesChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY
                 checkedIndicesChanged)

  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(
      QFont iconFont READ iconFont WRITE setIconFont NOTIFY iconFontChanged)

  Q_PROPERTY(QColor itemColor READ itemColor WRITE setItemColor NOTIFY
                 itemColorChanged)
  Q_PROPERTY(QColor selectedColor READ selectedColor WRITE setSelectedColor
                 NOTIFY selectedColorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor selectedTextColor READ selectedTextColor WRITE
                 setSelectedTextColor NOTIFY selectedTextColorChanged)
  Q_PROPERTY(QColor supportingColor READ supportingColor WRITE
                 setSupportingColor NOTIFY supportingColorChanged)
  Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor NOTIFY
                 iconColorChanged)
  Q_PROPERTY(QColor avatarColor READ avatarColor WRITE setAvatarColor NOTIFY
                 avatarColorChanged)
  Q_PROPERTY(QColor avatarTextColor READ avatarTextColor WRITE
                 setAvatarTextColor NOTIFY avatarTextColorChanged)
  Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor NOTIFY
                 dividerColorChanged)

  Q_PROPERTY(qreal springStiffness READ springStiffness WRITE setSpringStiffness
                 NOTIFY springStiffnessChanged)
  Q_PROPERTY(qreal springDamping READ springDamping WRITE setSpringDamping
                 NOTIFY springDampingChanged)

public:
  enum Variant { Standard, Segmented };
  Q_ENUM(Variant)
  enum SelectionMode { NoSelection, Single, Multiple };
  Q_ENUM(SelectionMode)

  explicit MaterialList(QQuickItem *parent = nullptr);

  QVariantList model() const { return m_raw; }
  int count() const { return m_items.size(); }
  Variant variant() const { return m_variant; }
  SelectionMode selectionMode() const { return m_mode; }
  QList<int> checkedIndices() const { return m_checked; }
  int currentIndex() const {
    return m_checked.isEmpty() ? -1 : m_checked.first();
  }
  QFont font() const { return m_font; }
  QFont iconFont() const { return m_iconFont; }
  QColor itemColor() const { return m_itemColor; }
  QColor selectedColor() const { return m_selectedColor; }
  QColor textColor() const { return m_textColor; }
  QColor selectedTextColor() const { return m_selectedTextColor; }
  QColor supportingColor() const { return m_supportingColor; }
  QColor iconColor() const { return m_iconColor; }
  QColor avatarColor() const { return m_avatarColor; }
  QColor avatarTextColor() const { return m_avatarTextColor; }
  QColor dividerColor() const { return m_dividerColor; }
  qreal springStiffness() const { return m_stiffness; }
  qreal springDamping() const { return m_damping; }

  void setModel(const QVariantList &v);
  void setVariant(Variant v);
  void setSelectionMode(SelectionMode v);
  void setCheckedIndices(const QList<int> &v);
  void setCurrentIndex(int i);
  void setFont(const QFont &v);
  void setIconFont(const QFont &v);
  void setItemColor(const QColor &v);
  void setSelectedColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setSelectedTextColor(const QColor &v);
  void setSupportingColor(const QColor &v);
  void setIconColor(const QColor &v);
  void setAvatarColor(const QColor &v);
  void setAvatarTextColor(const QColor &v);
  void setDividerColor(const QColor &v);
  void setSpringStiffness(qreal v);
  void setSpringDamping(qreal v);

  Q_INVOKABLE bool isChecked(int i) const { return m_checked.contains(i); }
  Q_INVOKABLE void setChecked(int i, bool on);

  void paint(QPainter *p) override;

signals:
  void modelChanged();
  void variantChanged();
  void selectionModeChanged();
  void checkedIndicesChanged();
  void fontChanged();
  void iconFontChanged();
  void itemColorChanged();
  void selectedColorChanged();
  void textColorChanged();
  void selectedTextColorChanged();
  void supportingColorChanged();
  void iconColorChanged();
  void avatarColorChanged();
  void avatarTextColorChanged();
  void dividerColorChanged();
  void springStiffnessChanged();
  void springDampingChanged();
  void clicked(int index, const QVariant &item);

protected:
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusInEvent(QFocusEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  struct Entry {
    QString overline;
    QString headline;
    QString supporting;
    QString trailingText;
    QString leadingIcon;
    QString trailingIcon;
    QString avatarText;
    QImage image;
    bool divider = false;
    bool enabled = true;

    int lines() const {
      return int(!overline.isEmpty()) + int(!headline.isEmpty()) +
             int(!supporting.isEmpty());
    }
  };

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

  QFont textFont(qreal px, int weight) const;
  QColor resolvedItemColor() const;
  qreal rowHeight(const Entry &e) const;
  qreal itemGap() const;
  qreal naturalHeight() const;
  void layoutItems();
  void structureChanged();
  bool selectable(int i) const;
  int rowAt(const QPointF &pt) const;
  int step(int from, int dir) const;
  void activate(int i);
  void ensureDriver();
  void tick(qreal dt);

  QVariantList m_raw;
  QVector<Entry> m_items;

  Variant m_variant = Standard;
  SelectionMode m_mode = NoSelection;
  QList<int> m_checked;

  QFont m_font;
  QFont m_iconFont;

  QColor m_itemColor;
  QColor m_selectedColor = QColor("#E8DEF8");
  QColor m_textColor = QColor("#1D1B20");
  QColor m_selectedTextColor = QColor("#1D192B");
  QColor m_supportingColor = QColor("#49454F");
  QColor m_iconColor;
  QColor m_avatarColor = QColor("#EADDFF");
  QColor m_avatarTextColor = QColor("#21005D");
  QColor m_dividerColor = QColor("#CAC4D0");

  qreal m_stiffness = 520.0;
  qreal m_damping = 0.6;

  QVector<QRectF> m_rects;
  bool m_layoutDirty = true;

  int m_pressIndex = -1;
  int m_pressShown = -1;
  int m_hoverIndex = -1;
  int m_focusIndex = -1;
  bool m_keyFocus = false;

  Spring m_press;
  QAbstractAnimation *m_driver = nullptr;
};
