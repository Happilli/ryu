#include "./powerprofiles.hpp"
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <algorithm>

namespace {
struct Endpoint {
  const char *service;
  const char *path;
  const char *iface;
};

constexpr Endpoint kEndpoints[] = {{"org.freedesktop.UPower.PowerProfiles",
                                    "/org/freedesktop/UPower/PowerProfiles",
                                    "org.freedesktop.UPower.PowerProfiles"},
                                   {"net.hadess.PowerProfiles",
                                    "/net/hadess/PowerProfiles",
                                    "net.hadess.PowerProfiles"}};
constexpr int kEndpointCount = int(sizeof(kEndpoints) / sizeof(kEndpoints[0]));

const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
constexpr int kTimeoutMs = 5000;

QString str(const char *s) { return QString::fromLatin1(s); }

int parse(const QString &s) {
  if (s == QLatin1String("power-saver"))
    return PowerProfiles::PowerSaver;
  if (s == QLatin1String("balanced"))
    return PowerProfiles::Balanced;
  if (s == QLatin1String("performance"))
    return PowerProfiles::Performance;
  return -1;
}

QString nameOf(PowerProfiles::Profile p) {
  switch (p) {
  case PowerProfiles::PowerSaver:
    return QStringLiteral("power-saver");
  case PowerProfiles::Performance:
    return QStringLiteral("performance");
  case PowerProfiles::Balanced:
    break;
  }
  return QStringLiteral("balanced");
}
} // namespace

PowerProfiles::PowerProfiles(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::systemBus()) {
  qDBusRegisterMetaType<QList<QVariantMap>>();

  if (!m_bus.isConnected())
    return;

  QStringList services;
  for (const Endpoint &e : kEndpoints) {
    services << str(e.service);
    m_bus.connect(str(e.service), str(e.path), kProps,
                  QStringLiteral("PropertiesChanged"), this,
                  SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
  }

  m_watcher = new QDBusServiceWatcher(this);
  m_watcher->setConnection(m_bus);
  m_watcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration |
                          QDBusServiceWatcher::WatchForUnregistration);
  m_watcher->setWatchedServices(services);
  connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this,
          [this] { load(0); });
  connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
    const QDBusConnectionInterface *i = m_bus.interface();
    if (i)
      for (const Endpoint &e : kEndpoints)
        if (i->isServiceRegistered(str(e.service)).value())
          return;
    clear();
  });

  load(0);
}

void PowerProfiles::load(int index) {
  if (index >= kEndpointCount) {
    clear();
    return;
  }
  const Endpoint &e = kEndpoints[index];
  QDBusMessage msg = QDBusMessage::createMethodCall(
      str(e.service), str(e.path), kProps, QStringLiteral("GetAll"));
  msg << str(e.iface);
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kTimeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [this, index](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QVariantMap> r = *watcher;
            watcher->deleteLater();
            if (r.isError()) {
              load(index + 1);
              return;
            }
            m_endpoint = index;
            apply(r.value());
            setAvailable(true);
          });
}

void PowerProfiles::onPropertiesChanged(const QString &iface,
                                        const QVariantMap &changed,
                                        const QStringList &invalidated) {
  bool known = false;
  for (const Endpoint &e : kEndpoints)
    if (iface == str(e.iface))
      known = true;
  if (!known)
    return;
  apply(changed);
  if (!invalidated.isEmpty() && m_endpoint >= 0)
    load(m_endpoint);
}

void PowerProfiles::apply(const QVariantMap &m) {
  if (auto it = m.constFind(QStringLiteral("Profiles")); it != m.cend()) {
    QList<QVariantMap> maps;
    if (it->canConvert<QDBusArgument>())
      maps = qdbus_cast<QList<QVariantMap>>(it->value<QDBusArgument>());

    QList<int> ids;
    for (const QVariantMap &p : std::as_const(maps)) {
      const int id = parse(p.value(QStringLiteral("Profile")).toString());
      if (id >= 0 && !ids.contains(id))
        ids.append(id);
    }
    std::sort(ids.begin(), ids.end());

    QVariantList next;
    next.reserve(ids.size());
    for (int id : std::as_const(ids))
      next.append(id);

    if (next != m_profiles) {
      m_profiles = next;
      emit profilesChanged();
    }
  }

  if (auto it = m.constFind(QStringLiteral("ActiveProfile")); it != m.cend()) {
    const int id = parse(it->toString());
    if (id >= 0 && Profile(id) != m_profile) {
      m_profile = Profile(id);
      emit profileChanged();
    }
  }
}

void PowerProfiles::setProfile(Profile p) {
  if (m_endpoint < 0 || p == m_profile || !m_profiles.contains(int(p)))
    return;

  const Endpoint &e = kEndpoints[m_endpoint];
  QDBusMessage msg = QDBusMessage::createMethodCall(
      str(e.service), str(e.path), kProps, QStringLiteral("Set"));
  msg << str(e.iface) << QStringLiteral("ActiveProfile")
      << QVariant::fromValue(QDBusVariant(nameOf(p)));

  m_profile = p;
  emit profileChanged();

  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kTimeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *watcher) {
            const bool failed = watcher->isError();
            watcher->deleteLater();
            if (failed && m_endpoint >= 0)
              load(m_endpoint);
          });
}

void PowerProfiles::cycle() {
  if (m_profile == PowerSaver)
    setProfile(Balanced);
  else if (m_profile == Balanced && hasPerformance())
    setProfile(Performance);
  else
    setProfile(PowerSaver);
}

void PowerProfiles::clear() {
  m_endpoint = -1;
  setAvailable(false);
  if (!m_profiles.isEmpty()) {
    m_profiles.clear();
    emit profilesChanged();
  }
}

void PowerProfiles::setAvailable(bool v) {
  if (m_available == v)
    return;
  m_available = v;
  emit availableChanged();
}
