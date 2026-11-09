#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

class ScriptModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(
      QVariantList values READ values WRITE setValues NOTIFY valuesChanged)
  Q_PROPERTY(QString objectProp READ objectProp WRITE setObjectProp NOTIFY
                 objectPropChanged)

public:
  explicit ScriptModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  QVariantList values() const { return m_values; }
  void setValues(const QVariantList &values);
  QString objectProp() const { return m_objectProp; }
  void setObjectProp(const QString &prop);

signals:
  void valuesChanged();
  void objectPropChanged();

private:
  QStringList keysFor(const QVariantList &values) const;

  QVariantList m_values;
  QStringList m_keys;
  QString m_objectProp;
};
