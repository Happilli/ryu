#pragma once
#include "../Core/paintutil.hpp"
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QFont>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVector>
#include <QWheelEvent>

class QSGGeometryNode;
class QSGOpacityNode;
class CarouselImageNode;

class MaterialCarousel : public SgMaterialItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QVariantList model READ model WRITE setModel NOTIFY modelChanged)
  Q_PROPERTY(int count READ count NOTIFY modelChanged)
  Q_PROPERTY(Type type READ type WRITE setType NOTIFY typeChanged)
  Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation
                 NOTIFY orientationChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY
                 currentIndexChanged)
  Q_PROPERTY(
      qreal scrollPosition READ scrollPosition NOTIFY scrollPositionChanged)
  Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged)
  Q_PROPERTY(bool snap READ snap WRITE setSnap NOTIFY snapChanged)

  Q_PROPERTY(
      qreal itemSize READ itemSize WRITE setItemSize NOTIFY itemSizeChanged)
  Q_PROPERTY(qreal minSmallItemSize READ minSmallItemSize WRITE
                 setMinSmallItemSize NOTIFY minSmallItemSizeChanged)
  Q_PROPERTY(qreal maxSmallItemSize READ maxSmallItemSize WRITE
                 setMaxSmallItemSize NOTIFY maxSmallItemSizeChanged)
  Q_PROPERTY(qreal itemSpacing READ itemSpacing WRITE setItemSpacing NOTIFY
                 itemSpacingChanged)
  Q_PROPERTY(qreal itemRadius READ itemRadius WRITE setItemRadius NOTIFY
                 itemRadiusChanged)

  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)

  Q_PROPERTY(QColor itemColor READ itemColor WRITE setItemColor NOTIFY
                 itemColorChanged)
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY
                 textColorChanged)
  Q_PROPERTY(QColor imageTextColor READ imageTextColor WRITE setImageTextColor
                 NOTIFY imageTextColorChanged)
  Q_PROPERTY(QColor scrimColor READ scrimColor WRITE setScrimColor NOTIFY
                 scrimColorChanged)
  Q_PROPERTY(QColor stateLayerColor READ stateLayerColor WRITE
                 setStateLayerColor NOTIFY stateLayerColorChanged)
  Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY
                 borderColorChanged)
  Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY
                 borderWidthChanged)

public:
  enum Type { MultiBrowse, Uncontained, Hero, CenteredHero, FullScreen };
  Q_ENUM(Type)
  enum Orientation { Horizontal, Vertical };
  Q_ENUM(Orientation)

  explicit MaterialCarousel(QQuickItem *parent = nullptr);

  QVariantList model() const { return m_raw; }
  int count() const { return m_items.size(); }
  Type type() const { return m_type; }
  Orientation orientation() const { return m_orientation; }
  int currentIndex() const { return m_current; }
  qreal scrollPosition() const { return m_pos.x; }
  bool dragging() const { return m_dragging; }
  bool snap() const { return m_snap; }
  qreal itemSize() const { return m_itemSize; }
  qreal minSmallItemSize() const { return m_minSmall; }
  qreal maxSmallItemSize() const { return m_maxSmall; }
  qreal itemSpacing() const { return m_spacing; }
  qreal itemRadius() const { return m_radius; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  QColor itemColor() const { return m_itemColor; }
  QColor textColor() const { return m_textColor; }
  QColor imageTextColor() const { return m_imageTextColor; }
  QColor scrimColor() const { return m_scrimColor; }
  QColor stateLayerColor() const { return m_stateLayerColor; }
  QColor borderColor() const { return m_borderColor; }
  qreal borderWidth() const { return m_borderWidth; }

  void setModel(const QVariantList &v);
  void setType(Type v);
  void setOrientation(Orientation v);
  void setCurrentIndex(int i);
  void setSnap(bool v);
  void setItemSize(qreal v);
  void setMinSmallItemSize(qreal v);
  void setMaxSmallItemSize(qreal v);
  void setItemSpacing(qreal v);
  void setItemRadius(qreal v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setItemColor(const QColor &v);
  void setTextColor(const QColor &v);
  void setImageTextColor(const QColor &v);
  void setScrimColor(const QColor &v);
  void setStateLayerColor(const QColor &v);
  void setBorderColor(const QColor &v);
  void setBorderWidth(qreal v);

  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void scrollTo(int index, bool animate = true);

signals:
  void modelChanged();
  void typeChanged();
  void orientationChanged();
  void currentIndexChanged();
  void scrollPositionChanged();
  void draggingChanged();
  void snapChanged();
  void itemSizeChanged();
  void minSmallItemSizeChanged();
  void maxSmallItemSizeChanged();
  void itemSpacingChanged();
  void itemRadiusChanged();
  void fontChanged();
  void fontWeightChanged();
  void itemColorChanged();
  void textColorChanged();
  void imageTextColorChanged();
  void scrimColorChanged();
  void stateLayerColorChanged();
  void borderColorChanged();
  void borderWidthChanged();
  void clicked(int index, const QVariant &item);

protected:
  void build(Ryu::Mesh &m) override;
  bool advance(qreal dt) override;
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *d) override;
  void componentComplete() override;
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseUngrabEvent() override;
  void hoverEnterEvent(QHoverEvent *e) override;
  void hoverMoveEvent(QHoverEvent *e) override;
  void hoverLeaveEvent(QHoverEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  void focusOutEvent(QFocusEvent *e) override;

private:
  struct Entry {
    QString headline;
    QString supporting;
    QString path;
    QImage image;
    QSize want;
    int rev = 0;
    bool loading = false;
    QColor color;
    bool enabled = true;
  };

  struct Slot {
    int index;
    qreal pos;
    qreal size;
  };

  struct Profile {
    QVector<qreal> sz;
    QVector<qreal> x;
  };

  bool horizontal() const { return m_orientation == Horizontal; }
  qreal length() const { return horizontal() ? width() : height(); }
  QRectF rectFor(const Slot &s) const;
  qreal radiusFor(const QRectF &r) const;
  void updateImplicit();
  void structureChanged();
  void layoutItems();
  void evalProfile(const Profile &p, qreal k, qreal &size, qreal &pos) const;
  void itemGeometry(int i, qreal f, qreal &size, qreal &pos) const;
  QVector<Slot> visibleSlots();
  int itemAt(const QPointF &pt);
  qreal snapF(qreal x) const;
  qreal rubber(qreal raw) const;
  void goTo(qreal target, bool immediate = false);
  void updateCurrent();
  void activate(int i);
  void fling();
  void endDrag();
  void settle();
  void syncImages();
  void syncScrim();
  void requestImages();

  QVariantList m_raw;
  QVector<Entry> m_items;
  int m_gen = 0;
  bool m_resetNodes = false;

  Type m_type = MultiBrowse;
  Orientation m_orientation = Horizontal;
  bool m_snap = true;
  qreal m_itemSize = 186.0;
  qreal m_minSmall = 40.0;
  qreal m_maxSmall = 56.0;
  qreal m_spacing = 8.0;
  qreal m_radius = 28.0;

  QFont m_font;
  int m_fontWeight = 500;

  QColor m_itemColor = QColor("#E8DEF8");
  QColor m_textColor = QColor("#1D192B");
  QColor m_imageTextColor = QColor("#FFFFFF");
  QColor m_scrimColor = QColor(0, 0, 0, 153);
  QColor m_stateLayerColor = QColor("#1D1B20");
  QColor m_borderColor = QColor("#79747E");
  qreal m_borderWidth = 0.0;

  bool m_layoutDirty = true;
  Profile m_start;
  Profile m_mid;
  Profile m_end;
  bool m_hasStart = false;
  bool m_hasEnd = false;
  int m_pivot = 0;
  int m_largeCount = 1;
  qreal m_large = 0.0;
  qreal m_pitch = 1.0;
  qreal m_maxF = 0.0;
  qreal m_cross = 0.0;

  Spring m_pos;
  int m_current = 0;
  bool m_pressed = false;
  bool m_dragging = false;
  bool m_moved = false;
  bool m_keyFocus = false;
  int m_pressIndex = -1;
  int m_hoverIndex = -1;
  QPointF m_pressPt;
  qreal m_dragStartF = 0.0;
  qreal m_vel = 0.0;
  qreal m_lastAlong = 0.0;
  qint64 m_lastMs = 0;
  QElapsedTimer m_clock;
  QTimer *m_settle = nullptr;

  QVector<Slot> m_vis;
  QVector<CarouselImageNode *> m_imgNodes;
  QSGOpacityNode *m_imgGroup = nullptr;
  QSGGeometryNode *m_scrim = nullptr;
};
