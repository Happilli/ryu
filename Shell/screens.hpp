#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>

class Screens : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QVariantList list READ list NOTIFY listChanged)

public:
  explicit Screens(QObject *parent = nullptr);
  QVariantList list() const { return m_list; }

signals:
  void listChanged();

private:
  void rebuild();

  QVariantList m_list;
};
