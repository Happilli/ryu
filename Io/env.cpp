#include "./env.hpp"
#include <QByteArray>
#include <QtGlobal>

Env::Env(QObject *parent) : QObject(parent) {}

QString Env::get(const QString &name) const {
  return qEnvironmentVariable(name.toUtf8().constData());
}

bool Env::has(const QString &name) const {
  return qEnvironmentVariableIsSet(name.toUtf8().constData());
}
