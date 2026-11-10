#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QString>

namespace ryu::wl {
class LockObject;
}

class WlSessionLockSurface : public QQuickWindow, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_CLASSINFO("DefaultProperty", "contents")
  Q_PROPERTY(QQmlListProperty<QObject> contents READ contents)
  Q_PROPERTY(QObject *screen READ screenSource WRITE setScreenSource NOTIFY
                 screenSourceChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
  explicit WlSessionLockSurface(QWindow *parent = nullptr);
  ~WlSessionLockSurface() override;

  void classBegin() override {}
  void componentComplete() override;

  QQmlListProperty<QObject> contents();

  QObject *screenSource() const { return m_screen.data(); }
  void setScreenSource(QObject *screen);

  bool ready() const { return m_ready; }
  void setReady();

signals:
  void screenSourceChanged();
  void readyChanged();

private:
  static void appendContent(QQmlListProperty<QObject> *list, QObject *object);
  static qsizetype contentCount(QQmlListProperty<QObject> *list);
  static QObject *contentAt(QQmlListProperty<QObject> *list, qsizetype index);
  static void clearContents(QQmlListProperty<QObject> *list);

  QScreen *resolveScreen() const;
  void present();

  QPointer<QObject> m_screen;
  bool m_ready = false;
  QList<QObject *> m_contents;
};
class WlSessionLock : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("DefaultProperty", "surface")
  Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY lockedChanged)
  Q_PROPERTY(bool secure READ secure NOTIFY secureChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QQmlComponent *surface READ surface WRITE setSurface NOTIFY
                 surfaceChanged)

public:
  explicit WlSessionLock(QObject *parent = nullptr);
  ~WlSessionLock() override;

  bool locked() const { return m_locked; }
  void setLocked(bool locked);

  bool secure() const { return m_secure; }

  QColor color() const { return m_color; }
  void setColor(const QColor &color);

  QQmlComponent *surface() const { return m_surface.data(); }
  void setSurface(QQmlComponent *surface);

  void handleLocked();
  void handleFinished();

signals:
  void lockedChanged();
  void secureChanged();
  void colorChanged();
  void surfaceChanged();
  void finished();

private:
  bool engage();
  void teardown(bool unlock);
  void createSurface(QScreen *screen);
  void dropSurface(QScreen *screen);

  bool m_locked = false;
  bool m_secure = false;
  QColor m_color = Qt::black;
  QPointer<QQmlComponent> m_surface;
  ryu::wl::LockObject *m_obj = nullptr;
  QList<QPointer<WlSessionLockSurface>> m_windows;
};
