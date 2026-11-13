#pragma once

#include <QColor>
#include <QImage>
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
#include <QVariantMap>

namespace ryu::wl {
class LockObject;
QImage backdropFor(const QString &name);
} // namespace ryu::wl

class LockBackdrop : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString screenName READ screenName WRITE setScreenName NOTIFY
                 screenNameChanged)
  Q_PROPERTY(bool hasImage READ hasImage NOTIFY hasImageChanged)

public:
  explicit LockBackdrop(QQuickItem *parent = nullptr);

  QString screenName() const { return m_screenName; }
  void setScreenName(const QString &name);
  bool hasImage() const { return !m_image.isNull(); }

signals:
  void screenNameChanged();
  void hasImageChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override;
  void geometryChange(const QRectF &next, const QRectF &previous) override;

private:
  QString m_screenName;
  QImage m_image;
  bool m_textureDirty = true;
};

class WlSessionLockSurface : public QQuickWindow, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_CLASSINFO("DefaultProperty", "contents")
  Q_PROPERTY(QQmlListProperty<QObject> contents READ contents)
  Q_PROPERTY(QObject *screen READ screenSource WRITE setScreenSource NOTIFY
                 screenSourceChanged)
  Q_PROPERTY(QString screenName READ screenName NOTIFY screenSourceChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
  Q_PROPERTY(bool painted READ painted NOTIFY paintedChanged)

public:
  explicit WlSessionLockSurface(QWindow *parent = nullptr);
  ~WlSessionLockSurface() override;

  void classBegin() override;
  void componentComplete() override {}

  QQmlListProperty<QObject> contents();

  QObject *screenSource() const { return m_screen.data(); }
  void setScreenSource(QObject *screen);
  QString screenName() const;

  bool ready() const { return m_ready; }
  void setReady();
  bool painted() const { return m_painted; }
  bool revealed() const { return m_revealed; }
  void markRevealed();

  QScreen *resolveScreen() const;
  void present();

  static void setDefaultColor(const QColor &color);

signals:
  void screenSourceChanged();
  void readyChanged();
  void paintedChanged();

private:
  static void appendContent(QQmlListProperty<QObject> *list, QObject *object);
  static qsizetype contentCount(QQmlListProperty<QObject> *list);
  static QObject *contentAt(QQmlListProperty<QObject> *list, qsizetype index);
  static void clearContents(QQmlListProperty<QObject> *list);

  QPointer<QObject> m_screen;
  QList<QObject *> m_contents;
  bool m_ready = false;
  bool m_revealed = false;
  bool m_painted = false;
  bool m_presented = false;
};

class WlSessionLock : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("DefaultProperty", "surface")
  Q_PROPERTY(bool locked READ locked WRITE setLocked NOTIFY lockedChanged)
  Q_PROPERTY(bool secure READ secure NOTIFY secureChanged)
  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QVariantMap backgrounds READ backgrounds WRITE setBackgrounds
                 NOTIFY backgroundsChanged)
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

  QVariantMap backgrounds() const { return m_backgrounds; }
  void setBackgrounds(const QVariantMap &backgrounds);

  QQmlComponent *surface() const { return m_surface.data(); }
  void setSurface(QQmlComponent *surface);

  void handleLocked();
  void handleFinished();

signals:
  void lockedChanged();
  void secureChanged();
  void colorChanged();
  void backgroundsChanged();
  void surfaceChanged();
  void finished();

private:
  enum State { Idle, Preparing, Locking };

  bool prepare();
  void handleLoaded(quint64 generation, const QString &name,
                    const QImage &image);
  void beginLock();
  bool engage();
  void fail();
  void teardown(bool unlock);
  void discardWindows();
  WlSessionLockSurface *buildSurface(QScreen *screen);
  WlSessionLockSurface *windowFor(QScreen *screen) const;
  void addScreen(QScreen *screen);
  void dropSurface(QScreen *screen);

  State m_state = Idle;
  quint64 m_generation = 0;
  int m_loading = 0;
  bool m_locked = false;
  bool m_secure = false;
  QColor m_color = Qt::black;
  QVariantMap m_backgrounds;
  QPointer<QQmlComponent> m_surface;
  ryu::wl::LockObject *m_obj = nullptr;
  QList<QPointer<WlSessionLockSurface>> m_windows;
};
