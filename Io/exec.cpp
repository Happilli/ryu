#include "./exec.hpp"

#include "./process.hpp"

#include <QMetaType>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>

Exec::Exec(QObject *parent) : QObject(parent) {}

bool Exec::detached(const QVariant &spec) {
  QStringList command;
  QVariantMap options;
  if (spec.typeId() == QMetaType::QVariantMap) {
    options = spec.toMap();
    command = options.value(QStringLiteral("command")).toStringList();
  } else {
    command = spec.toStringList();
  }
  if (command.isEmpty() || command.first().isEmpty()) {
    return false;
  }

  QProcess proc;
  proc.setProgram(command.first());
  proc.setArguments(command.mid(1));
  proc.setProcessEnvironment(ioEnvironment(
      false, options.value(QStringLiteral("environment")).toMap()));
  const QString dir =
      options.value(QStringLiteral("workingDirectory")).toString();
  if (!dir.isEmpty()) {
    proc.setWorkingDirectory(dir);
  }
  proc.setStandardInputFile(QProcess::nullDevice());
  proc.setStandardOutputFile(QProcess::nullDevice());
  proc.setStandardErrorFile(QProcess::nullDevice());
  return proc.startDetached();
}
