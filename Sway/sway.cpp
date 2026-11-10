#include "./sway.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMetaObject>
#include <QSet>
#include <cstring>
#include <memory>

namespace {

constexpr int kHeader = 14;

QString socketPath() {
  QString path = qEnvironmentVariable("SWAYSOCK");
  if (path.isEmpty())
    path = qEnvironmentVariable("I3SOCK");
  return path;
}

QByteArray frame(quint32 type, const QByteArray &payload) {
  QByteArray out;
  const quint32 length = quint32(payload.size());
  out.append("i3-ipc", 6);
  out.append(reinterpret_cast<const char *>(&length), 4);
  out.append(reinterpret_cast<const char *>(&type), 4);
  out.append(payload);
  return out;
}

bool readFrame(const QByteArray &buffer, quint32 &type, int &length) {
  if (buffer.size() < kHeader)
    return false;
  quint32 len = 0;
  std::memcpy(&len, buffer.constData() + 6, 4);
  std::memcpy(&type, buffer.constData() + 10, 4);
  if (buffer.size() < kHeader + int(len))
    return false;
  length = int(len);
  return true;
}

int workspaceKey(const QJsonObject &o) {
  const int num = o.value("num").toInt(-1);
  return num >= 0 ? num : -qAbs(o.value("id").toInt(1));
}

template <typename T, typename Key, typename MakeFn>
QVariantList sync(QHash<Key, T *> &map,
                  const QList<std::pair<Key, QVariantMap>> &items, MakeFn make,
                  QList<T *> &gone) {
  QVariantList out;
  QSet<Key> seen;
  for (const auto &item : items) {
    T *obj = map.value(item.first, nullptr);
    if (!obj) {
      obj = make(item.first);
      map.insert(item.first, obj);
    }
    obj->setIpc(item.second);
    seen.insert(item.first);
    out.append(QVariant::fromValue(static_cast<QObject *>(obj)));
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

void collect(const QJsonObject &node, const QString &output,
             const QVariantMap &workspace,
             const std::function<int(const QString &)> &monitorId,
             QList<std::pair<QString, QVariantMap>> &out) {
  const QString type = node.value("type").toString();
  const QJsonArray tiled = node.value("nodes").toArray();
  const QJsonArray floating = node.value("floating_nodes").toArray();

  if (type == "root") {
    for (const QJsonValue &child : tiled)
      collect(child.toObject(), QString(), QVariantMap(), monitorId, out);
    return;
  }

  if (type == "output") {
    const QString name = node.value("name").toString();
    for (const QJsonValue &child : tiled)
      collect(child.toObject(), name, QVariantMap(), monitorId, out);
    return;
  }

  if (type == "workspace") {
    QVariantMap ws;
    ws["id"] = workspaceKey(node);
    ws["name"] = node.value("name").toString();
    for (const QJsonValue &child : tiled)
      collect(child.toObject(), output, ws, monitorId, out);
    for (const QJsonValue &child : floating)
      collect(child.toObject(), output, ws, monitorId, out);
    return;
  }

  if (tiled.isEmpty() && floating.isEmpty() &&
      (type == "con" || type == "floating_con")) {
    const qint64 id = node.value("id").toInteger();
    const QJsonObject rect = node.value("rect").toObject();
    QString cls = node.value("app_id").toString();
    if (cls.isEmpty())
      cls =
          node.value("window_properties").toObject().value("class").toString();

    QVariantMap m;
    m["address"] = QString::number(id);
    m["foreignId"] = node.value("foreign_toplevel_identifier").toString();
    m["class"] = cls;
    m["title"] = node.value("name").toString();
    m["at"] = QVariantList{rect.value("x").toInt(), rect.value("y").toInt()};
    m["size"] =
        QVariantList{rect.value("width").toInt(), rect.value("height").toInt()};
    m["workspace"] = workspace;
    m["monitor"] = monitorId(output);
    m["floating"] = type == "floating_con";
    m["mapped"] = output != QStringLiteral("__i3");
    m["focused"] = node.value("focused").toBool();
    m["pid"] = node.value("pid").toInt();
    out.append({QString::number(id), m});
    return;
  }

  for (const QJsonValue &child : tiled)
    collect(child.toObject(), output, workspace, monitorId, out);
  for (const QJsonValue &child : floating)
    collect(child.toObject(), output, workspace, monitorId, out);
}

} // namespace

SwayList::SwayList(QObject *parent) : QObject(parent) {}

void SwayList::setValues(const QVariantList &values) {
  bool same = values.size() == m_values.size();
  for (int i = 0; same && i < values.size(); ++i)
    same = values.at(i).value<QObject *>() == m_values.at(i).value<QObject *>();
  if (same)
    return;
  m_values = values;
  emit valuesChanged();
}

SwayMonitor::SwayMonitor(Sway *owner, int id)
    : QObject(owner), m_owner(owner), m_id(id) {}

QString SwayMonitor::name() const {
  return m_ipc.value(QStringLiteral("name")).toString();
}

bool SwayMonitor::focused() const {
  return m_ipc.value(QStringLiteral("focused")).toBool();
}

SwayWorkspace *SwayMonitor::activeWorkspace() const { return m_active.data(); }

void SwayMonitor::setIpc(const QVariantMap &ipc) {
  if (m_ipc != ipc) {
    m_ipc = ipc;
    emit ipcObjectChanged();
  }
  resolve();
}

void SwayMonitor::resolve() {
  const QVariantMap active =
      m_ipc.value(QStringLiteral("activeWorkspace")).toMap();
  SwayWorkspace *workspace =
      active.isEmpty()
          ? nullptr
          : m_owner->workspaceById(active.value(QStringLiteral("id")).toInt());
  if (workspace == m_active.data())
    return;
  m_active = workspace;
  emit activeWorkspaceChanged();
}

SwayWorkspace::SwayWorkspace(Sway *owner, int id)
    : QObject(owner), m_owner(owner), m_id(id) {}

QString SwayWorkspace::name() const {
  return m_ipc.value(QStringLiteral("name")).toString();
}

void SwayWorkspace::setIpc(const QVariantMap &ipc) {
  if (m_ipc != ipc) {
    m_ipc = ipc;
    emit ipcObjectChanged();
  }
  resolveMonitor();
}

void SwayWorkspace::resolveMonitor() {
  SwayMonitor *monitor =
      m_owner->monitorByName(m_ipc.value(QStringLiteral("monitor")).toString());
  if (monitor == m_monitor.data())
    return;
  m_monitor = monitor;
  emit monitorChanged();
}

SwayToplevel::SwayToplevel(Sway *owner, const QString &address)
    : QObject(owner), m_owner(owner), m_address(address) {}

QString SwayToplevel::title() const {
  return m_ipc.value(QStringLiteral("title")).toString();
}

QString SwayToplevel::appId() const {
  return m_ipc.value(QStringLiteral("class")).toString();
}

bool SwayToplevel::mapped() const {
  return m_ipc.value(QStringLiteral("mapped"), true).toBool();
}

void SwayToplevel::setActivated(bool activated) {
  if (m_activated == activated)
    return;
  m_activated = activated;
  emit activatedChanged();
}

void SwayToplevel::setIpc(const QVariantMap &ipc) {
  if (m_ipc == ipc)
    return;
  m_ipc = ipc;
  emit ipcObjectChanged();
}

void SwayToplevel::activate() {
  m_owner->dispatch(QStringLiteral("[con_id=%1] focus").arg(m_address));
}

Sway::Sway(QObject *parent)
    : QObject(parent), m_monitors(new SwayList(this)),
      m_workspaces(new SwayList(this)), m_toplevels(new SwayList(this)) {
  connectEvents();
  refreshLayout();
}

SwayList *Sway::toplevels() {
  if (!m_toplevelsWanted)
    refreshToplevels();
  return m_toplevels;
}

SwayMonitor *Sway::monitorFor(QObject *screen) const {
  if (!screen)
    return nullptr;
  return monitorByName(screen->property("name").toString());
}

SwayWorkspace *Sway::workspaceById(int id) const {
  return m_workspaceMap.value(id, nullptr);
}

SwayMonitor *Sway::monitorByName(const QString &name) const {
  if (name.isEmpty())
    return nullptr;
  return m_monitorMap.value(name, nullptr);
}

int Sway::monitorId(const QString &name) {
  auto it = m_monitorIds.find(name);
  if (it != m_monitorIds.end())
    return it.value();
  const int id = m_nextMonitorId++;
  m_monitorIds.insert(name, id);
  return id;
}

void Sway::dispatch(const QString &command) {
  request(0, command.toUtf8(), nullptr);
}

void Sway::reconnect() {
  if (m_events) {
    m_events->disconnect(this);
    m_events->deleteLater();
    m_events = nullptr;
  }
  m_buffer.clear();
  setConnected(false);
  connectEvents();
  refreshLayout();
  if (m_toplevelsWanted)
    refreshToplevels();
}

void Sway::setConnected(bool connected) {
  if (m_connected == connected)
    return;
  m_connected = connected;
  emit connectedChanged();
}

void Sway::connectEvents() {
  const QString path = socketPath();
  if (path.isEmpty())
    return;
  m_events = new QLocalSocket(this);
  connect(m_events, &QLocalSocket::connected, this, [this] {
    m_events->write(
        frame(2, QByteArrayLiteral("[\"workspace\",\"output\",\"window\"]")));
    setConnected(true);
  });
  connect(m_events, &QLocalSocket::disconnected, this,
          [this] { setConnected(false); });
  connect(m_events, &QLocalSocket::readyRead, this, &Sway::readEvents);
  m_events->connectToServer(path);
}

void Sway::readEvents() {
  m_buffer.append(m_events->readAll());
  quint32 type = 0;
  int length = 0;
  while (readFrame(m_buffer, type, length)) {
    const QByteArray payload = m_buffer.mid(kHeader, length);
    m_buffer.remove(0, kHeader + length);
    if (type & 0x80000000u)
      handleEvent(type, payload);
  }
}

void Sway::handleEvent(quint32 type, const QByteArray &payload) {
  switch (type & 0x7fffffffu) {
  case 0:
  case 1:
    schedule(m_layoutScheduled, &Sway::refreshLayout);
    if (m_toplevelsWanted)
      schedule(m_toplevelsScheduled, &Sway::refreshToplevels);
    break;
  case 3: {
    const QJsonObject obj = QJsonDocument::fromJson(payload).object();
    if (obj.value("change").toString() == QLatin1String("focus")) {
      m_activeAddress = QString::number(
          obj.value("container").toObject().value("id").toInteger());
      applyActive();
    }
    if (m_toplevelsWanted)
      schedule(m_toplevelsScheduled, &Sway::refreshToplevels);
    break;
  }
  default:
    break;
  }
}

void Sway::applyActive() {
  for (SwayToplevel *toplevel : std::as_const(m_toplevelMap))
    toplevel->setActivated(!m_activeAddress.isEmpty() &&
                           toplevel->address() == m_activeAddress);
}

void Sway::schedule(bool &flag, void (Sway::*refresh)()) {
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

void Sway::request(quint32 type, const QByteArray &payload,
                   std::function<void(const QByteArray &)> done) {
  const QString path = socketPath();
  if (path.isEmpty()) {
    if (done)
      done(QByteArray());
    return;
  }

  auto buffer = std::make_shared<QByteArray>();
  auto finished = std::make_shared<bool>(false);
  auto *socket = new QLocalSocket(this);

  auto finish = [socket, finished, done](const QByteArray &result) {
    if (*finished)
      return;
    *finished = true;
    socket->disconnect();
    socket->abort();
    socket->deleteLater();
    if (done)
      done(result);
  };

  connect(socket, &QLocalSocket::connected, socket,
          [socket, type, payload] { socket->write(frame(type, payload)); });
  connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer, finish] {
    buffer->append(socket->readAll());
    quint32 replyType = 0;
    int length = 0;
    if (readFrame(*buffer, replyType, length))
      finish(buffer->mid(kHeader, length));
  });
  connect(socket, &QLocalSocket::disconnected, socket,
          [finish] { finish(QByteArray()); });
  connect(socket, &QLocalSocket::errorOccurred, socket,
          [finish](QLocalSocket::LocalSocketError) { finish(QByteArray()); });

  socket->connectToServer(path);
}

void Sway::refreshMonitors() { refreshLayout(); }

void Sway::refreshWorkspaces() { refreshLayout(); }

void Sway::refreshLayout() {
  if (m_layoutBusy) {
    m_layoutDirty = true;
    return;
  }
  m_layoutBusy = true;
  request(3, QByteArray(), [this](const QByteArray &outputs) {
    request(1, QByteArray(), [this, outputs](const QByteArray &workspaces) {
      m_layoutBusy = false;
      applyLayout(outputs, workspaces);
      if (m_layoutDirty) {
        m_layoutDirty = false;
        refreshLayout();
      }
    });
  });
}

void Sway::refreshToplevels() {
  m_toplevelsWanted = true;
  if (m_toplevelsBusy) {
    m_toplevelsDirty = true;
    return;
  }
  m_toplevelsBusy = true;
  request(4, QByteArray(), [this](const QByteArray &data) {
    m_toplevelsBusy = false;
    applyToplevels(data);
    if (m_toplevelsDirty) {
      m_toplevelsDirty = false;
      refreshToplevels();
    }
  });
}

void Sway::applyLayout(const QByteArray &outputs,
                       const QByteArray &workspaces) {
  const QJsonDocument outDoc = QJsonDocument::fromJson(outputs);
  const QJsonDocument wsDoc = QJsonDocument::fromJson(workspaces);
  if (!outDoc.isArray() || !wsDoc.isArray())
    return;

  QHash<QString, QJsonObject> visible;
  QList<std::pair<int, QVariantMap>> wsItems;
  for (const QJsonValue &value : wsDoc.array()) {
    const QJsonObject o = value.toObject();
    const int id = workspaceKey(o);
    const QString output = o.value("output").toString();
    if (o.value("visible").toBool())
      visible.insert(output, o);
    QVariantMap m;
    m["id"] = id;
    m["name"] = o.value("name").toString();
    m["monitor"] = output;
    m["focused"] = o.value("focused").toBool();
    m["visible"] = o.value("visible").toBool();
    m["urgent"] = o.value("urgent").toBool();
    wsItems.append({id, m});
  }

  QList<std::pair<QString, QVariantMap>> monItems;
  for (const QJsonValue &value : outDoc.array()) {
    const QJsonObject o = value.toObject();
    if (!o.value("active").toBool(true))
      continue;
    const QString name = o.value("name").toString();
    const QJsonObject rect = o.value("rect").toObject();
    double scale = o.value("scale").toDouble(1.0);
    if (scale <= 0)
      scale = 1.0;
    const int x = rect.value("x").toInt();
    const int y = rect.value("y").toInt();
    const int w = rect.value("width").toInt();
    const int h = rect.value("height").toInt();

    QVariantList reserved{0, 0, 0, 0};
    QVariantMap active;
    const auto vis = visible.constFind(name);
    if (vis != visible.constEnd()) {
      const QJsonObject wr = vis.value().value("rect").toObject();
      const int wx = wr.value("x").toInt();
      const int wy = wr.value("y").toInt();
      const int ww = wr.value("width").toInt();
      const int wh = wr.value("height").toInt();
      if (ww > 0 && wh > 0)
        reserved = QVariantList{wx - x, wy - y, (x + w) - (wx + ww),
                                (y + h) - (wy + wh)};
      active["id"] = workspaceKey(vis.value());
      active["name"] = vis.value().value("name").toString();
    }

    QVariantMap m;
    m["id"] = monitorId(name);
    m["name"] = name;
    m["focused"] = o.value("focused").toBool();
    m["x"] = x;
    m["y"] = y;
    m["width"] = qRound(w * scale);
    m["height"] = qRound(h * scale);
    m["scale"] = scale;
    m["reserved"] = reserved;
    m["activeWorkspace"] = active;
    monItems.append({name, m});
  }

  QList<SwayWorkspace *> goneWs;
  QList<SwayMonitor *> goneMon;
  const QVariantList wsList = sync<SwayWorkspace, int>(
      m_workspaceMap, wsItems,
      [this](int id) { return new SwayWorkspace(this, id); }, goneWs);
  const QVariantList monList = sync<SwayMonitor, QString>(
      m_monitorMap, monItems,
      [this](const QString &name) {
        return new SwayMonitor(this, monitorId(name));
      },
      goneMon);

  SwayMonitor *focused = nullptr;
  for (const auto &item : monItems) {
    if (item.second.value(QStringLiteral("focused")).toBool()) {
      focused = m_monitorMap.value(item.first, nullptr);
      break;
    }
  }

  m_workspaces->setValues(wsList);
  m_monitors->setValues(monList);

  if (focused != m_focused.data()) {
    m_focused = focused;
    emit focusedMonitorChanged();
  }

  for (SwayWorkspace *workspace : std::as_const(m_workspaceMap))
    workspace->resolveMonitor();
  for (SwayMonitor *monitor : std::as_const(m_monitorMap))
    monitor->resolve();

  for (SwayWorkspace *workspace : std::as_const(goneWs))
    workspace->deleteLater();
  for (SwayMonitor *monitor : std::as_const(goneMon))
    monitor->deleteLater();
}

void Sway::applyToplevels(const QByteArray &data) {
  const QJsonDocument doc = QJsonDocument::fromJson(data);
  if (!doc.isObject())
    return;

  QList<std::pair<QString, QVariantMap>> items;
  collect(
      doc.object(), QString(), QVariantMap(),
      [this](const QString &name) { return monitorId(name); }, items);

  QString active;
  for (const auto &item : items) {
    if (item.second.value(QStringLiteral("focused")).toBool()) {
      active = item.first;
      break;
    }
  }

  QList<SwayToplevel *> gone;
  const QVariantList list = sync<SwayToplevel, QString>(
      m_toplevelMap, items,
      [this](const QString &address) {
        return new SwayToplevel(this, address);
      },
      gone);

  m_activeAddress = active;
  applyActive();
  m_toplevels->setValues(list);

  for (SwayToplevel *toplevel : std::as_const(gone))
    toplevel->deleteLater();
}
