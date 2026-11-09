#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <memory>

struct PamSession;

class PamResult : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("PamResult only provides enum values")

public:
  enum Enum { Success = 0, Failed, Error, MaxTries, PermissionDenied };
  Q_ENUM(Enum)
};

class PamError : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("PamError only provides enum values")

public:
  enum Enum { StartFailed = 0, ConversationFailed, Unknown };
  Q_ENUM(Enum)
};

class PamContext : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString config READ config WRITE setConfig NOTIFY configChanged)
  Q_PROPERTY(QString configDirectory READ configDirectory WRITE
                 setConfigDirectory NOTIFY configDirectoryChanged)
  Q_PROPERTY(QString user READ user WRITE setUser NOTIFY userChanged)
  Q_PROPERTY(bool active READ active NOTIFY activeChanged)
  Q_PROPERTY(QString message READ message NOTIFY messageChanged)
  Q_PROPERTY(bool messageIsError READ messageIsError NOTIFY messageChanged)
  Q_PROPERTY(bool responseRequired READ responseRequired NOTIFY responseChanged)
  Q_PROPERTY(bool responseVisible READ responseVisible NOTIFY responseChanged)

public:
  explicit PamContext(QObject *parent = nullptr);
  ~PamContext() override;

  QString config() const { return m_config; }
  void setConfig(const QString &config);
  QString configDirectory() const { return m_configDirectory; }
  void setConfigDirectory(const QString &dir);
  QString user() const { return m_user; }
  void setUser(const QString &user);
  bool active() const { return m_active; }
  QString message() const { return m_message; }
  bool messageIsError() const { return m_messageIsError; }
  bool responseRequired() const { return m_responseRequired; }
  bool responseVisible() const { return m_responseVisible; }

  Q_INVOKABLE bool start();
  Q_INVOKABLE void abort();
  Q_INVOKABLE void respond(const QString &response);

  void handleMessage(quint64 generation, const QString &text, int style);
  void handleFinished(quint64 generation, int code, bool startFailed);

signals:
  void configChanged();
  void configDirectoryChanged();
  void userChanged();
  void activeChanged();
  void messageChanged();
  void responseChanged();
  void pamMessage();
  void completed(PamResult::Enum result);
  void error(PamError::Enum err);

private:
  void clearPrompt();

  std::shared_ptr<PamSession> m_session;
  QString m_config = QStringLiteral("login");
  QString m_configDirectory;
  QString m_user;
  QString m_message;
  quint64 m_generation = 0;
  bool m_active = false;
  bool m_messageIsError = false;
  bool m_responseRequired = false;
  bool m_responseVisible = false;
};
