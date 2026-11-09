#include "./upower.hpp"
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <algorithm>

namespace {
const QString kService = QStringLiteral("org.freedesktop.UPower");
const QString kDevicePath =
    QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice");
const QString kDeviceIface = QStringLiteral("org.freedesktop.UPower.Device");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
constexpr int kTimeoutMs = 5000;
} // namespace

UPower::UPower(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::systemBus()) {
  if (!m_bus.isConnected())
    return;

  m_bus.connect(kService, kDevicePath, kProps,
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

  m_watcher =
      new QDBusServiceWatcher(kService, m_bus,
                              QDBusServiceWatcher::WatchForRegistration |
                                  QDBusServiceWatcher::WatchForUnregistration,
                              this);
  connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this,
          [this] { load(); });
  connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this,
          [this] { clear(); });

  load();
}

void UPower::load() {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      kService, kDevicePath, kProps, QStringLiteral("GetAll"));
  msg << kDeviceIface;
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kTimeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QVariantMap> r = *watcher;
            watcher->deleteLater();
            if (r.isError()) {
              clear();
              return;
            }
            apply(r.value());
            setAvailable(true);
          });
}

void UPower::onPropertiesChanged(const QString &iface,
                                 const QVariantMap &changed,
                                 const QStringList &invalidated) {
  if (iface != kDeviceIface)
    return;
  apply(changed);
  if (!invalidated.isEmpty())
    load();
}

void UPower::apply(const QVariantMap &m) {
  bool present = false, pct = false, state = false, times = false;
  auto upd = [](auto &field, const auto &v) {
    if (field == v)
      return false;
    field = v;
    return true;
  };

  if (auto it = m.constFind(QStringLiteral("IsPresent")); it != m.cend())
    present = upd(m_present, it->toBool());

  if (auto it = m.constFind(QStringLiteral("Percentage")); it != m.cend())
    pct = upd(m_percentage, std::clamp(it->toDouble() / 100.0, 0.0, 1.0));

  if (auto it = m.constFind(QStringLiteral("State")); it != m.cend())
    state = upd(m_state, State(it->toUInt()));

  if (auto it = m.constFind(QStringLiteral("TimeToEmpty")); it != m.cend())
    times |= upd(m_timeToEmpty, qint64(it->toLongLong()));

  if (auto it = m.constFind(QStringLiteral("TimeToFull")); it != m.cend())
    times |= upd(m_timeToFull, qint64(it->toLongLong()));

  if (present)
    emit presentChanged();
  if (pct)
    emit percentageChanged();
  if (state)
    emit stateChanged();
  if (times)
    emit timesChanged();
}

void UPower::clear() {
  const bool present = m_present, pct = m_percentage != 0.0,
             state = m_state != Unknown,
             times = m_timeToEmpty != 0 || m_timeToFull != 0;
  m_present = false;
  m_percentage = 0.0;
  m_state = Unknown;
  m_timeToEmpty = 0;
  m_timeToFull = 0;
  setAvailable(false);
  if (present)
    emit presentChanged();
  if (pct)
    emit percentageChanged();
  if (state)
    emit stateChanged();
  if (times)
    emit timesChanged();
}

void UPower::setAvailable(bool v) {
  if (m_available == v)
    return;
  m_available = v;
  emit availableChanged();
}
