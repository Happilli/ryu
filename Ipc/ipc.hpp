#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaMethod>
#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QString>
#include <QStringList>
#include <qqmlregistration.h>

class QLocalServer;
class QLocalSocket;
class IpcHandler;

class IpcServer : public QObject {
  Q_OBJECT

public:
  static IpcServer *instance();
  static IpcServer *existing();

  void add(IpcHandler *handler);
  void remove(IpcHandler *handler);

private:
  struct Reply {
    bool ok;
    QString text;
  };

  explicit IpcServer(QObject *parent);

  void accept();
  void read(QLocalSocket *socket);
  void send(QLocalSocket *socket, const Reply &reply);
  Reply dispatch(const QStringList &fields) const;
  Reply call(IpcHandler *handler, const QString &function,
             const QStringList &args) const;
  QString describe() const;

  QLocalServer *m_server;
  QHash<QString, QPointer<IpcHandler>> m_handlers;
  QHash<QLocalSocket *, QByteArray> m_buffers;
};

class IpcHandler : public QObject, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
  explicit IpcHandler(QObject *parent = nullptr);
  ~IpcHandler() override;

  QString target() const { return m_target; }
  void setTarget(const QString &target);
  bool enabled() const { return m_enabled; }
  void setEnabled(bool enabled);

  void classBegin() override {}
  void componentComplete() override;

signals:
  void targetChanged();
  void enabledChanged();

private:
  void sync();

  QString m_target;
  bool m_enabled = true;
  bool m_ready = false;
};
