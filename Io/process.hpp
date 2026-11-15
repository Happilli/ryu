#pragma once
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QStringList>

class Process : public QObject, public QQmlParserStatus {
  Q_OBJECT
  Q_INTERFACES(QQmlParserStatus)
  QML_ELEMENT
  Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
  Q_PROPERTY(
      QStringList command READ command WRITE setCommand NOTIFY commandChanged)

public:
  explicit Process(QObject *parent = nullptr);
  ~Process() override;

  void classBegin() override {}
  void componentComplete() override;

  bool running() const { return m_running; }
  void setRunning(bool on);

  QStringList command() const { return m_command; }
  void setCommand(const QStringList &command);

  Q_INVOKABLE void signal(int sig);

signals:
  void runningChanged();
  void commandChanged();
  void exited(int exitCode, int exitStatus);
  void failed(const QString &message);

private:
  void start();

  QStringList m_command;
  QPointer<QProcess> m_proc;
  bool m_running = false;
  bool m_pendingStart = false;
  bool m_complete = false;
};
