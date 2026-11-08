#include "./iwd.hpp"
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
const QString kService = QStringLiteral("net.connman.iwd");
const QString kRoot = QStringLiteral("/net/connman/iwd");
const QString kAgentManager = QStringLiteral("net.connman.iwd.AgentManager");
const QString kDeviceIface = QStringLiteral("net.connman.iwd.Device");
const QString kStationIface = QStringLiteral("net.connman.iwd.Station");
const QString kNetworkIface = QStringLiteral("net.connman.iwd.Network");
const QString kKnownIface = QStringLiteral("net.connman.iwd.KnownNetwork");
const QString kPropsIface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kObjectManager =
    QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kAgentError =
    QStringLiteral("net.connman.iwd.Agent.Error.Canceled");
constexpr int kCallTimeoutMs = 25000;
constexpr int kConnectTimeoutMs = 120000;
constexpr int kDebounceMs = 150;

QString messageOf(const QDBusMessage &r) {
  return r.errorMessage().isEmpty() ? r.errorName() : r.errorMessage();
}

bool quietError(const QDBusMessage &r) {
  const QString n = r.errorName();
  return n.endsWith(QLatin1String("Aborted")) ||
         n.endsWith(QLatin1String("Canceled")) ||
         n.endsWith(QLatin1String("Busy")) ||
         n.endsWith(QLatin1String("InProgress"));
}

QString pathOf(const QVariant &v) { return v.value<QDBusObjectPath>().path(); }
} // namespace

IwdAgent::IwdAgent(QObject *parent) : QObject(parent) {}

void IwdAgent::begin(const QString &kind, const QDBusObjectPath &network,
                     const QString &user, const QDBusMessage &message) {
  if (m_pending)
    abort();
  message.setDelayedReply(true);
  m_message = message;
  m_kind = kind;
  m_network = network.path();
  m_user = user;
  m_pending = true;
  emit requested();
}

void IwdAgent::reset() {
  m_pending = false;
  m_kind.clear();
  m_network.clear();
  m_user.clear();
  m_message = QDBusMessage();
  emit cleared();
}

void IwdAgent::submit(const QString &user, const QString &secret) {
  if (!m_pending)
    return;
  QDBusMessage reply;
  if (m_kind == QLatin1String("userAndPassword"))
    reply = m_message.createReply(QVariantList{user, secret});
  else
    reply = m_message.createReply(secret);
  QDBusConnection::systemBus().send(reply);
  reset();
}

void IwdAgent::abort() {
  if (!m_pending)
    return;
  QDBusConnection::systemBus().send(
      m_message.createErrorReply(kAgentError, QStringLiteral("canceled")));
  reset();
}

void IwdAgent::Release() {
  if (m_pending)
    reset();
}

void IwdAgent::Cancel(const QString &) {
  if (m_pending)
    reset();
}

void IwdAgent::RequestPassphrase(const QDBusObjectPath &network,
                                 const QDBusMessage &message) {
  begin(QStringLiteral("passphrase"), network, QString(), message);
}

void IwdAgent::RequestPrivateKeyPassphrase(const QDBusObjectPath &network,
                                           const QDBusMessage &message) {
  begin(QStringLiteral("privateKeyPassphrase"), network, QString(), message);
}

void IwdAgent::RequestUserNameAndPassword(const QDBusObjectPath &network,
                                          const QDBusMessage &message) {
  begin(QStringLiteral("userAndPassword"), network, QString(), message);
}

void IwdAgent::RequestUserPassword(const QDBusObjectPath &network,
                                   const QString &user,
                                   const QDBusMessage &message) {
  begin(QStringLiteral("userPassword"), network, user, message);
}

Iwd::Iwd(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::systemBus()) {
  qDBusRegisterMetaType<QMap<QString, QVariantMap>>();
  qDBusRegisterMetaType<IwdObjects>();

  m_refresh.setSingleShot(true);
  m_refresh.setInterval(kDebounceMs);
  connect(&m_refresh, &QTimer::timeout, this, &Iwd::refresh);

  if (!m_bus.isConnected())
    return;

  m_agent = new IwdAgent(this);
  m_agentPath = QStringLiteral("/ryu/iwd/agent/%1").arg(quintptr(this));
  m_bus.registerObject(m_agentPath, m_agent, QDBusConnection::ExportAllSlots);
  connect(m_agent, &IwdAgent::requested, this, &Iwd::onAgentRequested);
  connect(m_agent, &IwdAgent::cleared, this, &Iwd::requestChanged);

  m_bus.connect(kService, QString(), kPropsIface,
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
  m_bus.connect(
      kService, QStringLiteral("/"), kObjectManager,
      QStringLiteral("InterfacesAdded"), this,
      SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString, QVariantMap>)));
  m_bus.connect(kService, QStringLiteral("/"), kObjectManager,
                QStringLiteral("InterfacesRemoved"), this,
                SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));

  m_watcher =
      new QDBusServiceWatcher(kService, m_bus,
                              QDBusServiceWatcher::WatchForRegistration |
                                  QDBusServiceWatcher::WatchForUnregistration,
                              this);
  connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this,
          [this] { onAppeared(); });
  connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this,
          [this] { onGone(); });

  if (m_bus.interface() && m_bus.interface()->isServiceRegistered(kService))
    onAppeared();
}

Iwd::~Iwd() {
  if (m_available && m_bus.isConnected()) {
    QDBusMessage m = QDBusMessage::createMethodCall(
        kService, kRoot, kAgentManager, QStringLiteral("UnregisterAgent"));
    m.setArguments({QVariant::fromValue(QDBusObjectPath(m_agentPath))});
    m_bus.call(m, QDBus::Block, 500);
  }
  if (m_bus.isConnected())
    m_bus.unregisterObject(m_agentPath);
}

bool Iwd::requestPending() const { return m_agent && m_agent->pending(); }

QString Iwd::requestKind() const {
  return m_agent ? m_agent->kind() : QString();
}

QString Iwd::requestUser() const {
  return m_agent ? m_agent->user() : QString();
}

QString Iwd::requestNetwork() const {
  if (!m_agent || !m_agent->pending())
    return QString();
  const QString p = m_agent->networkPath();
  return m_names.value(p, p.section(QLatin1Char('/'), -1));
}

void Iwd::send(const QDBusMessage &m, int timeoutMs,
               std::function<void(const QDBusMessage &)> done) {
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(m, timeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [done = std::move(done)](QDBusPendingCallWatcher *watcher) {
            const QDBusMessage r = watcher->reply();
            watcher->deleteLater();
            if (done)
              done(r);
          });
}

void Iwd::call(const QString &path, const QString &iface, const QString &method,
               const QVariantList &args,
               std::function<void(const QDBusMessage &)> done) {
  if (!m_bus.isConnected())
    return;
  QDBusMessage m =
      QDBusMessage::createMethodCall(kService, path, iface, method);
  m.setArguments(args);
  send(m, kCallTimeoutMs,
       [this, done = std::move(done)](const QDBusMessage &r) {
         if (r.type() == QDBusMessage::ErrorMessage) {
           reportError(r);
           return;
         }
         if (done)
           done(r);
       });
}

void Iwd::reportError(const QDBusMessage &r) {
  if (!quietError(r))
    emit error(messageOf(r));
}

void Iwd::onAppeared() {
  if (!m_available) {
    m_available = true;
    emit availableChanged();
  }
  registerAgent();
  refresh();
}

void Iwd::onGone() {
  if (m_agent)
    m_agent->Release();
  m_available = false;
  m_powered = false;
  m_scanning = false;
  m_state = QStringLiteral("unavailable");
  m_deviceName.clear();
  m_connectedSsid.clear();
  m_device.clear();
  m_networks.clear();
  m_known.clear();
  m_names.clear();
  emit availableChanged();
  emit stateChanged();
  emit networksChanged();
  emit knownNetworksChanged();
}

void Iwd::registerAgent() {
  call(kRoot, kAgentManager, QStringLiteral("RegisterAgent"),
       {QVariant::fromValue(QDBusObjectPath(m_agentPath))});
}

void Iwd::onPropertiesChanged(const QString &, const QVariantMap &,
                              const QStringList &) {
  m_refresh.start();
}

void Iwd::onInterfacesAdded(const QDBusObjectPath &,
                            const QMap<QString, QVariantMap> &) {
  m_refresh.start();
}

void Iwd::onInterfacesRemoved(const QDBusObjectPath &, const QStringList &) {
  m_refresh.start();
}

void Iwd::refresh() {
  if (!m_available)
    return;
  call(QStringLiteral("/"), kObjectManager, QStringLiteral("GetManagedObjects"),
       {}, [this](const QDBusMessage &r) {
         const QDBusArgument arg =
             r.arguments().value(0).value<QDBusArgument>();
         applyObjects(qdbus_cast<IwdObjects>(arg));
       });
}

void Iwd::applyObjects(const IwdObjects &objs) {
  QString dev;
  for (auto it = objs.cbegin(); it != objs.cend(); ++it) {
    if (!it.value().contains(kDeviceIface))
      continue;
    const QString p = it.key().path();
    if (dev.isEmpty() || p == m_device)
      dev = p;
    if (p == m_device)
      break;
  }
  m_device = dev;

  const IwdInterfaces di = objs.value(QDBusObjectPath(dev));
  const QVariantMap dp = di.value(kDeviceIface);
  const QVariantMap sp = di.value(kStationIface);
  const bool hasStation = !sp.isEmpty();

  QHash<QString, QString> names;
  QVariantList known;
  QHash<QString, QVariantMap> knownByPath;

  for (auto it = objs.cbegin(); it != objs.cend(); ++it) {
    if (!it.value().contains(kKnownIface))
      continue;
    const QVariantMap p = it.value().value(kKnownIface);
    const QVariantMap k{
        {"path", it.key().path()},
        {"name", p.value("Name").toString()},
        {"type", p.value("Type").toString()},
        {"hidden", p.value("Hidden").toBool()},
        {"autoConnect", p.value("AutoConnect").toBool()},
        {"lastConnected", p.value("LastConnectedTime").toString()}};
    known.append(k);
    knownByPath.insert(it.key().path(), k);
  }
  std::sort(known.begin(), known.end(),
            [](const QVariant &a, const QVariant &b) {
              return a.toMap().value("name").toString().toLower() <
                     b.toMap().value("name").toString().toLower();
            });

  QHash<QString, QVariantMap> nets;
  for (auto it = objs.cbegin(); it != objs.cend(); ++it) {
    if (!it.value().contains(kNetworkIface))
      continue;
    const QString path = it.key().path();
    const QVariantMap p = it.value().value(kNetworkIface);
    const QString name = p.value("Name").toString();
    names.insert(path, name);
    if (pathOf(p.value("Device")) != dev)
      continue;
    const QString type = p.value("Type").toString();
    const QString kp = pathOf(p.value("KnownNetwork"));
    nets.insert(
        path, QVariantMap{{"path", path},
                          {"name", name},
                          {"type", type},
                          {"secured", type != QLatin1String("open")},
                          {"connected", p.value("Connected").toBool()},
                          {"known", !kp.isEmpty()},
                          {"knownPath", kp},
                          {"autoConnect",
                           knownByPath.value(kp).value("autoConnect").toBool()},
                          {"signal", 0},
                          {"strength", 0}});
  }

  const bool powered = dp.value("Powered").toBool();
  const QString name = dp.value("Name").toString();
  const bool scanning = sp.value("Scanning").toBool();
  QString state = QStringLiteral("unavailable");
  if (!dev.isEmpty())
    state = hasStation ? sp.value("State").toString() : QStringLiteral("off");
  const QString connected = names.value(pathOf(sp.value("ConnectedNetwork")));

  bool dirty = false;
  auto upd = [&dirty](auto &field, const auto &v) {
    if (field != v) {
      field = v;
      dirty = true;
    }
  };
  upd(m_powered, powered);
  upd(m_scanning, scanning);
  upd(m_state, state);
  upd(m_deviceName, name);
  upd(m_connectedSsid, connected);
  m_names = names;
  if (dirty)
    emit stateChanged();

  if (m_known != known) {
    m_known = known;
    emit knownNetworksChanged();
  }

  if (!hasStation) {
    if (!m_networks.isEmpty()) {
      m_networks.clear();
      emit networksChanged();
    }
    return;
  }

  call(dev, kStationIface, QStringLiteral("GetOrderedNetworks"), {},
       [this, nets](const QDBusMessage &r) { applyOrdered(r, nets); });
}

void Iwd::applyOrdered(const QDBusMessage &r,
                       const QHash<QString, QVariantMap> &nets) {
  QVariantList list;
  const QDBusArgument arg = r.arguments().value(0).value<QDBusArgument>();
  arg.beginArray();
  while (!arg.atEnd()) {
    QDBusObjectPath p;
    qint16 raw = 0;
    arg.beginStructure();
    arg >> p >> raw;
    arg.endStructure();
    auto it = nets.constFind(p.path());
    if (it == nets.constEnd())
      continue;
    QVariantMap n = it.value();
    const double dbm = raw / 100.0;
    n["signal"] = dbm;
    n["strength"] =
        int(std::lround(std::clamp((dbm + 90.0) * 2.5, 0.0, 100.0)));
    list.append(n);
  }
  arg.endArray();

  if (m_networks != list) {
    m_networks = list;
    emit networksChanged();
  }
}

void Iwd::setPowered(bool on) {
  if (m_device.isEmpty())
    return;
  call(m_device, kPropsIface, QStringLiteral("Set"),
       {kDeviceIface, QStringLiteral("Powered"),
        QVariant::fromValue(QDBusVariant(on))});
}

void Iwd::scan() {
  if (m_device.isEmpty())
    return;
  call(m_device, kStationIface, QStringLiteral("Scan"));
}

void Iwd::connectToNetwork(const QString &path, const QString &passphrase,
                           const QString &user) {
  if (!m_bus.isConnected() || path.isEmpty())
    return;
  m_secret = passphrase;
  m_secretUser = user;
  const QDBusMessage m = QDBusMessage::createMethodCall(
      kService, path, kNetworkIface, QStringLiteral("Connect"));
  send(m, kConnectTimeoutMs, [this, path](const QDBusMessage &r) {
    m_secret.clear();
    m_secretUser.clear();
    const bool ok = r.type() != QDBusMessage::ErrorMessage;
    emit connectResult(path, ok, ok ? QString() : messageOf(r));
    m_refresh.start();
  });
}

void Iwd::connectToSsid(const QString &ssid, const QString &passphrase,
                        const QString &user) {
  for (const QVariant &v : std::as_const(m_networks)) {
    const QVariantMap n = v.toMap();
    if (n.value("name").toString() == ssid) {
      connectToNetwork(n.value("path").toString(), passphrase, user);
      return;
    }
  }
  emit error(QStringLiteral("network not found: ") + ssid);
}

void Iwd::connectHidden(const QString &ssid, const QString &passphrase) {
  if (m_device.isEmpty() || ssid.isEmpty())
    return;
  m_secret = passphrase;
  m_secretUser.clear();
  QDBusMessage m =
      QDBusMessage::createMethodCall(kService, m_device, kStationIface,
                                     QStringLiteral("ConnectHiddenNetwork"));
  m.setArguments({ssid});
  send(m, kConnectTimeoutMs, [this, ssid](const QDBusMessage &r) {
    m_secret.clear();
    const bool ok = r.type() != QDBusMessage::ErrorMessage;
    emit connectResult(ssid, ok, ok ? QString() : messageOf(r));
    m_refresh.start();
  });
}

void Iwd::disconnectNetwork() {
  if (m_device.isEmpty())
    return;
  call(m_device, kStationIface, QStringLiteral("Disconnect"));
}

QString Iwd::knownPathFor(const QString &path) const {
  for (const QVariant &v : m_networks) {
    const QVariantMap n = v.toMap();
    if (n.value("path").toString() == path)
      return n.value("knownPath").toString();
  }
  return path;
}

void Iwd::forget(const QString &path) {
  const QString kp = knownPathFor(path);
  if (kp.isEmpty())
    return;
  call(kp, kKnownIface, QStringLiteral("Forget"));
}

void Iwd::setAutoConnect(const QString &path, bool on) {
  const QString kp = knownPathFor(path);
  if (kp.isEmpty())
    return;
  call(kp, kPropsIface, QStringLiteral("Set"),
       {kKnownIface, QStringLiteral("AutoConnect"),
        QVariant::fromValue(QDBusVariant(on))});
}

void Iwd::onAgentRequested() {
  if (!m_agent)
    return;
  const QString kind = m_agent->kind();
  if (kind == QLatin1String("userAndPassword") && !m_secret.isEmpty() &&
      !m_secretUser.isEmpty()) {
    const QString u = m_secretUser, s = m_secret;
    m_secret.clear();
    m_secretUser.clear();
    m_agent->submit(u, s);
    return;
  }
  if (kind != QLatin1String("userAndPassword") && !m_secret.isEmpty()) {
    const QString s = m_secret;
    m_secret.clear();
    m_agent->submit(m_agent->user(), s);
    return;
  }
  emit requestChanged();
}

void Iwd::submitPassphrase(const QString &passphrase) {
  if (m_agent)
    m_agent->submit(m_agent->user(), passphrase);
}

void Iwd::submitCredentials(const QString &user, const QString &password) {
  if (m_agent)
    m_agent->submit(user, password);
}

void Iwd::cancelRequest() {
  if (m_agent)
    m_agent->abort();
}
