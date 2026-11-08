#pragma once
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlComponent>
#include <QQuickItem>
#include <QRectF>
#include <QString>
#include <QVariant>
#include <QVector>

class MaterialList : public SgMaterialItem {
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

  Q_PROPERTY(QQmlComponent *contentDelegate READ contentDelegate WRITE
                 setContentDelegate NOTIFY contentDelegateChanged)
  Q_PROPERTY(ExpandTrigger expandTrigger READ expandTrigger WRITE
                 setExpandTrigger NOTIFY expandTriggerChanged)
  Q_PROPERTY(bool singleExpand READ singleExpand WRITE setSingleExpand NOTIFY
                 singleExpandChanged)
  Q_PROPERTY(QList<int> expandedIndices READ expandedIndices WRITE
                 setExpandedIndices NOTIFY expandedIndicesChanged)
  Q_PROPERTY(qreal contentPadding READ contentPadding WRITE setContentPadding
                 NOTIFY contentPaddingChanged)
  Q_PROPERTY(
      bool animated READ animated WRITE setAnimated NOTIFY animatedChanged)
  Q_PROPERTY(bool showChevron READ showChevron WRITE setShowChevron NOTIFY
                 showChevronChanged)
  Q_PROPERTY(QColor chevronColor READ chevronColor WRITE setChevronColor NOTIFY
                 chevronColorChanged)

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

public:
  enum Variant { Standard, Segmented };
  Q_ENUM(Variant)
  enum SelectionMode { NoSelection, Single, Multiple };
  Q_ENUM(SelectionMode)
  enum ExpandTrigger { Auto, Header, Chevron };
  Q_ENUM(ExpandTrigger)

  explicit MaterialList(QQuickItem *parent = nullptr);

  QVariantList model() const { return m_raw; }
  int count() const { return m_items.size(); }
  Variant variant() const { return m_variant; }
  SelectionMode selectionMode() const { return m_mode; }
  QList<int> checkedIndices() const { return m_checked; }
  int currentIndex() const {
    return m_checked.isEmpty() ? -1 : m_checked.first();
  }

  QQmlComponent *contentDelegate() const { return m_delegate.data(); }
  ExpandTrigger expandTrigger() const { return m_trigger; }
  bool singleExpand() const { return m_singleExpand; }
  QList<int> expandedIndices() const;
  qreal contentPadding() const { return m_contentPad; }
  bool animated() const { return m_animated; }
  bool showChevron() const { return m_showChevron; }
  QColor chevronColor() const { return m_chevronColor; }

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

  void setModel(const QVariantList &v);
  void setVariant(Variant v);
  void setSelectionMode(SelectionMode v);
  void setCheckedIndices(const QList<int> &v);
  void setCurrentIndex(int i);

  void setContentDelegate(QQmlComponent *c);
  void setExpandTrigger(ExpandTrigger v);
  void setSingleExpand(bool v);
  void setExpandedIndices(const QList<int> &v);
  void setContentPadding(qreal v);
  void setAnimated(bool v);
  void setShowChevron(bool v);
  void setChevronColor(const QColor &v);

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

  Q_INVOKABLE bool isChecked(int i) const { return m_checked.contains(i); }
  Q_INVOKABLE void setChecked(int i, bool on);
  Q_INVOKABLE bool isExpanded(int i) const;
  Q_INVOKABLE void setExpanded(int i, bool on);
  Q_INVOKABLE void toggleExpanded(int i);
  Q_INVOKABLE void expandAll();
  Q_INVOKABLE void collapseAll();
  Q_INVOKABLE QQuickItem *contentItem(int i) const;

signals:
  void modelChanged();
  void variantChanged();
  void selectionModeChanged();
  void checkedIndicesChanged();
  void contentDelegateChanged();
  void expandTriggerChanged();
  void singleExpandChanged();
  void expandedIndicesChanged();
  void contentPaddingChanged();
  void animatedChanged();
  void showChevronChanged();
  void chevronColorChanged();
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
  void clicked(int index, const QVariant &item);
  void expandedChanged(int index, bool expanded);

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
    QPointer<QQmlComponent> delegate;
    bool divider = false;
    bool enabled = true;
    bool expandable = false;
    bool expanded = false;

    int lines() const {
      return int(!overline.isEmpty()) + int(!headline.isEmpty()) +
             int(!supporting.isEmpty());
    }
  };

  struct Row {
    Spring open;
    bool expanded = false;
    QQuickItem *clip = nullptr;
    QQuickItem *content = nullptr;
  };

  QColor resolvedItemColor() const;
  qreal headerHeight(const Entry &e) const;
  qreal rowHeight(int i) const;
  qreal itemGap() const;
  qreal naturalHeight() const;
  qreal contentNatural(int i) const;
  QRectF headerRect(int i) const;
  void layoutItems();
  void structureChanged();
  void relayout();
  void positionContent();
  bool selectable(int i) const;
  bool expandable(int i) const;
  bool canToggle(int i) const;
  ExpandTrigger effectiveTrigger() const;
  bool chevronZone(int i, const QPointF &pt) const;
  int rowAt(const QPointF &pt) const;
  int step(int from, int dir) const;
  void activate(int i);
  QQmlComponent *delegateFor(int i) const;
  void ensureContent(int i);
  void destroyContent(int i);
  void destroyAllContent();
  bool applyExpanded(int i, bool on);
  void finishExpansion();
  bool contentHasFocus() const;

  QVariantList m_raw;
  QVector<Entry> m_items;
  QVector<Row> m_rows;
  int m_gen = 0;

  Variant m_variant = Standard;
  SelectionMode m_mode = NoSelection;
  QList<int> m_checked;

  QPointer<QQmlComponent> m_delegate;
  ExpandTrigger m_trigger = Auto;
  bool m_singleExpand = false;
  qreal m_contentPad = 16.0;
  bool m_animated = true;
  bool m_showChevron = true;
  QColor m_chevronColor;

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

  QVector<QRectF> m_rects;
  bool m_layoutDirty = true;
  bool m_relaying = false;
  bool m_relayAgain = false;
  bool m_wasMoving = false;

  int m_pressIndex = -1;
  int m_pressShown = -1;
  int m_hoverIndex = -1;
  int m_focusIndex = -1;
  bool m_pressChevron = false;
  bool m_hoverChevron = false;
  bool m_keyFocus = false;

  Spring m_press;
};
