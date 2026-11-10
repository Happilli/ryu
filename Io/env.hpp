#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class Env : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit Env(QObject *parent = nullptr);

  Q_INVOKABLE QString get(const QString &name) const;
  Q_INVOKABLE bool has(const QString &name) const;
};
