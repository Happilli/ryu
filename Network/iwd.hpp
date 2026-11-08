#pragma once
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

using IwdInterfaces = QMap<QString, QVariantMap>;
using IwdObjects = QMap<QDBusObjectPath, IwdInterfaces>;

class IwdAgent : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.Agent")

public:
  explicit IwdAgent(QObject *parent = nullptr);

  bool pending() const { return m_pending; }
  QString kind() const { return m_kind; }
  QString networkPath() const { return m_network; }
  QString user() const { return m_user; }

  void submit(const QString &user, const QString &secret);
  void abort();

public slots:
  void Release();
  void Cancel(const QString &reason);
  void RequestPassphrase(const QDBusObjectPath &network,
                         const QDBusMessage &message);
  void RequestPrivateKeyPassphrase(const QDBusObjectPath &network,
                                   const QDBusMessage &message);
  void RequestUserNameAndPassword(const QDBusObjectPath &network,
                                  const QDBusMessage &message);
  void RequestUserPassword(const QDBusObjectPath &network, const QString &user,
                           const QDBusMessage &message);

signals:
  void requested();
  void cleared();

private:
  void begin(const QString &kind, const QDBusObjectPath &network,
             const QString &user, const QDBusMessage &message);
  void reset();

  bool m_pending = false;
  QString m_kind;
  QString m_network;
  QString m_user;
  QDBusMessage m_message;
};

class Iwd : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(bool powered READ powered WRITE setPowered NOTIFY stateChanged)
  Q_PROPERTY(bool scanning READ scanning NOTIFY stateChanged)
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString deviceName READ deviceName NOTIFY stateChanged)
  Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY stateChanged)
  Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
  Q_PROPERTY(
      QVariantList knownNetworks READ knownNetworks NOTIFY knownNetworksChanged)
  Q_PROPERTY(bool requestPending READ requestPending NOTIFY requestChanged)
  Q_PROPERTY(QString requestKind READ requestKind NOTIFY requestChanged)
  Q_PROPERTY(QString requestNetwork READ requestNetwork NOTIFY requestChanged)
  Q_PROPERTY(QString requestUser READ requestUser NOTIFY requestChanged)

public:
  explicit Iwd(QObject *parent = nullptr);
  ~Iwd() override;

  bool available() const { return m_available; }
  bool powered() const { return m_powered; }
  bool scanning() const { return m_scanning; }
  QString state() const { return m_state; }
  QString deviceName() const { return m_deviceName; }
  QString connectedSsid() const { return m_connectedSsid; }
  QVariantList networks() const { return m_networks; }
  QVariantList knownNetworks() const { return m_known; }
  bool requestPending() const;
  QString requestKind() const;
  QString requestNetwork() const;
  QString requestUser() const;

  void setPowered(bool on);

  Q_INVOKABLE void refresh();
  Q_INVOKABLE void scan();
  Q_INVOKABLE void connectToNetwork(const QString &path,
                                    const QString &passphrase = QString(),
                                    const QString &user = QString());
  Q_INVOKABLE void connectToSsid(const QString &ssid,
                                 const QString &passphrase = QString(),
                                 const QString &user = QString());
  Q_INVOKABLE void connectHidden(const QString &ssid,
                                 const QString &passphrase = QString());
  Q_INVOKABLE void disconnectNetwork();
  Q_INVOKABLE void forget(const QString &path);
  Q_INVOKABLE void setAutoConnect(const QString &path, bool on);
  Q_INVOKABLE void submitPassphrase(const QString &passphrase);
  Q_INVOKABLE void submitCredentials(const QString &user,
                                     const QString &password);
  Q_INVOKABLE void cancelRequest();

signals:
  void availableChanged();
  void stateChanged();
  void networksChanged();
  void knownNetworksChanged();
  void requestChanged();
  void connectResult(const QString &path, bool ok, const QString &message);
  void error(const QString &message);

private slots:
  void onPropertiesChanged(const QString &, const QVariantMap &,
                           const QStringList &);
  void onInterfacesAdded(const QDBusObjectPath &,
                         const QMap<QString, QVariantMap> &);
  void onInterfacesRemoved(const QDBusObjectPath &, const QStringList &);

private:
  void send(const QDBusMessage &m, int timeoutMs,
            std::function<void(const QDBusMessage &)> done);
  void call(const QString &path, const QString &iface, const QString &method,
            const QVariantList &args = {},
            std::function<void(const QDBusMessage &)> done = nullptr);
  void reportError(const QDBusMessage &r);
  void onAppeared();
  void onGone();
  void registerAgent();
  void applyObjects(const IwdObjects &objs);
  void applyOrdered(const QDBusMessage &r,
                    const QHash<QString, QVariantMap> &nets);
  void onAgentRequested();
  QString knownPathFor(const QString &path) const;

  QDBusConnection m_bus;
  QDBusServiceWatcher *m_watcher = nullptr;
  IwdAgent *m_agent = nullptr;
  QString m_agentPath;
  QTimer m_refresh;

  bool m_available = false;
  bool m_powered = false;
  bool m_scanning = false;
  QString m_state = QStringLiteral("unavailable");
  QString m_deviceName;
  QString m_connectedSsid;
  QString m_device;
  QVariantList m_networks;
  QVariantList m_known;
  QHash<QString, QString> m_names;

  QString m_secret;
  QString m_secretUser;
};
