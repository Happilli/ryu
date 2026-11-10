#pragma once

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>

class Scope : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("DefaultProperty", "contents")
  Q_PROPERTY(QQmlListProperty<QObject> contents READ contents)

public:
  explicit Scope(QObject *parent = nullptr);
  QQmlListProperty<QObject> contents();

private:
  static void appendContent(QQmlListProperty<QObject> *list, QObject *object);
  static qsizetype contentCount(QQmlListProperty<QObject> *list);
  static QObject *contentAt(QQmlListProperty<QObject> *list, qsizetype index);
  static void clearContents(QQmlListProperty<QObject> *list);

  QList<QObject *> m_contents;
};

class ShellRoot : public Scope {
  Q_OBJECT
  QML_ELEMENT

public:
  explicit ShellRoot(QObject *parent = nullptr) : Scope(parent) {}
};
