#include "./process.hpp"

#include <signal.h>
#include <sys/types.h>

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

void Process::signal(int sig) {
  if (!m_proc) {
    return;
  }
  const qint64 pid = m_proc->processId();
  if (pid > 0) {
    ::kill(static_cast<pid_t>(pid), sig);
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
  proc->setProcessChannelMode(QProcess::ForwardedErrorChannel);
  proc->setStandardOutputFile(QProcess::nullDevice());
  proc->setStandardInputFile(QProcess::nullDevice());
  auto release = [this, proc]() {
    const bool current = m_proc == proc;
    proc->deleteLater();
    if (current) {
      m_proc = nullptr;
      m_running = false;
      emit runningChanged();
    }
    return current;
  };
  connect(proc, &QProcess::errorOccurred, this,
          [this, proc, release](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart) {
              return;
            }
            const QString message = proc->errorString();
            if (release()) {
              emit failed(message);
            }
          });
  connect(proc, &QProcess::finished, this,
          [this, release](int code, QProcess::ExitStatus status) {
            if (release()) {
              emit exited(code, static_cast<int>(status));
            }
          });
  m_running = true;
  emit runningChanged();
  proc->start();
}
