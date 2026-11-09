#include "./process.hpp"

#include <signal.h>
#include <sys/types.h>
#include <utility>

QProcessEnvironment ioEnvironment(bool clear, const QVariantMap &overrides) {
  QProcessEnvironment env =
      clear ? QProcessEnvironment() : QProcessEnvironment::systemEnvironment();
  for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) {
    if (!it.value().isValid() || it.value().isNull()) {
      env.remove(it.key());
    } else {
      env.insert(it.key(), it.value().toString());
    }
  }
  return env;
}

SplitParser::SplitParser(QObject *parent) : QObject(parent) {}

void SplitParser::setSplitMarker(const QString &marker) {
  if (m_marker == marker) {
    return;
  }
  m_marker = marker;
  emit splitMarkerChanged();
}

void SplitParser::feed(const QByteArray &bytes) {
  if (bytes.isEmpty()) {
    return;
  }
  m_buffer.append(bytes);
  const QByteArray marker = m_marker.toUtf8();
  if (marker.isEmpty()) {
    const QByteArray all = std::exchange(m_buffer, QByteArray());
    emit read(QString::fromUtf8(all));
    return;
  }
  qsizetype start = 0;
  qsizetype index = 0;
  while ((index = m_buffer.indexOf(marker, start)) >= 0) {
    const QByteArray line = m_buffer.mid(start, index - start);
    start = index + marker.size();
    emit read(QString::fromUtf8(line));
  }
  m_buffer.remove(0, start);
}

void SplitParser::flush() {
  if (m_buffer.isEmpty()) {
    return;
  }
  const QByteArray rest = std::exchange(m_buffer, QByteArray());
  emit read(QString::fromUtf8(rest));
}

Process::Process(QObject *parent) : QObject(parent) {}

Process::~Process() {
  if (!m_proc) {
    return;
  }
  QProcess *proc = m_proc;
  disconnect(proc, nullptr, this, nullptr);
  proc->setParent(nullptr);
  connect(proc, &QProcess::finished, proc, &QObject::deleteLater);
  connect(proc, &QProcess::errorOccurred, proc, &QObject::deleteLater);
  proc->terminate();
}

void Process::componentComplete() {
  m_complete = true;
  if (m_pendingStart) {
    m_pendingStart = false;
    start();
  }
}

void Process::setRunning(bool on) {
  if (on) {
    if (m_running) {
      return;
    }
    if (!m_complete) {
      m_pendingStart = true;
      return;
    }
    start();
    return;
  }
  m_pendingStart = false;
  if (m_proc) {
    m_proc->terminate();
  }
}

void Process::setCommand(const QStringList &command) {
  if (m_command == command) {
    return;
  }
  m_command = command;
  emit commandChanged();
}

void Process::setWorkingDirectory(const QString &dir) {
  if (m_workingDirectory == dir) {
    return;
  }
  m_workingDirectory = dir;
  emit workingDirectoryChanged();
}

void Process::setEnvironment(const QVariantMap &env) {
  if (m_environment == env) {
    return;
  }
  m_environment = env;
  emit environmentChanged();
}

void Process::setClearEnvironment(bool on) {
  if (m_clearEnvironment == on) {
    return;
  }
  m_clearEnvironment = on;
  emit clearEnvironmentChanged();
}

void Process::setStdinEnabled(bool on) {
  if (m_stdinEnabled == on) {
    return;
  }
  m_stdinEnabled = on;
  emit stdinEnabledChanged();
}

void Process::setStdoutParser(SplitParser *parser) {
  if (m_stdout == parser) {
    return;
  }
  m_stdout = parser;
  emit stdoutChanged();
}

void Process::setStderrParser(SplitParser *parser) {
  if (m_stderr == parser) {
    return;
  }
  m_stderr = parser;
  emit stderrChanged();
}

void Process::signal(int sig) {
  if (!m_proc) {
    return;
  }
  const qint64 pid = m_proc->processId();
  if (pid > 0) {
    ::kill(static_cast<pid_t>(pid), sig);
  }
}

void Process::write(const QString &data) {
  if (m_proc && m_stdinEnabled) {
    m_proc->write(data.toUtf8());
  }
}

void Process::start() {
  if (m_command.isEmpty() || m_command.first().isEmpty()) {
    emit failed(QStringLiteral("Process has no command"));
    return;
  }

  auto *proc = new QProcess(this);
  m_proc = proc;
  proc->setProgram(m_command.first());
  proc->setArguments(m_command.mid(1));
  proc->setProcessEnvironment(ioEnvironment(m_clearEnvironment, m_environment));
  if (!m_workingDirectory.isEmpty()) {
    proc->setWorkingDirectory(m_workingDirectory);
  }
  proc->setProcessChannelMode(m_stderr ? QProcess::SeparateChannels
                                       : QProcess::ForwardedErrorChannel);
  if (!m_stdout) {
    proc->setStandardOutputFile(QProcess::nullDevice());
  }
  if (!m_stdinEnabled) {
    proc->setStandardInputFile(QProcess::nullDevice());
  }

  connect(proc, &QProcess::started, this, [this, proc]() {
    if (m_proc == proc) {
      emit started();
    }
  });
  connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
    if (m_stdout) {
      m_stdout->feed(proc->readAllStandardOutput());
    }
  });
  connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
    if (m_stderr) {
      m_stderr->feed(proc->readAllStandardError());
    }
  });
  connect(proc, &QProcess::errorOccurred, this,
          [this, proc](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart) {
              return;
            }
            const QString message = proc->errorString();
            const bool current = m_proc == proc;
            proc->deleteLater();
            if (!current) {
              return;
            }
            m_proc = nullptr;
            m_running = false;
            emit runningChanged();
            emit failed(message);
          });
  connect(proc, &QProcess::finished, this,
          [this, proc](int code, QProcess::ExitStatus status) {
            if (m_stdout) {
              m_stdout->feed(proc->readAllStandardOutput());
              m_stdout->flush();
            }
            if (m_stderr) {
              m_stderr->feed(proc->readAllStandardError());
              m_stderr->flush();
            }
            const bool current = m_proc == proc;
            proc->deleteLater();
            if (!current) {
              return;
            }
            m_proc = nullptr;
            m_running = false;
            emit runningChanged();
            emit exited(code, static_cast<int>(status));
          });

  m_running = true;
  emit runningChanged();
  proc->start();
}
