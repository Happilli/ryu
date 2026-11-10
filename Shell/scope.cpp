#include "./scope.hpp"

Scope::Scope(QObject *parent) : QObject(parent) {}

QQmlListProperty<QObject> Scope::contents() {
  return QQmlListProperty<QObject>(this, nullptr, &Scope::appendContent,
                                   &Scope::contentCount, &Scope::contentAt,
                                   &Scope::clearContents);
}

void Scope::appendContent(QQmlListProperty<QObject> *list, QObject *object) {
  static_cast<Scope *>(list->object)->m_contents.append(object);
}

qsizetype Scope::contentCount(QQmlListProperty<QObject> *list) {
  return static_cast<Scope *>(list->object)->m_contents.size();
}

QObject *Scope::contentAt(QQmlListProperty<QObject> *list, qsizetype index) {
  return static_cast<Scope *>(list->object)->m_contents.at(index);
}

void Scope::clearContents(QQmlListProperty<QObject> *list) {
  static_cast<Scope *>(list->object)->m_contents.clear();
}
