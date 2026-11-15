#pragma once
#include <QList>
#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QVariant>

class Variants : public QObject, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_CLASSINFO("DefaultProperty", "delegate")
  Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY
                 delegateChanged)
  Q_PROPERTY(QVariant model READ model WRITE setModel NOTIFY modelChanged)

public:
  explicit Variants(QObject *parent = nullptr);
  void classBegin() override {}
  void componentComplete() override;

  QQmlComponent *delegate() const { return m_delegate.data(); }
  void setDelegate(QQmlComponent *delegate);

  QVariant model() const { return m_model; }
  void setModel(const QVariant &model);

signals:
  void delegateChanged();
  void modelChanged();

private:
  struct Entry {
    QVariant value;
    QPointer<QObject> object;
  };

  static QVariantList toList(const QVariant &model);
  static void release(const QList<Entry> &entries);
  QObject *createInstance(const QVariant &value);
  void sync();

  QPointer<QQmlComponent> m_delegate;
  QVariant m_model;
  QList<Entry> m_entries;
  bool m_complete = false;
};
