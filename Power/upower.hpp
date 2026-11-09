#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class UPower : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(bool present READ present NOTIFY presentChanged)
  Q_PROPERTY(qreal percentage READ percentage NOTIFY percentageChanged)
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(bool charging READ charging NOTIFY stateChanged)
  Q_PROPERTY(bool fullyCharged READ fullyCharged NOTIFY stateChanged)
  Q_PROPERTY(qint64 timeToEmpty READ timeToEmpty NOTIFY timesChanged)
  Q_PROPERTY(qint64 timeToFull READ timeToFull NOTIFY timesChanged)

public:
  enum State {
    Unknown = 0,
    Charging = 1,
    Discharging = 2,
    Empty = 3,
    FullyCharged = 4,
    PendingCharge = 5,
    PendingDischarge = 6
  };
  Q_ENUM(State)

  explicit UPower(QObject *parent = nullptr);

  bool available() const { return m_available; }
  bool present() const { return m_present; }
  qreal percentage() const { return m_percentage; }
  State state() const { return m_state; }
  bool charging() const { return m_state == Charging; }
  bool fullyCharged() const { return m_state == FullyCharged; }
  qint64 timeToEmpty() const { return m_timeToEmpty; }
  qint64 timeToFull() const { return m_timeToFull; }

signals:
  void availableChanged();
  void presentChanged();
  void percentageChanged();
  void stateChanged();
  void timesChanged();

private slots:
  void onPropertiesChanged(const QString &iface, const QVariantMap &changed,
                           const QStringList &invalidated);

private:
  void load();
  void apply(const QVariantMap &m);
  void clear();
  void setAvailable(bool v);

  QDBusConnection m_bus;
  QDBusServiceWatcher *m_watcher = nullptr;

  bool m_available = false;
  bool m_present = false;
  qreal m_percentage = 0.0;
  State m_state = Unknown;
  qint64 m_timeToEmpty = 0;
  qint64 m_timeToFull = 0;
};
