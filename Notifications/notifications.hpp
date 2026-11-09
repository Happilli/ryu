#pragma once

#include <QDBusAbstractAdaptor>
#include <QList>
#include <QObject>
#include <QPair>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

class Notification;
class NotificationServer;
class NotificationsAdaptor;

struct NotificationData {
  QString appName;
  QString appIcon;
  QString summary;
  QString body;
  QString image;
  QString desktopEntry;
  QString replyPlaceholder;
  int urgency = 1;
  int expireTimeout = -1;
  bool transient = false;
  bool resident = false;
  bool hasInlineReply = false;
  QList<QPair<QString, QString>> actions;
};

class NotificationAction : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(QString identifier READ identifier CONSTANT)
  Q_PROPERTY(QString text READ text CONSTANT)

public:
  NotificationAction(Notification *notification, const QString &identifier,
                     const QString &text);

  QString identifier() const { return m_identifier; }
  QString text() const { return m_text; }

  Q_INVOKABLE void invoke();

private:
  Notification *m_notification;
  QString m_identifier;
  QString m_text;
};

class Notification : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Notifications are created by NotificationServer")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(bool tracked READ tracked WRITE setTracked NOTIFY trackedChanged)
  Q_PROPERTY(QString appName READ appName NOTIFY contentChanged)
  Q_PROPERTY(QString appIcon READ appIcon NOTIFY contentChanged)
  Q_PROPERTY(QString summary READ summary NOTIFY contentChanged)
  Q_PROPERTY(QString body READ body NOTIFY contentChanged)
  Q_PROPERTY(QString image READ image NOTIFY contentChanged)
  Q_PROPERTY(Urgency urgency READ urgency NOTIFY urgencyChanged)
  Q_PROPERTY(int expireTimeout READ expireTimeout NOTIFY expireTimeoutChanged)
  Q_PROPERTY(QString desktopEntry READ desktopEntry NOTIFY metaChanged)
  Q_PROPERTY(bool transient READ transient NOTIFY metaChanged)
  Q_PROPERTY(bool resident READ resident NOTIFY metaChanged)
  Q_PROPERTY(bool hasInlineReply READ hasInlineReply NOTIFY metaChanged)
  Q_PROPERTY(QString inlineReplyPlaceholder READ inlineReplyPlaceholder NOTIFY
                 metaChanged)
  Q_PROPERTY(QVariantList actions READ actions NOTIFY actionsChanged)

public:
  enum Urgency { Low = 0, Normal = 1, Critical = 2 };
  Q_ENUM(Urgency)

  enum CloseReason { Expired = 1, Dismissed = 2, Closed = 3, Undefined = 4 };
  Q_ENUM(CloseReason)

  Notification(NotificationServer *server, int id);
  ~Notification() override;

  int id() const { return m_id; }
  bool tracked() const { return m_tracked; }
  void setTracked(bool on);
  QString appName() const { return m_appName; }
  QString appIcon() const { return m_appIcon; }
  QString summary() const { return m_summary; }
  QString body() const { return m_body; }
  QString image() const { return m_image; }
  Urgency urgency() const { return m_urgency; }
  int expireTimeout() const { return m_expireTimeout; }
  QString desktopEntry() const { return m_desktopEntry; }
  bool transient() const { return m_transient; }
  bool resident() const { return m_resident; }
  bool hasInlineReply() const { return m_hasInlineReply; }
  QString inlineReplyPlaceholder() const { return m_replyPlaceholder; }
  QVariantList actions() const;

  Q_INVOKABLE void dismiss();
  Q_INVOKABLE void expire();
  Q_INVOKABLE void sendInlineReply(const QString &text);

  void invokeAction(const QString &identifier);
  void assign(const NotificationData &data, const QString &imageKey);

signals:
  void trackedChanged();
  void contentChanged();
  void urgencyChanged();
  void expireTimeoutChanged();
  void metaChanged();
  void actionsChanged();
  void closed(int reason);

private:
  friend class NotificationServer;

  NotificationServer *m_server;
  int m_id;
  bool m_tracked = false;
  bool m_announced = false;
  bool m_closing = false;
  QString m_appName;
  QString m_appIcon;
  QString m_summary;
  QString m_body;
  QString m_image;
  QString m_imageKey;
  QString m_desktopEntry;
  QString m_replyPlaceholder;
  Urgency m_urgency = Normal;
  int m_expireTimeout = -1;
  bool m_transient = false;
  bool m_resident = false;
  bool m_hasInlineReply = false;
  QList<NotificationAction *> m_actions;
};

class NotificationIcons : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit NotificationIcons(QObject *parent = nullptr);

  static NotificationIcons *create(QQmlEngine *engine, QJSEngine *scriptEngine);

  Q_INVOKABLE QString resolve(const QString &name) const;
};

class NotificationServer : public QObject, public QQmlParserStatus {
  Q_OBJECT
  Q_INTERFACES(QQmlParserStatus)
  QML_ELEMENT
  Q_PROPERTY(bool bodySupported READ bodySupported WRITE setBodySupported NOTIFY
                 capabilitiesChanged)
  Q_PROPERTY(bool bodyMarkupSupported READ bodyMarkupSupported WRITE
                 setBodyMarkupSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool bodyHyperlinksSupported READ bodyHyperlinksSupported WRITE
                 setBodyHyperlinksSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool bodyImagesSupported READ bodyImagesSupported WRITE
                 setBodyImagesSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool actionsSupported READ actionsSupported WRITE
                 setActionsSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool actionIconsSupported READ actionIconsSupported WRITE
                 setActionIconsSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool imageSupported READ imageSupported WRITE setImageSupported
                 NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool persistenceSupported READ persistenceSupported WRITE
                 setPersistenceSupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(bool inlineReplySupported READ inlineReplySupported WRITE
                 setInlineReplySupported NOTIFY capabilitiesChanged)
  Q_PROPERTY(QStringList extraHints READ extraHints WRITE setExtraHints NOTIFY
                 capabilitiesChanged)
  Q_PROPERTY(bool keepOnReload READ keepOnReload WRITE setKeepOnReload NOTIFY
                 keepOnReloadChanged)
  Q_PROPERTY(int maxTracked READ maxTracked WRITE setMaxTracked NOTIFY
                 maxTrackedChanged)
  Q_PROPERTY(bool running READ running NOTIFY runningChanged)
  Q_PROPERTY(QVariantList trackedNotifications READ trackedNotifications NOTIFY
                 trackedNotificationsChanged)

public:
  explicit NotificationServer(QObject *parent = nullptr);
  ~NotificationServer() override;

  void classBegin() override {}
  void componentComplete() override;

  bool bodySupported() const { return m_bodySupported; }
  bool bodyMarkupSupported() const { return m_bodyMarkupSupported; }
  bool bodyHyperlinksSupported() const { return m_bodyHyperlinksSupported; }
  bool bodyImagesSupported() const { return m_bodyImagesSupported; }
  bool actionsSupported() const { return m_actionsSupported; }
  bool actionIconsSupported() const { return m_actionIconsSupported; }
  bool imageSupported() const { return m_imageSupported; }
  bool persistenceSupported() const { return m_persistenceSupported; }
  bool inlineReplySupported() const { return m_inlineReplySupported; }
  QStringList extraHints() const { return m_extraHints; }
  bool keepOnReload() const { return m_keepOnReload; }
  int maxTracked() const { return m_maxTracked; }
  bool running() const { return m_running; }
  QVariantList trackedNotifications() const;

  void setBodySupported(bool v) { assignCapability(m_bodySupported, v); }
  void setBodyMarkupSupported(bool v) {
    assignCapability(m_bodyMarkupSupported, v);
  }
  void setBodyHyperlinksSupported(bool v) {
    assignCapability(m_bodyHyperlinksSupported, v);
  }
  void setBodyImagesSupported(bool v) {
    assignCapability(m_bodyImagesSupported, v);
  }
  void setActionsSupported(bool v) { assignCapability(m_actionsSupported, v); }
  void setActionIconsSupported(bool v) {
    assignCapability(m_actionIconsSupported, v);
  }
  void setImageSupported(bool v) { assignCapability(m_imageSupported, v); }
  void setPersistenceSupported(bool v) {
    assignCapability(m_persistenceSupported, v);
  }
  void setInlineReplySupported(bool v) {
    assignCapability(m_inlineReplySupported, v);
  }
  void setExtraHints(const QStringList &hints);
  void setKeepOnReload(bool on);
  void setMaxTracked(int count);

  QStringList capabilities() const;
  uint handleNotify(const QString &appName, uint replacesId,
                    const QString &appIcon, const QString &summary,
                    const QString &body, const QStringList &actions,
                    const QVariantMap &hints, int expireTimeout);
  void handleClose(uint id);
  void closeNotification(Notification *notification,
                         Notification::CloseReason reason);
  void invokeAction(Notification *notification, const QString &identifier);
  void sendReply(Notification *notification, const QString &text);
  void syncTracked(Notification *notification);

signals:
  void notification(Notification *notification);
  void error(const QString &message);
  void capabilitiesChanged();
  void keepOnReloadChanged();
  void maxTrackedChanged();
  void runningChanged();
  void trackedNotificationsChanged();

private:
  void assignCapability(bool &field, bool value);
  void start();
  void setRunning(bool running);
  void trim();
  int allocateId();
  Notification *findTracked(int id) const;

  NotificationsAdaptor *m_adaptor = nullptr;
  QList<Notification *> m_tracked;
  QStringList m_extraHints;
  int m_nextId = 1;
  int m_maxTracked = 100;
  quint64 m_imageSerial = 0;
  bool m_bodySupported = true;
  bool m_bodyMarkupSupported = false;
  bool m_bodyHyperlinksSupported = false;
  bool m_bodyImagesSupported = false;
  bool m_actionsSupported = false;
  bool m_actionIconsSupported = false;
  bool m_imageSupported = false;
  bool m_persistenceSupported = false;
  bool m_inlineReplySupported = false;
  bool m_keepOnReload = false;
  bool m_running = false;
  bool m_registered = false;
};

class NotificationsAdaptor : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
  explicit NotificationsAdaptor(NotificationServer *server);

public slots:
  QStringList GetCapabilities();
  uint Notify(const QString &appName, uint replacesId, const QString &appIcon,
              const QString &summary, const QString &body,
              const QStringList &actions, const QVariantMap &hints,
              int expireTimeout);
  void CloseNotification(uint id);
  QString GetServerInformation(QString &vendor, QString &version,
                               QString &specVersion);

signals:
  void NotificationClosed(uint id, uint reason);
  void ActionInvoked(uint id, const QString &actionKey);
  void NotificationReplied(uint id, const QString &text);

private:
  NotificationServer *m_server;
};
