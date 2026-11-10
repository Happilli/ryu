#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QtQml/qqmlregistration.h>

class LazyLoader : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_CLASSINFO("DefaultProperty", "component")
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
  Q_PROPERTY(bool activeAsync READ activeAsync WRITE setActiveAsync NOTIFY
                 activeAsyncChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QObject *item READ item NOTIFY itemChanged)
  Q_PROPERTY(QQmlComponent *component READ component WRITE setComponent NOTIFY
                 componentChanged)

public:
  explicit LazyLoader(QObject *parent = nullptr);
  ~LazyLoader() override;

  bool active() const { return m_active; }
  bool activeAsync() const { return m_activeAsync; }
  bool loading() const { return m_loading; }
  QObject *item() const { return m_item; }
  QQmlComponent *component() const { return m_component; }

  void setActive(bool on);
  void setActiveAsync(bool on);
  void setComponent(QQmlComponent *component);

signals:
  void activeChanged();
  void activeAsyncChanged();
  void loadingChanged();
  void itemChanged();
  void componentChanged();

private:
  void create();
  void destroyItem();
  void setLoading(bool on);

  QPointer<QQmlComponent> m_component;
  QPointer<QObject> m_item;
  bool m_active = false;
  bool m_activeAsync = false;
  bool m_loading = false;
};
