#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegion>
#include <QSize>
#include <QString>
#include <cstdint>

namespace ryu::wl {
class LayerSurface;
}

namespace WlrLayer {
Q_NAMESPACE
QML_ELEMENT
enum Enum { Background = 0, Bottom = 1, Top = 2, Overlay = 3 };
Q_ENUM_NS(Enum)
} // namespace WlrLayer

namespace WlrKeyboardFocus {
Q_NAMESPACE
QML_ELEMENT
enum Enum { None = 0, Exclusive = 1, OnDemand = 2 };
Q_ENUM_NS(Enum)
} // namespace WlrKeyboardFocus

class WlrAnchors : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(bool top READ top WRITE setTop NOTIFY changed)
  Q_PROPERTY(bool bottom READ bottom WRITE setBottom NOTIFY changed)
  Q_PROPERTY(bool left READ left WRITE setLeft NOTIFY changed)
  Q_PROPERTY(bool right READ right WRITE setRight NOTIFY changed)

public:
  explicit WlrAnchors(QObject *parent = nullptr) : QObject(parent) {}

  bool top() const { return m_top; }
  bool bottom() const { return m_bottom; }
  bool left() const { return m_left; }
  bool right() const { return m_right; }

  void setTop(bool v) {
    if (m_top != v) {
      m_top = v;
      emit changed();
    }
  }
  void setBottom(bool v) {
    if (m_bottom != v) {
      m_bottom = v;
      emit changed();
    }
  }
  void setLeft(bool v) {
    if (m_left != v) {
      m_left = v;
      emit changed();
    }
  }
  void setRight(bool v) {
    if (m_right != v) {
      m_right = v;
      emit changed();
    }
  }

  uint32_t mask() const {
    return (m_top ? 1u : 0u) | (m_bottom ? 2u : 0u) | (m_left ? 4u : 0u) |
           (m_right ? 8u : 0u);
  }

signals:
  void changed();

private:
  bool m_top = false;
  bool m_bottom = false;
  bool m_left = false;
  bool m_right = false;
};

class WlrMargins : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(int top READ top WRITE setTop NOTIFY changed)
  Q_PROPERTY(int bottom READ bottom WRITE setBottom NOTIFY changed)
  Q_PROPERTY(int left READ left WRITE setLeft NOTIFY changed)
  Q_PROPERTY(int right READ right WRITE setRight NOTIFY changed)

public:
  explicit WlrMargins(QObject *parent = nullptr) : QObject(parent) {}

  int top() const { return m_top; }
  int bottom() const { return m_bottom; }
  int left() const { return m_left; }
  int right() const { return m_right; }

  void setTop(int v) {
    if (m_top != v) {
      m_top = v;
      emit changed();
    }
  }
  void setBottom(int v) {
    if (m_bottom != v) {
      m_bottom = v;
      emit changed();
    }
  }
  void setLeft(int v) {
    if (m_left != v) {
      m_left = v;
      emit changed();
    }
  }
  void setRight(int v) {
    if (m_right != v) {
      m_right = v;
      emit changed();
    }
  }

signals:
  void changed();

private:
  int m_top = 0;
  int m_bottom = 0;
  int m_left = 0;
  int m_right = 0;
};

class Region : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("DefaultProperty", "regions")
  Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY changed)
  Q_PROPERTY(QQmlListProperty<Region> regions READ regions)

public:
  explicit Region(QObject *parent = nullptr);

  QQuickItem *item() const { return m_item.data(); }
  void setItem(QQuickItem *item);

  QQmlListProperty<Region> regions();

  QRegion build() const;

signals:
  void changed();

private:
  static void appendRegion(QQmlListProperty<Region> *list, Region *region);
  static qsizetype regionCount(QQmlListProperty<Region> *list);
  static Region *regionAt(QQmlListProperty<Region> *list, qsizetype index);
  static void clearRegions(QQmlListProperty<Region> *list);

  QPointer<QQuickItem> m_item;
  QList<Region *> m_regions;
};

class WlrLayershell : public QQuickWindow, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_CLASSINFO("DefaultProperty", "contents")
  Q_PROPERTY(QQmlListProperty<QObject> contents READ contents)
  Q_PROPERTY(QString namespace READ ns WRITE setNs NOTIFY nsChanged)
  Q_PROPERTY(QObject *screen READ screenSource WRITE setScreenSource NOTIFY
                 screenSourceChanged)
  Q_PROPERTY(WlrLayer::Enum layer READ layerValue WRITE setLayerValue NOTIFY
                 layerChanged)
  Q_PROPERTY(WlrKeyboardFocus::Enum keyboardFocus READ keyboardFocusValue WRITE
                 setKeyboardFocusValue NOTIFY keyboardFocusChanged)
  Q_PROPERTY(int exclusiveZone READ exclusiveZone WRITE setExclusiveZone NOTIFY
                 exclusiveZoneChanged)
  Q_PROPERTY(WlrAnchors *anchors READ anchors CONSTANT)
  Q_PROPERTY(WlrMargins *margins READ margins CONSTANT)
  Q_PROPERTY(
      Region *mask READ maskRegion WRITE setMaskRegion NOTIFY maskChanged)

public:
  explicit WlrLayershell(QWindow *parent = nullptr);
  ~WlrLayershell() override;

  void classBegin() override {}
  void componentComplete() override;

  QQmlListProperty<QObject> contents();

  QString ns() const { return m_ns; }
  void setNs(const QString &ns);

  QObject *screenSource() const { return m_screen.data(); }
  void setScreenSource(QObject *screen);

  WlrLayer::Enum layerValue() const { return m_layer; }
  void setLayerValue(WlrLayer::Enum layer);

  WlrKeyboardFocus::Enum keyboardFocusValue() const { return m_keyboard; }
  void setKeyboardFocusValue(WlrKeyboardFocus::Enum focus);

  int exclusiveZone() const { return m_exclusive; }
  void setExclusiveZone(int zone);

  WlrAnchors *anchors() const { return m_anchors; }
  WlrMargins *margins() const { return m_margins; }

  Region *maskRegion() const { return m_mask.data(); }
  void setMaskRegion(Region *region);

  void attachSurface(ryu::wl::LayerSurface *surface);
  void detachSurface(ryu::wl::LayerSurface *surface);
  void pushInput();

signals:
  void nsChanged();
  void screenSourceChanged();
  void layerChanged();
  void keyboardFocusChanged();
  void exclusiveZoneChanged();
  void maskChanged();

private:
  static void appendContent(QQmlListProperty<QObject> *list, QObject *object);
  static qsizetype contentCount(QQmlListProperty<QObject> *list);
  static QObject *contentAt(QQmlListProperty<QObject> *list, qsizetype index);
  static void clearContents(QQmlListProperty<QObject> *list);

  QScreen *resolveScreen() const;
  void present();
  void recreate();
  void schedulePush();

  QString m_ns = QStringLiteral("ryu");
  QPointer<QObject> m_screen;
  WlrLayer::Enum m_layer = WlrLayer::Top;
  WlrKeyboardFocus::Enum m_keyboard = WlrKeyboardFocus::None;
  int m_exclusive = 0;
  WlrAnchors *m_anchors = nullptr;
  WlrMargins *m_margins = nullptr;
  QPointer<Region> m_mask;
  QRegion m_input;
  bool m_inputValid = false;
  bool m_pushQueued = false;
  bool m_complete = false;
  ryu::wl::LayerSurface *m_surface = nullptr;
  QList<QObject *> m_contents;
};
