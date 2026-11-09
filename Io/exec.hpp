#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariant>

class Exec : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit Exec(QObject *parent = nullptr);

  Q_INVOKABLE bool detached(const QVariant &spec);
};
