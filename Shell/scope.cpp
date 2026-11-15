#include "./scope.hpp"

Scope::Scope(QObject *parent) : QObject(parent) {}

QQmlListProperty<QObject> Scope::contents() {
  return QQmlListProperty<QObject>(this, &m_contents);
}
