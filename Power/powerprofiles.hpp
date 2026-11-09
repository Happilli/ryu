#pragma once
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class PowerProfiles : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(
      Profile profile READ profile WRITE setProfile NOTIFY profileChanged)
  Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
  Q_PROPERTY(bool hasPerformance READ hasPerformance NOTIFY profilesChanged)

public:
  enum Profile { PowerSaver = 0, Balanced = 1, Performance = 2 };
  Q_ENUM(Profile)

  explicit PowerProfiles(QObject *parent = nullptr);

  bool available() const { return m_available; }
  Profile profile() const { return m_profile; }
  QVariantList profiles() const { return m_profiles; }
  bool hasPerformance() const { return m_profiles.contains(int(Performance)); }

  void setProfile(Profile p);

  Q_INVOKABLE void cycle();

signals:
  void availableChanged();
  void profileChanged();
  void profilesChanged();

private slots:
  void onPropertiesChanged(const QString &iface, const QVariantMap &changed,
                           const QStringList &invalidated);

private:
  void load(int index);
  void apply(const QVariantMap &m);
  void clear();
  void setAvailable(bool v);

  QDBusConnection m_bus;
  QDBusServiceWatcher *m_watcher = nullptr;

  int m_endpoint = -1;
  bool m_available = false;
  Profile m_profile = Balanced;
  QVariantList m_profiles;
};
