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
class Hyprland;
class HyprlandWorkspace;
class HyprlandList : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)
public:
  explicit HyprlandList(QObject *parent = nullptr);
  QVariantList values() const { return m_values; }
  void setValues(const QVariantList &values);
signals:
  void valuesChanged();

private:
  QVariantList m_values;
};
class HyprlandMonitor : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("HyprlandMonitor is created by Hyprland")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(QString name READ name NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool focused READ focused NOTIFY ipcObjectChanged)
  Q_PROPERTY(HyprlandWorkspace *activeWorkspace READ activeWorkspace NOTIFY
                 activeWorkspaceChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)
public:
  HyprlandMonitor(Hyprland *owner, int id);
  int id() const { return m_id; }
  QString name() const;
  bool focused() const;
  HyprlandWorkspace *activeWorkspace() const;
  QVariantMap lastIpcObject() const { return m_ipc; }
  void setIpc(const QVariantMap &ipc);
  void resolve();
signals:
  void ipcObjectChanged();
  void activeWorkspaceChanged();

private:
  Hyprland *m_owner;
  int m_id;
  QVariantMap m_ipc;
  QPointer<HyprlandWorkspace> m_active;
};
class HyprlandWorkspace : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("HyprlandWorkspace is created by Hyprland")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(QString name READ name NOTIFY ipcObjectChanged)
  Q_PROPERTY(HyprlandMonitor *monitor READ monitor NOTIFY monitorChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)
public:
  HyprlandWorkspace(Hyprland *owner, int id);
  int id() const { return m_id; }
  QString name() const;
  HyprlandMonitor *monitor() const { return m_monitor.data(); }
  QVariantMap lastIpcObject() const { return m_ipc; }
  void setIpc(const QVariantMap &ipc);
  void resolveMonitor();
signals:
  void ipcObjectChanged();
  void monitorChanged();

private:
  Hyprland *m_owner;
  int m_id;
  QVariantMap m_ipc;
  QPointer<HyprlandMonitor> m_monitor;
};
class HyprlandToplevel : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("HyprlandToplevel is created by Hyprland")
  Q_PROPERTY(QString address READ address CONSTANT)
  Q_PROPERTY(QString title READ title NOTIFY ipcObjectChanged)
  Q_PROPERTY(QString appId READ appId NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool mapped READ mapped NOTIFY ipcObjectChanged)
  Q_PROPERTY(bool activated READ activated NOTIFY activatedChanged)
  Q_PROPERTY(
      QVariantMap lastIpcObject READ lastIpcObject NOTIFY ipcObjectChanged)
  Q_PROPERTY(
      QObject *wayland READ wayland WRITE setWayland NOTIFY waylandChanged)
public:
  HyprlandToplevel(Hyprland *owner, const QString &address);
  QString address() const { return m_address; }
  QString title() const;
  QString appId() const;
  bool mapped() const;
  bool activated() const { return m_activated; }
  QVariantMap lastIpcObject() const { return m_ipc; }
  QObject *wayland() const { return m_wayland; }
  void setWayland(QObject *wayland);
  void setActivated(bool activated);
  Q_INVOKABLE void activate();
  void setIpc(const QVariantMap &ipc);
signals:
  void ipcObjectChanged();
  void activatedChanged();
  void waylandChanged();

private:
  Hyprland *m_owner;
  QString m_address;
  QVariantMap m_ipc;
  QPointer<QObject> m_wayland;
  bool m_activated = false;
};
class Hyprland : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(HyprlandMonitor *focusedMonitor READ focusedMonitor NOTIFY
                 focusedMonitorChanged)
  Q_PROPERTY(HyprlandList *monitors READ monitors CONSTANT)
  Q_PROPERTY(HyprlandList *workspaces READ workspaces CONSTANT)
  Q_PROPERTY(HyprlandList *toplevels READ toplevels CONSTANT)
  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
public:
  explicit Hyprland(QObject *parent = nullptr);
  HyprlandMonitor *focusedMonitor() const { return m_focused; }
  HyprlandList *monitors() const { return m_monitors; }
  HyprlandList *workspaces() const { return m_workspaces; }
  HyprlandList *toplevels();
  bool connected() const { return m_connected; }
  Q_INVOKABLE HyprlandMonitor *monitorFor(QObject *screen) const;
  Q_INVOKABLE void dispatch(const QString &request);
  Q_INVOKABLE void refreshMonitors();
  Q_INVOKABLE void refreshWorkspaces();
  Q_INVOKABLE void refreshToplevels();
  Q_INVOKABLE void reconnect();
  HyprlandWorkspace *workspaceById(int id) const;
  HyprlandMonitor *monitorByName(const QString &name) const;
signals:
  void focusedMonitorChanged();
  void connectedChanged();

private:
  void connectEvents();
  void readEvents();
  void handleEvent(const QByteArray &name, const QByteArray &data);
  void request(const QByteArray &command,
               std::function<void(const QByteArray &)> done);
  void schedule(bool &flag, void (Hyprland::*refresh)());
  void applyMonitors(const QByteArray &data);
  void applyWorkspaces(const QByteArray &data);
  void applyToplevels(const QByteArray &data);
  void applyActive();
  void setConnected(bool connected);
  QLocalSocket *m_events = nullptr;
  QByteArray m_buffer;
  bool m_connected = false;
  bool m_toplevelsWanted = false;
  HyprlandList *m_monitors;
  HyprlandList *m_workspaces;
  HyprlandList *m_toplevels;
  QHash<int, HyprlandMonitor *> m_monitorMap;
  QHash<int, HyprlandWorkspace *> m_workspaceMap;
  QHash<QString, HyprlandToplevel *> m_toplevelMap;
  QPointer<HyprlandMonitor> m_focused;
  QString m_activeAddress;
  bool m_activeKnown = false;
  bool m_monitorsBusy = false;
  bool m_monitorsDirty = false;
  bool m_monitorsScheduled = false;
  bool m_workspacesBusy = false;
  bool m_workspacesDirty = false;
  bool m_workspacesScheduled = false;
  bool m_toplevelsBusy = false;
  bool m_toplevelsDirty = false;
  bool m_toplevelsScheduled = false;
};
