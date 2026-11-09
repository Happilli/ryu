#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QList>
#include <QMap>
#include <QObject>
#include <QQmlParserStatus>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <qqmlregistration.h>

class QIODevice;
class PolkitAgent;

struct PolkitIdentity {
  QString kind;
  QVariantMap details;
};
Q_DECLARE_METATYPE(PolkitIdentity)

QDBusArgument &operator<<(QDBusArgument &arg, const PolkitIdentity &value);
const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PolkitIdentity &value);

struct PolkitRequest {
  QString actionId;
  QString message;
  QString iconName;
  QString cookie;
  QMap<QString, QString> details;
  QList<PolkitIdentity> identities;
  QDBusMessage reply;
};

class PolkitFlow : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("PolkitFlow is created by PolkitAgent")
  Q_PROPERTY(QString actionId READ actionId CONSTANT)
  Q_PROPERTY(QString message READ message CONSTANT)
  Q_PROPERTY(QString iconName READ iconName CONSTANT)
  Q_PROPERTY(QString inputPrompt READ inputPrompt NOTIFY promptChanged)
  Q_PROPERTY(bool responseVisible READ responseVisible NOTIFY promptChanged)
  Q_PROPERTY(bool responseRequired READ responseRequired NOTIFY promptChanged)
  Q_PROPERTY(QString supplementaryMessage READ supplementaryMessage NOTIFY
                 supplementaryChanged)
  Q_PROPERTY(bool supplementaryIsError READ supplementaryIsError NOTIFY
                 supplementaryChanged)
  Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)

public:
  PolkitFlow(const QString &actionId, const QString &message,
             const QString &iconName, QObject *parent = nullptr);

  QString actionId() const { return m_actionId; }
  QString message() const { return m_message; }
  QString iconName() const { return m_iconName; }
  QString inputPrompt() const { return m_prompt; }
  bool responseVisible() const { return m_responseVisible; }
  bool responseRequired() const { return m_responseRequired; }
  QString supplementaryMessage() const { return m_supplementary; }
  bool supplementaryIsError() const { return m_supplementaryIsError; }
  bool failed() const { return m_failed; }

  Q_INVOKABLE void submit(const QString &response);
  Q_INVOKABLE void cancelAuthenticationRequest();

  void setPrompt(const QString &prompt, bool visible);
  void clearPrompt();
  void setSupplementary(const QString &text, bool isError);
  void setFailed(bool failed);

signals:
  void promptChanged();
  void supplementaryChanged();
  void failedChanged();
  void submitted(const QString &response);
  void cancelRequested();

private:
  QString m_actionId;
  QString m_message;
  QString m_iconName;
  QString m_prompt;
  QString m_supplementary;
  bool m_responseVisible = false;
  bool m_responseRequired = false;
  bool m_supplementaryIsError = false;
  bool m_failed = false;
};

class PolkitAgent : public QObject, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
  Q_PROPERTY(bool isActive READ isActive NOTIFY activeChanged)
  Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY registeredChanged)
  Q_PROPERTY(PolkitFlow *flow READ flow NOTIFY flowChanged)

public:
  explicit PolkitAgent(QObject *parent = nullptr);
  ~PolkitAgent() override;

  QString path() const { return m_path; }
  void setPath(const QString &path);
  bool isActive() const { return m_flow != nullptr; }
  bool isRegistered() const { return m_registered; }
  PolkitFlow *flow() const { return m_flow; }

  void classBegin() override {}
  void componentComplete() override;

  void beginAuthentication(const PolkitRequest &request);
  void cancelAuthentication(const QString &cookie);

signals:
  void pathChanged();
  void activeChanged();
  void registeredChanged();
  void flowChanged();
  void error(const QString &message);

private:
  enum class Outcome { None, Success, Failure };

  void registerAgent();
  void unregisterAgent();
  void setRegistered(bool registered);
  void startNext();
  void startHelper();
  void dropHelper();
  void readHelper();
  void handleLine(const QByteArray &line);
  void onHelperFinished();
  void respond(const QString &response);
  void finishCurrent(const QString &errorName, const QString &errorText);
  QDBusMessage replyFor(const PolkitRequest &request, const QString &errorName,
                        const QString &errorText) const;

  QString m_path = QStringLiteral("/org/ryu/PolkitAgent");
  QString m_helperPath;
  QString m_user;
  bool m_ready = false;
  bool m_registered = false;

  PolkitFlow *m_flow = nullptr;
  PolkitRequest m_current;
  bool m_hasCurrent = false;
  QList<PolkitRequest> m_queue;

  QIODevice *m_helper = nullptr;
  QByteArray m_buffer;
  Outcome m_outcome = Outcome::None;
  bool m_sawPrompt = false;
};

class PolkitAgentAdaptor : public QDBusAbstractAdaptor {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface",
              "org.freedesktop.PolicyKit1.AuthenticationAgent")

public:
  explicit PolkitAgentAdaptor(PolkitAgent *agent);

public slots:
  void BeginAuthentication(const QString &actionId, const QString &message,
                           const QString &iconName,
                           const QMap<QString, QString> &details,
                           const QString &cookie,
                           const QList<PolkitIdentity> &identities,
                           const QDBusMessage &msg);
  void CancelAuthentication(const QString &cookie);

private:
  PolkitAgent *m_agent;
};
