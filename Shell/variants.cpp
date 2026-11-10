#include "./variants.hpp"
#include <QJSValue>
#include <QQmlContext>

Variants::Variants(QObject *parent) : QObject(parent) {}

void Variants::componentComplete() {
  m_complete = true;
  sync();
}

void Variants::setDelegate(QQmlComponent *delegate) {
  if (m_delegate.data() == delegate) {
    return;
  }
  m_delegate = delegate;
  emit delegateChanged();
  for (const Entry &entry : std::as_const(m_entries)) {
    if (entry.object) {
      entry.object->deleteLater();
    }
  }
  m_entries.clear();
  sync();
}

void Variants::setModel(const QVariant &model) {
  m_model = model;
  emit modelChanged();
  sync();
}

QVariantList Variants::instances() const {
  QVariantList out;
  for (const Entry &entry : m_entries) {
    if (entry.object) {
      out.append(QVariant::fromValue(entry.object.data()));
    }
  }
  return out;
}

QVariantList Variants::toList(const QVariant &model) {
  if (model.metaType() == QMetaType::fromType<QJSValue>()) {
    return model.value<QJSValue>().toVariant().toList();
  }
  return model.toList();
}

QObject *Variants::createInstance(const QVariant &value) {
  QQmlContext *context = m_delegate->creationContext();
  if (!context) {
    context = qmlContext(this);
  }
  QObject *object = m_delegate->beginCreate(context);
  if (!object) {
    qWarning("ryu variants: delegate creation failed: %s",
             qPrintable(m_delegate->errorString()));
    return nullptr;
  }
  object->setParent(this);
  m_delegate->setInitialProperties(object, QVariantMap{{"modelData", value}});
  m_delegate->completeCreate();
  return object;
}

void Variants::sync() {
  if (!m_complete || !m_delegate) {
    return;
  }
  if (m_delegate->isLoading()) {
    return;
  }

  const QVariantList wanted = toList(m_model);
  QList<Entry> old = m_entries;
  QList<Entry> next;

  for (const QVariant &value : wanted) {
    int found = -1;
    for (int i = 0; i < old.size(); ++i) {
      if (old.at(i).value == value && old.at(i).object) {
        found = i;
        break;
      }
    }
    if (found >= 0) {
      next.append(old.takeAt(found));
      continue;
    }
    QObject *object = createInstance(value);
    if (object) {
      next.append(Entry{value, object});
    }
  }

  for (const Entry &entry : std::as_const(old)) {
    if (entry.object) {
      entry.object->deleteLater();
    }
  }

  m_entries = next;
  emit instancesChanged();
}
