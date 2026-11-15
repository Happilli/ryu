#include "./env.hpp"
#include <QtGlobal>

Env::Env(QObject *parent) : QObject(parent) {}

QString Env::get(const QString &name) const {
  return qEnvironmentVariable(name.toUtf8().constData());
}
