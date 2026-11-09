#include "./polkit.hpp"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLocalSocket>
#include <QLocale>
#include <QProcess>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace {

const QString kAuthorityService = QStringLiteral("org.freedesktop.PolicyKit1");
const QString kAuthorityPath =
    QStringLiteral("/org/freedesktop/PolicyKit1/Authority");
const QString kAuthorityInterface =
    QStringLiteral("org.freedesktop.PolicyKit1.Authority");
const QString kCancelled =
    QStringLiteral("org.freedesktop.PolicyKit1.Error.Cancelled");
const QString kFailed =
    QStringLiteral("org.freedesktop.PolicyKit1.Error.Failed");
const QString kHelperSocket = QStringLiteral("/run/polkit/agent-helper.socket");

void registerTypes() {
  static const bool done = [] {
    qDBusRegisterMetaType<PolkitIdentity>();
    qDBusRegisterMetaType<QList<PolkitIdentity>>();
    qDBusRegisterMetaType<QMap<QString, QString>>();
    return true;
  }();
  Q_UNUSED(done);
}

QString findHelper() {
  static const char *candidates[] = {
      "/usr/lib/polkit-1/polkit-agent-helper-1",
      "/usr/libexec/polkit-agent-helper-1",
      "/usr/lib64/polkit-1/polkit-agent-helper-1",
      "/usr/lib/polkit-agent-helper-1",
      "/usr/libexec/polkit-1/polkit-agent-helper-1"};
  for (const char *path : candidates) {
    if (QFileInfo::exists(QLatin1String(path)))
      return QLatin1String(path);
  }
  return QString();
}

QString userName(uid_t uid) {
  QByteArray buffer(16384, 0);
  passwd pwd;
  passwd *result = nullptr;
  if (getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &result) != 0 ||
      !result)
    return QString();
  return QString::fromLocal8Bit(result->pw_name);
}

QStringList groupMembers(gid_t gid) {
  QByteArray buffer(16384, 0);
  group grp;
  group *result = nullptr;
  QStringList members;
  if (getgrgid_r(gid, &grp, buffer.data(), buffer.size(), &result) != 0 ||
      !result)
    return members;
  for (char **member = result->gr_mem; member && *member; ++member)
    members.append(QString::fromLocal8Bit(*member));
  return members;
}

QString pickUser(const QList<PolkitIdentity> &identities) {
  QStringList users;
  for (const PolkitIdentity &identity : identities) {
    if (identity.kind == QLatin1String("unix-user")) {
      const QString name =
          userName(identity.details.value(QStringLiteral("uid")).toUInt());
      if (!name.isEmpty())
        users.append(name);
    } else if (identity.kind == QLatin1String("unix-group")) {
      users.append(
          groupMembers(identity.details.value(QStringLiteral("gid")).toUInt()));
    }
  }
  const QString me = userName(getuid());
  if (users.contains(me))
    return me;
  return users.value(0);
}

quint64 processStartTime() {
  QFile file(QStringLiteral("/proc/self/stat"));
  if (!file.open(QIODevice::ReadOnly))
    return 0;
  const QByteArray data = file.readAll();
  const int paren = data.lastIndexOf(')');
  if (paren < 0)
    return 0;
  const QList<QByteArray> parts = data.mid(paren + 2).split(' ');
  return parts.value(19).toULongLong();
}

PolkitIdentity makeSubject() {
  PolkitIdentity subject;
  const QString session = qEnvironmentVariable("XDG_SESSION_ID");
  if (!session.isEmpty()) {
    subject.kind = QStringLiteral("unix-session");
    subject.details.insert(QStringLiteral("session-id"), session);
  } else {
    subject.kind = QStringLiteral("unix-process");
    subject.details.insert(QStringLiteral("pid"),
                           QVariant::fromValue<quint32>(getpid()));
    subject.details.insert(QStringLiteral("start-time"),
                           QVariant::fromValue<quint64>(processStartTime()));
  }
  return subject;
}

QDBusMessage authorityCall(const QString &method, const QString &path,
                           bool withLocale) {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      kAuthorityService, kAuthorityPath, kAuthorityInterface, method);
  msg << QVariant::fromValue(makeSubject());
  if (withLocale)
    msg << QLocale().name() + QStringLiteral(".UTF-8");
  msg << path;
  return msg;
}

} // namespace

QDBusArgument &operator<<(QDBusArgument &arg, const PolkitIdentity &value) {
  arg.beginStructure();
  arg << value.kind << value.details;
  arg.endStructure();
  return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg,
                                PolkitIdentity &value) {
  arg.beginStructure();
  arg >> value.kind >> value.details;
  arg.endStructure();
  return arg;
}

PolkitFlow::PolkitFlow(const QString &actionId, const QString &message,
                       const QString &iconName, QObject *parent)
    : QObject(parent), m_actionId(actionId), m_message(message),
      m_iconName(iconName) {}

void PolkitFlow::submit(const QString &response) {
  if (!m_responseRequired)
    return;
  setFailed(false);
  m_responseRequired = false;
  emit promptChanged();
  emit submitted(response);
}

void PolkitFlow::cancelAuthenticationRequest() { emit cancelRequested(); }

void PolkitFlow::setPrompt(const QString &prompt, bool visible) {
  m_prompt = prompt;
  m_responseVisible = visible;
  m_responseRequired = true;
  emit promptChanged();
}

void PolkitFlow::clearPrompt() {
  m_prompt.clear();
  m_responseVisible = false;
  m_responseRequired = false;
  emit promptChanged();
}

void PolkitFlow::setSupplementary(const QString &text, bool isError) {
  if (m_supplementary == text && m_supplementaryIsError == isError)
    return;
  m_supplementary = text;
  m_supplementaryIsError = isError;
  emit supplementaryChanged();
}

void PolkitFlow::setFailed(bool failed) {
  if (m_failed == failed)
    return;
  m_failed = failed;
  emit failedChanged();
}

PolkitAgent::PolkitAgent(QObject *parent) : QObject(parent) {
  registerTypes();
  new PolkitAgentAdaptor(this);
}

PolkitAgent::~PolkitAgent() {
  dropHelper();
  unregisterAgent();
}

void PolkitAgent::setPath(const QString &path) {
  if (m_path == path || path.isEmpty())
    return;
  const bool wasReady = m_ready;
  if (wasReady)
    unregisterAgent();
  m_path = path;
  emit pathChanged();
  if (wasReady)
    registerAgent();
}

void PolkitAgent::componentComplete() {
  m_ready = true;
  registerAgent();
}

void PolkitAgent::setRegistered(bool registered) {
  if (m_registered == registered)
    return;
  m_registered = registered;
  emit registeredChanged();
}

void PolkitAgent::registerAgent() {
  m_helperPath = findHelper();
  if (m_helperPath.isEmpty() && !QFileInfo::exists(kHelperSocket)) {
    emit error(QStringLiteral("polkit helper not found"));
    return;
  }

  QDBusConnection bus = QDBusConnection::systemBus();
  if (!bus.isConnected()) {
    emit error(QStringLiteral("system bus unavailable"));
    return;
  }
  if (!bus.registerObject(m_path, this, QDBusConnection::ExportAdaptors)) {
    emit error(QStringLiteral("failed to export agent object at ") + m_path);
    return;
  }

  auto *watcher = new QDBusPendingCallWatcher(
      bus.asyncCall(authorityCall(QStringLiteral("RegisterAuthenticationAgent"),
                                  m_path, true)),
      this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *call) {
            call->deleteLater();
            const QDBusPendingReply<> reply = *call;
            if (reply.isError()) {
              QDBusConnection::systemBus().unregisterObject(m_path);
              emit error(reply.error().message());
              return;
            }
            setRegistered(true);
          });
}

void PolkitAgent::unregisterAgent() {
  if (!m_registered)
    return;
  QDBusConnection bus = QDBusConnection::systemBus();
  bus.call(authorityCall(QStringLiteral("UnregisterAuthenticationAgent"),
                         m_path, false),
           QDBus::NoBlock);
  bus.unregisterObject(m_path);
  setRegistered(false);
}

QDBusMessage PolkitAgent::replyFor(const PolkitRequest &request,
                                   const QString &errorName,
                                   const QString &errorText) const {
  return errorName.isEmpty()
             ? request.reply.createReply()
             : request.reply.createErrorReply(errorName, errorText);
}

void PolkitAgent::beginAuthentication(const PolkitRequest &request) {
  m_queue.append(request);
  if (!m_hasCurrent)
    startNext();
}

void PolkitAgent::cancelAuthentication(const QString &cookie) {
  if (m_hasCurrent && m_current.cookie == cookie) {
    finishCurrent(kCancelled, QStringLiteral("Authentication cancelled"));
    return;
  }
  for (int i = 0; i < m_queue.size(); ++i) {
    if (m_queue.at(i).cookie != cookie)
      continue;
    QDBusConnection::systemBus().send(replyFor(
        m_queue.at(i), kCancelled, QStringLiteral("Authentication cancelled")));
    m_queue.removeAt(i);
    return;
  }
}

void PolkitAgent::startNext() {
  while (!m_hasCurrent && !m_queue.isEmpty()) {
    m_current = m_queue.takeFirst();
    const QString user = pickUser(m_current.identities);
    if (user.isEmpty()) {
      QDBusConnection::systemBus().send(
          replyFor(m_current, kFailed, QStringLiteral("No usable identity")));
      m_current = PolkitRequest();
      continue;
    }

    m_user = user;
    m_hasCurrent = true;
    m_flow = new PolkitFlow(m_current.actionId, m_current.message,
                            m_current.iconName, this);
    connect(m_flow, &PolkitFlow::submitted, this, &PolkitAgent::respond);
    connect(m_flow, &PolkitFlow::cancelRequested, this, [this] {
      finishCurrent(kCancelled, QStringLiteral("Authentication cancelled"));
    });
    emit flowChanged();
    emit activeChanged();
    startHelper();
  }
}

void PolkitAgent::startHelper() {
  m_buffer.clear();
  m_outcome = Outcome::None;
  m_sawPrompt = false;

  const QByteArray cookie = m_current.cookie.toUtf8();
  const QByteArray user = m_user.toUtf8();

  if (QFileInfo::exists(kHelperSocket)) {
    auto *socket = new QLocalSocket(this);
    m_helper = socket;
    connect(socket, &QLocalSocket::connected, socket, [socket, user, cookie] {
      socket->write(user + '\n' + cookie + '\n');
    });
    connect(socket, &QLocalSocket::readyRead, this, &PolkitAgent::readHelper);
    connect(socket, &QLocalSocket::disconnected, this,
            [this] { onHelperFinished(); });
    connect(socket, &QLocalSocket::errorOccurred, this,
            [this](QLocalSocket::LocalSocketError err) {
              if (err == QLocalSocket::PeerClosedError)
                return;
              finishCurrent(
                  kFailed,
                  QStringLiteral("Failed to reach polkit helper socket"));
            });
    socket->connectToServer(kHelperSocket);
    return;
  }

  auto *helper = new QProcess(this);
  m_helper = helper;

  connect(helper, &QProcess::started, helper,
          [helper, cookie] { helper->write(cookie + '\n'); });
  connect(helper, &QProcess::readyReadStandardOutput, this,
          &PolkitAgent::readHelper);
  connect(helper, &QProcess::finished, this,
          [this](int, QProcess::ExitStatus) { onHelperFinished(); });
  connect(helper, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
              finishCurrent(kFailed,
                            QStringLiteral("Failed to start polkit helper"));
          });

  helper->start(m_helperPath, {m_user});
}

void PolkitAgent::dropHelper() {
  if (!m_helper)
    return;
  QIODevice *helper = m_helper;
  m_helper = nullptr;
  helper->disconnect(this);
  if (auto *process = qobject_cast<QProcess *>(helper)) {
    if (process->state() != QProcess::NotRunning) {
      process->kill();
      process->waitForFinished(200);
    }
  } else if (auto *socket = qobject_cast<QLocalSocket *>(helper)) {
    socket->abort();
  }
  helper->deleteLater();
}

void PolkitAgent::readHelper() {
  if (!m_helper || !m_flow)
    return;
  if (auto *process = qobject_cast<QProcess *>(m_helper))
    m_buffer.append(process->readAllStandardOutput());
  else
    m_buffer.append(m_helper->readAll());
  int index;
  while ((index = m_buffer.indexOf('\n')) >= 0) {
    const QByteArray line = m_buffer.left(index);
    m_buffer.remove(0, index + 1);
    handleLine(line);
  }
}

void PolkitAgent::handleLine(const QByteArray &line) {
  if (!m_flow)
    return;

  auto rest = [&line](int prefix) {
    return QString::fromUtf8(line.mid(prefix)).trimmed();
  };
  auto prompt = [&rest](int prefix) {
    QString text = rest(prefix);
    if (text.endsWith(QLatin1Char(':')))
      text.chop(1);
    return text.trimmed();
  };

  if (line.startsWith("PAM_PROMPT_ECHO_OFF")) {
    m_sawPrompt = true;
    m_flow->setPrompt(prompt(19), false);
  } else if (line.startsWith("PAM_PROMPT_ECHO_ON")) {
    m_sawPrompt = true;
    m_flow->setPrompt(prompt(18), true);
  } else if (line.startsWith("PAM_ERROR_MSG")) {
    m_flow->setSupplementary(rest(13), true);
  } else if (line.startsWith("PAM_TEXT_INFO")) {
    m_flow->setSupplementary(rest(13), false);
  } else if (line.startsWith("SUCCESS")) {
    m_outcome = Outcome::Success;
  } else if (line.startsWith("FAILURE")) {
    m_outcome = Outcome::Failure;
    if (m_sawPrompt) {
      m_flow->clearPrompt();
      m_flow->setFailed(true);
    }
  }
}

void PolkitAgent::onHelperFinished() {
  readHelper();
  if (!m_hasCurrent)
    return;

  if (m_outcome == Outcome::Success) {
    finishCurrent(QString(), QString());
    return;
  }

  if (m_outcome == Outcome::Failure) {
    if (!m_sawPrompt) {
      finishCurrent(kFailed,
                    QStringLiteral("Polkit helper failed before prompting"));
      return;
    }
    dropHelper();
    startHelper();
    return;
  }

  finishCurrent(kFailed, QStringLiteral("Polkit helper exited unexpectedly"));
}

void PolkitAgent::respond(const QString &response) {
  if (!m_helper || !m_helper->isOpen())
    return;
  m_helper->write(response.toUtf8() + '\n');
}

void PolkitAgent::finishCurrent(const QString &errorName,
                                const QString &errorText) {
  if (!m_hasCurrent)
    return;

  dropHelper();
  QDBusConnection::systemBus().send(replyFor(m_current, errorName, errorText));

  m_hasCurrent = false;
  m_current = PolkitRequest();
  m_user.clear();

  PolkitFlow *old = m_flow;
  m_flow = nullptr;
  emit flowChanged();
  emit activeChanged();
  if (old)
    old->deleteLater();

  startNext();
}

PolkitAgentAdaptor::PolkitAgentAdaptor(PolkitAgent *agent)
    : QDBusAbstractAdaptor(agent), m_agent(agent) {}

void PolkitAgentAdaptor::BeginAuthentication(
    const QString &actionId, const QString &message, const QString &iconName,
    const QMap<QString, QString> &details, const QString &cookie,
    const QList<PolkitIdentity> &identities, const QDBusMessage &msg) {
  msg.setDelayedReply(true);

  PolkitRequest request;
  request.actionId = actionId;
  request.message = message;
  request.iconName = iconName;
  request.details = details;
  request.cookie = cookie;
  request.identities = identities;
  request.reply = msg;
  m_agent->beginAuthentication(request);
}

void PolkitAgentAdaptor::CancelAuthentication(const QString &cookie) {
  m_agent->cancelAuthentication(cookie);
}
