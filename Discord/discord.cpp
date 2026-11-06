#include "./discord.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QtEndian>

namespace {
QStringList runtimeDirs() {
  const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (runtime.isEmpty())
    return {};
  return {runtime, runtime + "/app/com.discordapp.Discord",
          runtime + "/snap.discord"};
}
} // namespace

Discord::Discord(QObject *parent) : QObject(parent) {
  m_flushTimer.setSingleShot(true);
  connect(&m_flushTimer, &QTimer::timeout, this, &Discord::flush);

  connect(&m_dirWatcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &) {
            if (m_enabled && m_socket.state() == QLocalSocket::UnconnectedState)
              connectToDiscord();
          });

  connect(&m_socket, &QLocalSocket::connected, this, &Discord::onConnected);
  connect(&m_socket, &QLocalSocket::readyRead, this, &Discord::onReadyRead);
  connect(&m_socket, &QLocalSocket::disconnected, this, [this] { teardown(); });
  connect(&m_socket, &QLocalSocket::errorOccurred, this,
          [this](QLocalSocket::LocalSocketError) {
            if (!m_attemptConnected)
              tryNextCandidate();
          });

  refreshLatest();
}

Discord::~Discord() {
  m_enabled = false;
  m_flushTimer.stop();
  m_socket.abort();
}

void Discord::setClientId(const QString &v) {
  if (v == m_clientId)
    return;
  m_clientId = v;
  emit clientIdChanged();
  if (m_enabled) {
    m_socket.abort();
    teardown();
    connectToDiscord();
  }
}

void Discord::setEnabled(bool v) {
  if (v == m_enabled)
    return;
  m_enabled = v;
  emit enabledChanged();

  if (v) {
    watchRuntimeDirs();
    connectToDiscord();
    return;
  }

  m_flushTimer.stop();
  m_socket.abort();
  teardown();
  m_queue.clear();
  m_sentKey.clear();
  if (!m_dirWatcher.directories().isEmpty())
    m_dirWatcher.removePaths(m_dirWatcher.directories());
}

void Discord::watchRuntimeDirs() {
  for (const QString &dir : runtimeDirs())
    if (QFileInfo(dir).isDir() && !m_dirWatcher.directories().contains(dir))
      m_dirWatcher.addPath(dir);
}

void Discord::connectToDiscord() {
  if (!m_enabled || m_clientId.isEmpty() ||
      m_socket.state() != QLocalSocket::UnconnectedState)
    return;
  m_candidates.clear();
  for (const QString &dir : runtimeDirs())
    for (int i = 0; i < 10; ++i) {
      const QString path = QString("%1/discord-ipc-%2").arg(dir).arg(i);
      if (QFileInfo::exists(path))
        m_candidates.append(path);
    }
  m_candidateIdx = -1;
  m_attemptConnected = false;
  tryNextCandidate();
}

void Discord::tryNextCandidate() {
  if (!m_enabled)
    return;
  m_socket.abort();
  if (++m_candidateIdx >= m_candidates.size())
    return;
  m_socket.connectToServer(m_candidates.at(m_candidateIdx));
}

void Discord::teardown() {
  m_buffer.clear();
  m_flushTimer.stop();
  m_sentKey.clear();
  if (m_ready) {
    m_ready = false;
    emit connectedChanged();
  }
}

void Discord::onConnected() {
  m_attemptConnected = true;
  send(Handshake, QJsonObject{{"v", 1}, {"client_id", m_clientId}});
}

void Discord::send(quint32 op, const QJsonObject &payload) {
  const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
  QByteArray frame(8, '\0');
  qToLittleEndian<quint32>(op, frame.data());
  qToLittleEndian<quint32>(static_cast<quint32>(json.size()), frame.data() + 4);
  frame.append(json);
  m_socket.write(frame);
}

void Discord::onReadyRead() {
  m_buffer.append(m_socket.readAll());
  while (m_buffer.size() >= 8) {
    const quint32 op = qFromLittleEndian<quint32>(m_buffer.constData());
    const quint32 len = qFromLittleEndian<quint32>(m_buffer.constData() + 4);
    if (static_cast<quint32>(m_buffer.size()) < 8 + len)
      return;
    const QByteArray data = m_buffer.mid(8, len);
    m_buffer.remove(0, 8 + len);
    handleFrame(op, data);
  }
}

void Discord::handleFrame(quint32 op, const QByteArray &data) {
  switch (op) {
  case Ping:
    send(Pong, QJsonDocument::fromJson(data).object());
    break;
  case Close:
    emit error("discord closed the connection: " + QString::fromUtf8(data));
    m_socket.abort();
    break;
  case Frame: {
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    const QString evt = obj.value("evt").toString();
    if (evt == "READY") {
      m_ready = true;
      emit connectedChanged();
      requestFlush();
    } else if (evt == "ERROR") {
      emit error(
          QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    }
    break;
  }
  default:
    break;
  }
}

QVariantMap Discord::currentMap() const {
  return {{"details", m_details},       {"state", m_state},
          {"largeImage", m_largeImage}, {"largeText", m_largeText},
          {"smallImage", m_smallImage}, {"smallText", m_smallText},
          {"start", m_start},           {"end", m_end},
          {"type", int(m_type)},        {"buttons", m_buttons}};
}

QByteArray Discord::keyOf(const QJsonObject &o) {
  return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QJsonObject Discord::makeActivity(const QVariantMap &m) {
  QJsonObject act;

  const QString details = m.value("details").toString().left(128);
  const QString state = m.value("state").toString().left(128);
  if (!details.isEmpty())
    act["details"] = details;
  if (!state.isEmpty())
    act["state"] = state;

  QJsonObject assets;
  auto asset = [&](const char *mapKey, const char *jsonKey) {
    const QString v = m.value(mapKey).toString();
    if (!v.isEmpty())
      assets[jsonKey] = v;
  };
  asset("largeImage", "large_image");
  asset("largeText", "large_text");
  asset("smallImage", "small_image");
  asset("smallText", "small_text");
  if (!assets.isEmpty())
    act["assets"] = assets;

  QJsonObject ts;
  const qint64 s = m.value("start").toLongLong();
  const qint64 e = m.value("end").toLongLong();
  if (s > 0)
    ts["start"] = s;
  if (e > 0)
    ts["end"] = e;
  if (!ts.isEmpty())
    act["timestamps"] = ts;

  QJsonArray buttons;
  for (const QVariant &b : m.value("buttons").toList()) {
    if (buttons.size() >= 2)
      break;
    const QVariantMap bm = b.toMap();
    const QString label = bm.value("label").toString().left(32);
    const QString url = bm.value("url").toString();
    if (!label.isEmpty() && !url.isEmpty())
      buttons.append(QJsonObject{{"label", label}, {"url", url}});
  }
  if (!buttons.isEmpty())
    act["buttons"] = buttons;

  if (act.isEmpty())
    return act;

  const int type = m.value("type").toInt();
  if (type != 0)
    act["type"] = type;
  return act;
}

void Discord::refreshLatest() {
  m_latest = makeActivity(currentMap());
  m_latestKey = keyOf(m_latest);
  requestFlush();
}

void Discord::pushActivity(const QVariantMap &overrides) {
  if (!m_enabled)
    return;
  QVariantMap m = currentMap();
  for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
    m[it.key()] = it.value();

  const QJsonObject act = makeActivity(m);
  const QByteArray key = keyOf(act);
  if (!m_queue.isEmpty() && m_queue.last().key == key)
    return;
  m_queue.append({act, key});
  while (m_queue.size() > kMaxQueued)
    m_queue.removeFirst();
  requestFlush();
}

bool Discord::hasWork() const {
  if (!m_queue.isEmpty())
    return true;
  if (m_latestKey == m_sentKey)
    return false;
  return !(m_sentKey.isEmpty() && m_latest.isEmpty());
}

void Discord::requestFlush() {
  if (!m_enabled)
    return;
  if (m_ready) {
    if (hasWork())
      m_flushTimer.start(kDebounceMs);
  } else if (m_socket.state() == QLocalSocket::UnconnectedState) {
    connectToDiscord();
  }
}

void Discord::flush() {
  if (!m_enabled || !m_ready)
    return;

  while (!m_queue.isEmpty() && m_queue.first().key == m_sentKey)
    m_queue.removeFirst();

  if (!hasWork())
    return;

  if (m_sendClock.isValid()) {
    const qint64 elapsed = m_sendClock.elapsed();
    if (elapsed < kMinIntervalMs) {
      m_flushTimer.start(int(kMinIntervalMs - elapsed) + 100);
      return;
    }
  }

  QJsonObject next;
  QByteArray nextKey;
  if (!m_queue.isEmpty()) {
    next = m_queue.first().activity;
    nextKey = m_queue.first().key;
    m_queue.removeFirst();
  } else {
    next = m_latest;
    nextKey = m_latestKey;
  }

  sendActivity(next);
  m_sendClock.restart();
  m_sentKey = nextKey;

  if (hasWork())
    m_flushTimer.start(int(kMinIntervalMs) + 100);
}

void Discord::sendActivity(const QJsonObject &activity) {
  QJsonObject act = activity;

  QJsonObject ts = act.value("timestamps").toObject();
  if (ts.contains("end") && static_cast<qint64>(ts.value("end").toDouble()) <=
                                QDateTime::currentSecsSinceEpoch()) {
    ts.remove("end");
    if (ts.isEmpty())
      act.remove("timestamps");
    else
      act["timestamps"] = ts;
  }

  QJsonObject args{{"pid", QCoreApplication::applicationPid()}};
  if (!act.isEmpty())
    args["activity"] = act;

  send(Frame, QJsonObject{{"cmd", "SET_ACTIVITY"},
                          {"args", args},
                          {"nonce", QUuid::createUuid().toString(
                                        QUuid::WithoutBraces)}});
}
