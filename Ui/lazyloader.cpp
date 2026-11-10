#include "./lazyloader.hpp"

#include <QMetaObject>
#include <QQmlEngine>
#include <QWindow>

LazyLoader::LazyLoader(QObject *parent) : QObject(parent) {}

LazyLoader::~LazyLoader() {
  if (m_item) {
    delete m_item.data();
  }
}

void LazyLoader::setLoading(bool on) {
  if (m_loading == on) {
    return;
  }
  m_loading = on;
  emit loadingChanged();
}

void LazyLoader::setComponent(QQmlComponent *component) {
  if (m_component == component) {
    return;
  }
  m_component = component;
  emit componentChanged();
  if (m_active && !m_item) {
    create();
  }
}

void LazyLoader::setActive(bool on) {
  if (m_active == on) {
    return;
  }
  m_active = on;
  if (on) {
    create();
  } else {
    destroyItem();
  }
  emit activeChanged();
}

void LazyLoader::setActiveAsync(bool on) {
  if (m_activeAsync == on) {
    return;
  }
  m_activeAsync = on;
  emit activeAsyncChanged();
  if (!on) {
    setLoading(false);
    setActive(false);
    return;
  }
  if (m_active || m_loading) {
    return;
  }
  setLoading(true);
  QMetaObject::invokeMethod(
      this,
      [this]() {
        if (m_activeAsync && m_loading) {
          setLoading(false);
          setActive(true);
        }
      },
      Qt::QueuedConnection);
}

void LazyLoader::create() {
  if (!m_component || m_item) {
    return;
  }
  QObject *object = m_component->create();
  if (!object) {
    qWarning("ryu lazyloader: component creation failed: %s",
             qPrintable(m_component->errorString()));
    return;
  }
  QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);
  m_item = object;
  emit itemChanged();
}

void LazyLoader::destroyItem() {
  if (!m_item) {
    return;
  }
  QObject *object = m_item.data();
  m_item = nullptr;
  emit itemChanged();
  if (auto *window = qobject_cast<QWindow *>(object)) {
    window->hide();
  }
  object->deleteLater();
}
