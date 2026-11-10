#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <qqmlregistration.h>

class QLocalSocket;
class Sway;
class SwayWorkspace;

class SwayList : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)

public:
  explicit SwayList(QObject *parent = nullptr);
  QVariantList values() const { return m_values; }
  void setValues(const QVariantList &values);

signals:
  void valuesChanged();

private:
  QVariantList m_values;
};

class SwayMonitor : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("SwayMonitor is created by Sway")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(QString name READ name NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool focused READ focused NOTIFY ipcObjectChanged)
  Q_PROPERTY(SwayWorkspace *activeWorkspace READ activeWorkspace NOTIFY
                 activeWorkspaceChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)

public:
  SwayMonitor(Sway *owner, int id);
  int id() const { return m_id; }
  QString name() const;
  bool focused() const;
  SwayWorkspace *activeWorkspace() const;
  QVariantMap lastIpcObject() const { return m_ipc; }
  void setIpc(const QVariantMap &ipc);
  void resolve();

signals:
  void ipcObjectChanged();
  void activeWorkspaceChanged();

private:
  Sway *m_owner;
  int m_id;
  QVariantMap m_ipc;
  QPointer<SwayWorkspace> m_active;
};

class SwayWorkspace : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("SwayWorkspace is created by Sway")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(QString name READ name NOTIFY ipcObjectChanged)
  Q_PROPERTY(SwayMonitor *monitor READ monitor NOTIFY monitorChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)

public:
  SwayWorkspace(Sway *owner, int id);
  int id() const { return m_id; }
  QString name() const;
  SwayMonitor *monitor() const { return m_monitor.data(); }
  QVariantMap lastIpcObject() const { return m_ipc; }
  void setIpc(const QVariantMap &ipc);
  void resolveMonitor();

signals:
  void ipcObjectChanged();
  void monitorChanged();

private:
  Sway *m_owner;
  int m_id;
  QVariantMap m_ipc;
  QPointer<SwayMonitor> m_monitor;
};

class SwayToplevel : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("SwayToplevel is created by Sway")
  Q_PROPERTY(QString address READ address CONSTANT)
  Q_PROPERTY(QString foreignId READ foreignId NOTIFY ipcObjectChanged)
  Q_PROPERTY(QString title READ title NOTIFY ipcObjectChanged)
  Q_PROPERTY(QString appId READ appId NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool mapped READ mapped NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool activated READ activated NOTIFY activatedChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)

public:
  SwayToplevel(Sway *owner, const QString &address);
  QString address() const { return m_address; }
  QString foreignId() const {
    return m_ipc.value(QStringLiteral("foreignId")).toString();
  }
  QString title() const;
  QString appId() const;
  bool mapped() const;
  bool activated() const { return m_activated; }
  QVariantMap lastIpcObject() const { return m_ipc; }
  void setActivated(bool activated);
  void setIpc(const QVariantMap &ipc);
  Q_INVOKABLE void activate();

signals:
  void ipcObjectChanged();
  void activatedChanged();

private:
  Sway *m_owner;
  QString m_address;
  QVariantMap m_ipc;
  bool m_activated = false;
};
class Sway : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(SwayMonitor *focusedMonitor READ focusedMonitor NOTIFY
                 focusedMonitorChanged)
  Q_PROPERTY(SwayList *monitors READ monitors CONSTANT)
  Q_PROPERTY(SwayList *workspaces READ workspaces CONSTANT)
  Q_PROPERTY(SwayList *toplevels READ toplevels CONSTANT)
  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

public:
  explicit Sway(QObject *parent = nullptr);
  SwayMonitor *focusedMonitor() const { return m_focused; }
  SwayList *monitors() const { return m_monitors; }
  SwayList *workspaces() const { return m_workspaces; }
  SwayList *toplevels();
  bool connected() const { return m_connected; }

  Q_INVOKABLE SwayMonitor *monitorFor(QObject *screen) const;
  Q_INVOKABLE void dispatch(const QString &command);
  Q_INVOKABLE void refreshMonitors();
  Q_INVOKABLE void refreshWorkspaces();
  Q_INVOKABLE void refreshToplevels();
  Q_INVOKABLE void reconnect();

  SwayWorkspace *workspaceById(int id) const;
  SwayMonitor *monitorByName(const QString &name) const;

signals:
  void focusedMonitorChanged();
  void connectedChanged();

private:
  void connectEvents();
  void readEvents();
  void handleEvent(quint32 type, const QByteArray &payload);
  void request(quint32 type, const QByteArray &payload,
               std::function<void(const QByteArray &)> done);
  void schedule(bool &flag, void (Sway::*refresh)());
  void refreshLayout();
  void applyLayout(const QByteArray &outputs, const QByteArray &workspaces);
  void applyToplevels(const QByteArray &data);
  void applyActive();
  void setConnected(bool connected);
  int monitorId(const QString &name);

  QLocalSocket *m_events = nullptr;
  QByteArray m_buffer;
  bool m_connected = false;
  bool m_toplevelsWanted = false;
  SwayList *m_monitors;
  SwayList *m_workspaces;
  SwayList *m_toplevels;
  QHash<QString, SwayMonitor *> m_monitorMap;
  QHash<int, SwayWorkspace *> m_workspaceMap;
  QHash<QString, SwayToplevel *> m_toplevelMap;
  QHash<QString, int> m_monitorIds;
  int m_nextMonitorId = 0;
  QPointer<SwayMonitor> m_focused;
  QString m_activeAddress;
  bool m_layoutBusy = false;
  bool m_layoutDirty = false;
  bool m_layoutScheduled = false;
  bool m_toplevelsBusy = false;
  bool m_toplevelsDirty = false;
  bool m_toplevelsScheduled = false;
};
