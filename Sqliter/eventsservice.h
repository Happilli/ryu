#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

class EventsService : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit EventsService(QObject *parent = nullptr);

  Q_INVOKABLE QVariantMap eventsGet(int month, int day);
  Q_INVOKABLE void eventsSave(int month, int day, const QString &title,
                              const QString &description);
  Q_INVOKABLE void eventsRemove(int month, int day);
};
