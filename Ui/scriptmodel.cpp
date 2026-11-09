#include "./scriptmodel.hpp"

#include <QMetaType>
#include <QObject>
#include <QSet>
#include <QVariantMap>

namespace {

constexpr int kValueRole = Qt::UserRole + 1;

bool isObjectPointer(const QVariant &value) {
  return value.metaType().flags().testFlag(QMetaType::PointerToQObject);
}

QString baseKey(const QVariant &value, const QString &prop) {
  QVariant key = value;
  if (!prop.isEmpty()) {
    if (isObjectPointer(value)) {
      QObject *object = value.value<QObject *>();
      key = object ? object->property(prop.toUtf8().constData()) : QVariant();
    } else if (value.metaType() == QMetaType::fromType<QVariantMap>()) {
      key = value.toMap().value(prop);
    }
  }
  if (isObjectPointer(key)) {
    return QStringLiteral("o:") +
           QString::number(quintptr(key.value<QObject *>()), 16);
  }
  if (!key.isValid() || key.isNull()) {
    return {};
  }
  const QString text = key.toString();
  if (text.isEmpty()) {
    return {};
  }
  return QString::number(key.typeId()) + QLatin1Char(':') + text;
}

} // namespace

ScriptModel::ScriptModel(QObject *parent) : QAbstractListModel(parent) {}

int ScriptModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_values.size());
}

QVariant ScriptModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_values.size() ||
      role != kValueRole) {
    return {};
  }
  return m_values.at(index.row());
}

QHash<int, QByteArray> ScriptModel::roleNames() const {
  return {{kValueRole, QByteArrayLiteral("modelData")}};
}

QStringList ScriptModel::keysFor(const QVariantList &values) const {
  QStringList keys;
  keys.reserve(values.size());
  QHash<QString, int> seen;
  for (int i = 0; i < values.size(); ++i) {
    QString base = baseKey(values.at(i), m_objectProp);
    if (base.isEmpty()) {
      base = QStringLiteral("#") + QString::number(i);
    }
    const int occurrence = seen[base]++;
    keys.append(base + QChar(0x1f) + QString::number(occurrence));
  }
  return keys;
}

void ScriptModel::setValues(const QVariantList &values) {
  const QStringList next = keysFor(values);
  if (next == m_keys && values == m_values) {
    return;
  }

  const QSet<QString> keep(next.begin(), next.end());
  for (int i = static_cast<int>(m_keys.size()) - 1; i >= 0; --i) {
    if (keep.contains(m_keys.at(i))) {
      continue;
    }
    beginRemoveRows(QModelIndex(), i, i);
    m_keys.removeAt(i);
    m_values.removeAt(i);
    endRemoveRows();
  }

  for (int i = 0; i < next.size(); ++i) {
    if (i >= m_keys.size() || m_keys.at(i) != next.at(i)) {
      const int from = i < m_keys.size() ? m_keys.indexOf(next.at(i), i + 1) : -1;
      if (from >= 0) {
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), i);
        m_keys.move(from, i);
        m_values.move(from, i);
        endMoveRows();
      } else {
        beginInsertRows(QModelIndex(), i, i);
        m_keys.insert(i, next.at(i));
        m_values.insert(i, values.at(i));
        endInsertRows();
        continue;
      }
    }
    if (m_values.at(i) != values.at(i)) {
      m_values[i] = values.at(i);
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {kValueRole});
    }
  }

  m_values = values;
  emit valuesChanged();
}

void ScriptModel::setObjectProp(const QString &prop) {
  if (m_objectProp == prop) {
    return;
  }
  m_objectProp = prop;
  beginResetModel();
  m_keys = keysFor(m_values);
  endResetModel();
  emit objectPropChanged();
}
