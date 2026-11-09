#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QString>
#include <QStringList>
#include <QVariantMap>

QProcessEnvironment ioEnvironment(bool clear, const QVariantMap &overrides);

class SplitParser : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QString splitMarker READ splitMarker WRITE setSplitMarker NOTIFY
                 splitMarkerChanged)

public:
  explicit SplitParser(QObject *parent = nullptr);

  QString splitMarker() const { return m_marker; }
  void setSplitMarker(const QString &marker);

  void feed(const QByteArray &bytes);
  void flush();

signals:
  void read(const QString &data);
  void splitMarkerChanged();

private:
  QString m_marker = QStringLiteral("\n");
  QByteArray m_buffer;
};

class Process : public QObject, public QQmlParserStatus {
  Q_OBJECT
  Q_INTERFACES(QQmlParserStatus)
  QML_ELEMENT
  Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
  Q_PROPERTY(
      QStringList command READ command WRITE setCommand NOTIFY commandChanged)
  Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE
                 setWorkingDirectory NOTIFY workingDirectoryChanged)
  Q_PROPERTY(QVariantMap environment READ environment WRITE setEnvironment
                 NOTIFY environmentChanged)
  Q_PROPERTY(bool clearEnvironment READ clearEnvironment WRITE
                 setClearEnvironment NOTIFY clearEnvironmentChanged)
  Q_PROPERTY(bool stdinEnabled READ stdinEnabled WRITE setStdinEnabled NOTIFY
                 stdinEnabledChanged)
  Q_PROPERTY(SplitParser *stdout READ stdoutParser WRITE setStdoutParser NOTIFY
                 stdoutChanged)
  Q_PROPERTY(SplitParser *stderr READ stderrParser WRITE setStderrParser NOTIFY
                 stderrChanged)

public:
  explicit Process(QObject *parent = nullptr);
  ~Process() override;

  void classBegin() override {}
  void componentComplete() override;

  bool running() const { return m_running; }
  void setRunning(bool on);
  QStringList command() const { return m_command; }
  void setCommand(const QStringList &command);
  QString workingDirectory() const { return m_workingDirectory; }
  void setWorkingDirectory(const QString &dir);
  QVariantMap environment() const { return m_environment; }
  void setEnvironment(const QVariantMap &env);
  bool clearEnvironment() const { return m_clearEnvironment; }
  void setClearEnvironment(bool on);
  bool stdinEnabled() const { return m_stdinEnabled; }
  void setStdinEnabled(bool on);
  SplitParser *stdoutParser() const { return m_stdout; }
  void setStdoutParser(SplitParser *parser);
  SplitParser *stderrParser() const { return m_stderr; }
  void setStderrParser(SplitParser *parser);

  Q_INVOKABLE void signal(int sig);
  Q_INVOKABLE void write(const QString &data);

signals:
  void runningChanged();
  void commandChanged();
  void workingDirectoryChanged();
  void environmentChanged();
  void clearEnvironmentChanged();
  void stdinEnabledChanged();
  void stdoutChanged();
  void stderrChanged();
  void started();
  void exited(int exitCode, int exitStatus);
  void failed(const QString &message);

private:
  void start();

  QStringList m_command;
  QString m_workingDirectory;
  QVariantMap m_environment;
  QPointer<SplitParser> m_stdout;
  QPointer<SplitParser> m_stderr;
  QPointer<QProcess> m_proc;
  bool m_clearEnvironment = false;
  bool m_stdinEnabled = false;
  bool m_running = false;
  bool m_pendingStart = false;
  bool m_complete = false;
};
