#include "./ipc.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaType>
#include <QVariant>

#include <unistd.h>

namespace {

constexpr int kMaxFields = 64;
constexpr int kMaxBytes = 1 << 20;

QPointer<IpcServer> g_server;

QString socketPath() {
  const QString override = qEnvironmentVariable("RYU_IPC_SOCKET");
  if (!override.isEmpty())
    return override;
  const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (!runtime.isEmpty())
    return runtime + QStringLiteral("/ryu-ipc.sock");
  return QStringLiteral("/tmp/ryu-ipc-%1.sock").arg(getuid());
}

QList<QMetaMethod> functionsOf(const QObject *object) {
  QList<QMetaMethod> out;
  const QMetaObject *mo = object->metaObject();
  for (int i = IpcHandler::staticMetaObject.methodCount();
       i < mo->methodCount(); ++i) {
    const QMetaMethod method = mo->method(i);
    if (method.methodType() != QMetaMethod::Signal &&
        method.methodType() != QMetaMethod::Constructor)
      out.append(method);
  }
  return out;
}
QString typeLabel(const QMetaType &type) {
  switch (type.id()) {
  case QMetaType::Void:
    return QStringLiteral("void");
  case QMetaType::QString:
    return QStringLiteral("string");
  case QMetaType::Bool:
    return QStringLiteral("bool");
  case QMetaType::Int:
    return QStringLiteral("int");
  case QMetaType::Double:
    return QStringLiteral("real");
  default:
    return QStringLiteral("var");
  }
}

} // namespace

IpcServer *IpcServer::instance() {
  if (!g_server)
    g_server = new IpcServer(QCoreApplication::instance());
  return g_server;
}

IpcServer *IpcServer::existing() { return g_server; }

IpcServer::IpcServer(QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this)) {
  m_server->setSocketOptions(QLocalServer::UserAccessOption);
  const QString path = socketPath();
  QLocalServer::removeServer(path);
  if (!m_server->listen(path))
    qWarning() << "ryu ipc: cannot listen on" << path
               << m_server->errorString();
  connect(m_server, &QLocalServer::newConnection, this, &IpcServer::accept);
}

void IpcServer::add(IpcHandler *handler) {
  const QString target = handler->target();
  const QPointer<IpcHandler> current = m_handlers.value(target);
  if (current && current != handler)
    qWarning() << "ryu ipc: target" << target << "re-registered";
  m_handlers.insert(target, handler);
}

void IpcServer::remove(IpcHandler *handler) {
  for (auto it = m_handlers.begin(); it != m_handlers.end();) {
    if (it.value() == handler || it.value().isNull())
      it = m_handlers.erase(it);
    else
      ++it;
  }
}

void IpcServer::accept() {
  while (QLocalSocket *socket = m_server->nextPendingConnection()) {
    connect(socket, &QLocalSocket::readyRead, this,
            [this, socket] { read(socket); });
    connect(socket, &QLocalSocket::disconnected, this,
            [this, socket] { m_buffers.remove(socket); });
    connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    if (socket->bytesAvailable() > 0)
      read(socket);
  }
}

void IpcServer::read(QLocalSocket *socket) {
  QByteArray &buffer = m_buffers[socket];
  buffer.append(socket->readAll());

  if (buffer.size() > kMaxBytes) {
    send(socket, {false, QStringLiteral("Request too large")});
    return;
  }

  const int head = buffer.indexOf('\0');
  if (head < 0)
    return;

  bool ok = false;
  const int count = buffer.left(head).toInt(&ok);
  if (!ok || count < 0 || count > kMaxFields) {
    send(socket, {false, QStringLiteral("Malformed request")});
    return;
  }

  QStringList fields;
  int pos = head + 1;
  while (fields.size() < count) {
    const int end = buffer.indexOf('\0', pos);
    if (end < 0)
      return;
    fields.append(QString::fromUtf8(buffer.mid(pos, end - pos)));
    pos = end + 1;
  }

  send(socket, dispatch(fields));
}

void IpcServer::send(QLocalSocket *socket, const Reply &reply) {
  socket->disconnect(this);
  m_buffers.remove(socket);
  socket->write(QByteArray(1, reply.ok ? 'o' : 'e') + reply.text.toUtf8());
  socket->disconnectFromServer();
}

IpcServer::Reply IpcServer::dispatch(const QStringList &fields) const {
  if (fields.size() == 1 && fields.first() == QLatin1String("@show"))
    return {true, describe()};
  if (fields.size() < 2)
    return {false, QStringLiteral("Malformed request")};

  const QPointer<IpcHandler> handler = m_handlers.value(fields.at(0));
  if (!handler)
    return {false, QStringLiteral("Target not found: ") + fields.at(0)};
  return call(handler.data(), fields.at(1), fields.mid(2));
}

IpcServer::Reply IpcServer::call(IpcHandler *handler, const QString &function,
                                 const QStringList &args) const {
  const QByteArray name = function.toUtf8();
  const QList<QMetaMethod> methods = functionsOf(handler);

  QMetaMethod method;
  bool found = false;
  int expected = -1;
  for (const QMetaMethod &candidate : methods) {
    if (candidate.name() != name)
      continue;
    expected = candidate.parameterCount();
    if (expected == args.size()) {
      method = candidate;
      found = true;
      break;
    }
  }

  if (!found) {
    if (expected >= 0)
      return {false, QStringLiteral("Function %1 expects %2 arguments, got %3")
                         .arg(function)
                         .arg(expected)
                         .arg(args.size())};
    return {false, QStringLiteral("Function not found: ") + function};
  }

  if (args.size() > 10)
    return {false, QStringLiteral("Too many arguments")};

  QVariant values[10];
  QGenericArgument generic[10];
  for (int i = 0; i < args.size(); ++i) {
    const QMetaType type = method.parameterMetaType(i);
    if (!type.isValid() || type.id() == QMetaType::QVariant) {
      values[i] = args.at(i);
      generic[i] = QGenericArgument("QVariant", &values[i]);
      continue;
    }
    QVariant converted(args.at(i));
    if (!converted.convert(type))
      return {false, QStringLiteral("Argument %1 cannot be converted to %2")
                         .arg(i + 1)
                         .arg(typeLabel(type))};
    values[i] = converted;
    generic[i] = QGenericArgument(method.parameterTypeName(i).constData(),
                                  values[i].constData());
  }

  const QMetaType returnType = method.returnMetaType();
  QVariant result;
  bool invoked = false;

  if (returnType.id() == QMetaType::Void) {
    invoked =
        method.invoke(handler, Qt::DirectConnection, generic[0], generic[1],
                      generic[2], generic[3], generic[4], generic[5],
                      generic[6], generic[7], generic[8], generic[9]);
  } else if (!returnType.isValid() || returnType.id() == QMetaType::QVariant) {
    invoked = method.invoke(handler, Qt::DirectConnection,
                            QGenericReturnArgument("QVariant", &result),
                            generic[0], generic[1], generic[2], generic[3],
                            generic[4], generic[5], generic[6], generic[7],
                            generic[8], generic[9]);
  } else {
    void *buffer = returnType.create();
    invoked = method.invoke(handler, Qt::DirectConnection,
                            QGenericReturnArgument(returnType.name(), buffer),
                            generic[0], generic[1], generic[2], generic[3],
                            generic[4], generic[5], generic[6], generic[7],
                            generic[8], generic[9]);
    if (invoked)
      result = QVariant(returnType, buffer);
    returnType.destroy(buffer);
  }

  if (!invoked)
    return {false, QStringLiteral("Failed to invoke ") + function};

  if (result.metaType().id() == QMetaType::Bool)
    return {true,
            result.toBool() ? QStringLiteral("true") : QStringLiteral("false")};
  return {true, result.toString()};
}

QString IpcServer::describe() const {
  QStringList targets = m_handlers.keys();
  targets.sort();

  QStringList lines;
  for (const QString &target : std::as_const(targets)) {
    const QPointer<IpcHandler> handler = m_handlers.value(target);
    if (!handler)
      continue;
    lines.append(QStringLiteral("target ") + target);
    for (const QMetaMethod &method : functionsOf(handler.data())) {
      QStringList params;
      const QList<QByteArray> names = method.parameterNames();
      for (int i = 0; i < method.parameterCount(); ++i)
        params.append(QString::fromUtf8(names.value(i)) + QStringLiteral(": ") +
                      typeLabel(method.parameterMetaType(i)));
      lines.append(QStringLiteral("  function %1(%2): %3")
                       .arg(QString::fromUtf8(method.name()),
                            params.join(QStringLiteral(", ")),
                            typeLabel(method.returnMetaType())));
    }
  }
  return lines.join(QLatin1Char('\n'));
}

IpcHandler::IpcHandler(QObject *parent) : QObject(parent) {}

IpcHandler::~IpcHandler() {
  if (IpcServer *server = IpcServer::existing())
    server->remove(this);
}

void IpcHandler::setTarget(const QString &target) {
  if (m_target == target)
    return;
  m_target = target;
  emit targetChanged();
  if (m_ready)
    sync();
}

void IpcHandler::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  emit enabledChanged();
  if (m_ready)
    sync();
}

void IpcHandler::componentComplete() {
  m_ready = true;
  sync();
}

void IpcHandler::sync() {
  if (IpcServer *server = IpcServer::existing())
    server->remove(this);
  if (m_enabled && !m_target.isEmpty())
    IpcServer::instance()->add(this);
}
