#pragma once
#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QQmlListProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>

template <typename T> bool assignChanged(T &field, const T &value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

inline QScreen *screenFromSource(QObject *source) {
  if (!source) {
    return nullptr;
  }
  if (auto *screen = qobject_cast<QScreen *>(source)) {
    return screen;
  }
  const QString name = source->property("name").toString();
  for (QScreen *screen : QGuiApplication::screens()) {
    if (screen->name() == name) {
      return screen;
    }
  }
  return nullptr;
}

template <typename Window>
QQmlListProperty<QObject> windowContents(Window *window,
                                         QList<QObject *> *list) {
  return QQmlListProperty<QObject>(
      window, list,
      [](QQmlListProperty<QObject> *p, QObject *object) {
        auto *self = static_cast<Window *>(p->object);
        static_cast<QList<QObject *> *>(p->data)->append(object);
        if (auto *item = qobject_cast<QQuickItem *>(object)) {
          item->setParentItem(self->contentItem());
        } else {
          object->setParent(self);
        }
      },
      [](QQmlListProperty<QObject> *p) -> qsizetype {
        return static_cast<QList<QObject *> *>(p->data)->size();
      },
      [](QQmlListProperty<QObject> *p, qsizetype index) -> QObject * {
        return static_cast<QList<QObject *> *>(p->data)->at(index);
      },
      [](QQmlListProperty<QObject> *p) {
        static_cast<QList<QObject *> *>(p->data)->clear();
      });
}
