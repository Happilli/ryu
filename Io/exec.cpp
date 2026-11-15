#include "./exec.hpp"
#include <QProcess>

Exec::Exec(QObject *parent) : QObject(parent) {}

bool Exec::detached(const QStringList &command) {
  if (command.isEmpty() || command.first().isEmpty()) {
    return false;
  }
  QProcess proc;
  proc.setProgram(command.first());
  proc.setArguments(command.mid(1));
  proc.setStandardInputFile(QProcess::nullDevice());
  proc.setStandardOutputFile(QProcess::nullDevice());
  proc.setStandardErrorFile(QProcess::nullDevice());
  return proc.startDetached();
}
