#include "./hyprland.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMetaObject>
#include <QSet>

#include <memory>

namespace {

QString socketPath(const char *file) {
  const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
  const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
  if (runtime.isEmpty() || signature.isEmpty())
    return QString();
  return runtime + QStringLiteral("/hypr/") + signature + QLatin1Char('/') +
         QLatin1String(file);
}

QString normAddress(QString address) {
  if (address.startsWith(QStringLiteral("0x")))
    address.remove(0, 2);
  return address;
}

template <typename T, typename Key, typename KeyFn, typename MakeFn>
QVariantList sync(QHash<Key, T *> &map, const QJsonArray &array, KeyFn keyOf,
                  MakeFn make, QList<T *> &gone) {
  QVariantList out;
  QSet<Key> seen;
  for (const QJsonValue &value : array) {
    const QJsonObject obj = value.toObject();
    const Key key = keyOf(obj);
    T *item = map.value(key, nullptr);
    if (!item) {
      item = make(key);
      map.insert(key, item);
    }
    item->setIpc(obj.toVariantMap());
    seen.insert(key);
    out.append(QVariant::fromValue(static_cast<QObject *>(item)));
  }
  for (auto it = map.begin(); it != map.end();) {
    if (!seen.contains(it.key())) {
      gone.append(it.value());
      it = map.erase(it);
    } else {
      ++it;
    }
  }
  return out;
}

} // namespace

HyprlandList::HyprlandList(QObject *parent) : QObject(parent) {}

void HyprlandList::setValues(const QVariantList &values) {
  bool same = values.size() == m_values.size();
  for (int i = 0; same && i < values.size(); ++i)
    same = values.at(i).value<QObject *>() == m_values.at(i).value<QObject *>();
  if (same)
    return;
  m_values = values;
  emit valuesChanged();
}

HyprlandMonitor::HyprlandMonitor(Hyprland *owner, int id)
    : QObject(owner), m_owner(owner), m_id(id) {}

QString HyprlandMonitor::name() const {
  return m_ipc.value(QStringLiteral("name")).toString();
}

bool HyprlandMonitor::focused() const {
  return m_ipc.value(QStringLiteral("focused")).toBool();
}

HyprlandWorkspace *HyprlandMonitor::activeWorkspace() const {
  return m_active.data();
}

void HyprlandMonitor::setIpc(const QVariantMap &ipc) {
  if (m_ipc != ipc) {
    m_ipc = ipc;
    emit ipcObjectChanged();
  }
  resolve();
}

void HyprlandMonitor::resolve() {
  const int id = m_ipc.value(QStringLiteral("activeWorkspace"))
                     .toMap()
                     .value(QStringLiteral("id"))
                     .toInt();
  HyprlandWorkspace *workspace = m_owner->workspaceById(id);
  if (workspace == m_active.data())
    return;
  m_active = workspace;
  emit activeWorkspaceChanged();
}

HyprlandWorkspace::HyprlandWorkspace(Hyprland *owner, int id)
    : QObject(owner), m_owner(owner), m_id(id) {}

QString HyprlandWorkspace::name() const {
  return m_ipc.value(QStringLiteral("name")).toString();
}

void HyprlandWorkspace::setIpc(const QVariantMap &ipc) {
  if (m_ipc != ipc) {
    m_ipc = ipc;
    emit ipcObjectChanged();
  }
  resolveMonitor();
}

void HyprlandWorkspace::resolveMonitor() {
  HyprlandMonitor *monitor =
      m_owner->monitorByName(m_ipc.value(QStringLiteral("monitor")).toString());
  if (monitor == m_monitor.data())
    return;
  m_monitor = monitor;
  emit monitorChanged();
}

HyprlandToplevel::HyprlandToplevel(Hyprland *owner, const QString &address)
    : QObject(owner), m_owner(owner), m_address(address) {}

QString HyprlandToplevel::title() const {
  return m_ipc.value(QStringLiteral("title")).toString();
}

void HyprlandToplevel::setWayland(QObject *wayland) {
  if (m_wayland == wayland)
    return;
  m_wayland = wayland;
  emit waylandChanged();
}

void HyprlandToplevel::activate() {
  m_owner->dispatch(QStringLiteral("hl.dsp.focus({window = \"address:0x%1\"})")
                        .arg(m_address));
}

void HyprlandToplevel::setIpc(const QVariantMap &ipc) {
  if (m_ipc == ipc)
    return;
  m_ipc = ipc;
  emit ipcObjectChanged();
}

Hyprland::Hyprland(QObject *parent)
    : QObject(parent), m_monitors(new HyprlandList(this)),
      m_workspaces(new HyprlandList(this)),
      m_toplevels(new HyprlandList(this)) {
  connectEvents();
  refreshMonitors();
  refreshWorkspaces();
}

HyprlandList *Hyprland::toplevels() {
  if (!m_toplevelsWanted)
    refreshToplevels();
  return m_toplevels;
}

HyprlandMonitor *Hyprland::monitorFor(QObject *screen) const {
  if (!screen)
    return nullptr;
  return monitorByName(screen->property("name").toString());
}

HyprlandWorkspace *Hyprland::workspaceById(int id) const {
  return m_workspaceMap.value(id, nullptr);
}

HyprlandMonitor *Hyprland::monitorByName(const QString &name) const {
  if (name.isEmpty())
    return nullptr;
  for (HyprlandMonitor *monitor : m_monitorMap) {
    if (monitor->name() == name)
      return monitor;
  }
  return nullptr;
}

void Hyprland::dispatch(const QString &requestText) {
  request("dispatch " + requestText.toUtf8(), nullptr);
}

void Hyprland::reconnect() {
  if (m_events) {
    m_events->disconnect(this);
    m_events->deleteLater();
    m_events = nullptr;
  }
  m_buffer.clear();
  setConnected(false);
  connectEvents();
  refreshMonitors();
  refreshWorkspaces();
  if (m_toplevelsWanted)
    refreshToplevels();
}

void Hyprland::setConnected(bool connected) {
  if (m_connected == connected)
    return;
  m_connected = connected;
  emit connectedChanged();
}

void Hyprland::connectEvents() {
  const QString path = socketPath(".socket2.sock");
  if (path.isEmpty())
    return;
  m_events = new QLocalSocket(this);
  connect(m_events, &QLocalSocket::connected, this,
          [this] { setConnected(true); });
  connect(m_events, &QLocalSocket::disconnected, this,
          [this] { setConnected(false); });
  connect(m_events, &QLocalSocket::readyRead, this, &Hyprland::readEvents);
  m_events->connectToServer(path);
}

void Hyprland::readEvents() {
  m_buffer.append(m_events->readAll());
  int index;
  while ((index = m_buffer.indexOf('\n')) >= 0) {
    const QByteArray line = m_buffer.left(index);
    m_buffer.remove(0, index + 1);
    const int split = line.indexOf(">>");
    handleEvent(split >= 0 ? line.left(split) : line);
  }
}

void Hyprland::handleEvent(const QByteArray &name) {
  static const QSet<QByteArray> layout = {
      "workspace",        "workspacev2",        "focusedmon",
      "focusedmonv2",     "createworkspace",    "createworkspacev2",
      "destroyworkspace", "destroyworkspacev2", "moveworkspace",
      "moveworkspacev2",  "renameworkspace",    "monitoradded",
      "monitoraddedv2",   "monitorremoved",     "monitorremovedv2",
      "activespecial",    "activespecialv2",    "configreloaded"};
  static const QSet<QByteArray> windows = {
      "openwindow",  "closewindow",   "movewindow",         "movewindowv2",
      "windowtitle", "windowtitlev2", "changefloatingmode", "fullscreen",
      "pin",         "urgent"};
  static const QSet<QByteArray> focus = {"activewindowv2"};

  if (layout.contains(name)) {
    schedule(m_monitorsScheduled, &Hyprland::refreshMonitors);
    schedule(m_workspacesScheduled, &Hyprland::refreshWorkspaces);
  }
  if (windows.contains(name))
    schedule(m_workspacesScheduled, &Hyprland::refreshWorkspaces);
  if (m_toplevelsWanted && (windows.contains(name) || focus.contains(name) ||
                            name == "configreloaded"))
    schedule(m_toplevelsScheduled, &Hyprland::refreshToplevels);
}

void Hyprland::schedule(bool &flag, void (Hyprland::*refresh)()) {
  if (flag)
    return;
  flag = true;
  QMetaObject::invokeMethod(
      this,
      [this, &flag, refresh] {
        flag = false;
        (this->*refresh)();
      },
      Qt::QueuedConnection);
}

void Hyprland::request(const QByteArray &command,
                       std::function<void(const QByteArray &)> done) {
  const QString path = socketPath(".socket.sock");
  if (path.isEmpty()) {
    if (done)
      done(QByteArray());
    return;
  }

  struct State {
    QByteArray data;
    bool finished = false;
  };
  auto state = std::make_shared<State>();
  auto *socket = new QLocalSocket(this);

  auto finish = [socket, state, done] {
    if (state->finished)
      return;
    state->finished = true;
    socket->deleteLater();
    if (done)
      done(state->data);
  };

  connect(socket, &QLocalSocket::connected, socket,
          [socket, command] { socket->write(command); });
  connect(socket, &QLocalSocket::readyRead, socket,
          [socket, state] { state->data.append(socket->readAll()); });
  connect(socket, &QLocalSocket::disconnected, socket, finish);
  connect(socket, &QLocalSocket::errorOccurred, socket,
          [state, finish](QLocalSocket::LocalSocketError error) {
            if (error == QLocalSocket::PeerClosedError)
              return;
            state->data.clear();
            finish();
          });

  socket->connectToServer(path);
}

void Hyprland::refreshMonitors() {
  if (m_monitorsBusy) {
    m_monitorsDirty = true;
    return;
  }
  m_monitorsBusy = true;
  request("j/monitors", [this](const QByteArray &data) {
    m_monitorsBusy = false;
    applyMonitors(data);
    if (m_monitorsDirty) {
      m_monitorsDirty = false;
      refreshMonitors();
    }
  });
}

void Hyprland::refreshWorkspaces() {
  if (m_workspacesBusy) {
    m_workspacesDirty = true;
    return;
  }
  m_workspacesBusy = true;
  request("j/workspaces", [this](const QByteArray &data) {
    m_workspacesBusy = false;
    applyWorkspaces(data);
    if (m_workspacesDirty) {
      m_workspacesDirty = false;
      refreshWorkspaces();
    }
  });
}

void Hyprland::refreshToplevels() {
  m_toplevelsWanted = true;
  if (m_toplevelsBusy) {
    m_toplevelsDirty = true;
    return;
  }
  m_toplevelsBusy = true;
  request("j/clients", [this](const QByteArray &data) {
    m_toplevelsBusy = false;
    applyToplevels(data);
    if (m_toplevelsDirty) {
      m_toplevelsDirty = false;
      refreshToplevels();
    }
  });
}

void Hyprland::applyMonitors(const QByteArray &data) {
  const QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isArray())
    return;

  const QJsonArray array = doc.array();
  QList<HyprlandMonitor *> gone;
  const QVariantList list = sync<HyprlandMonitor, int>(
      m_monitorMap, array,
      [](const QJsonObject &o) { return o.value("id").toInt(); },
      [this](int id) { return new HyprlandMonitor(this, id); }, gone);

  HyprlandMonitor *focused = nullptr;
  for (const QJsonValue &value : array) {
    const QJsonObject obj = value.toObject();
    if (obj.value("focused").toBool()) {
      focused = m_monitorMap.value(obj.value("id").toInt(), nullptr);
      break;
    }
  }

  m_monitors->setValues(list);
  if (focused != m_focused.data()) {
    m_focused = focused;
    emit focusedMonitorChanged();
  }
  for (HyprlandWorkspace *workspace : std::as_const(m_workspaceMap))
    workspace->resolveMonitor();
}

void Hyprland::applyWorkspaces(const QByteArray &data) {
  const QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isArray())
    return;

  QList<HyprlandWorkspace *> gone;
  const QVariantList list = sync<HyprlandWorkspace, int>(
      m_workspaceMap, doc.array(),
      [](const QJsonObject &o) { return o.value("id").toInt(); },
      [this](int id) { return new HyprlandWorkspace(this, id); }, gone);

  m_workspaces->setValues(list);
  for (HyprlandMonitor *monitor : std::as_const(m_monitorMap))
    monitor->resolve();
  for (HyprlandWorkspace *workspace : std::as_const(gone))
    workspace->deleteLater();
}

void Hyprland::applyToplevels(const QByteArray &data) {
  const QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isArray())
    return;

  QList<HyprlandToplevel *> gone;
  const QVariantList list = sync<HyprlandToplevel, QString>(
      m_toplevelMap, doc.array(),
      [](const QJsonObject &o) {
        return normAddress(o.value("address").toString());
      },
      [this](const QString &address) {
        return new HyprlandToplevel(this, address);
      },
      gone);

  m_toplevels->setValues(list);
  for (HyprlandToplevel *toplevel : std::as_const(gone))
    toplevel->deleteLater();
}
