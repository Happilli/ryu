#include "./mpris.hpp"
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QQmlEngine>
#include <QStringList>
#include <algorithm>
#include <limits>

namespace {
const QString kPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kRootIface = QStringLiteral("org.mpris.MediaPlayer2");
const QString kPlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString kProps = QStringLiteral("org.freedesktop.DBus.Properties");
const QString kNoTrack =
    QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");
const QString kBusName = QStringLiteral("org.freedesktop.DBus");
const QString kBusPath = QStringLiteral("/org/freedesktop/DBus");

constexpr int kTimeoutMs = 5000;
constexpr int kResyncMs = 4000;
constexpr int kTickMs = 250;

bool ignored(const QString &name) {
  return !name.startsWith(kPrefix) ||
         name == QLatin1String("org.mpris.MediaPlayer2.playerctld");
}

QVariantMap asMap(const QVariant &v) {
  if (v.canConvert<QDBusArgument>())
    return qdbus_cast<QVariantMap>(v.value<QDBusArgument>());
  return v.toMap();
}

QString idOf(const QVariant &v) {
  if (v.metaType() == QMetaType::fromType<QDBusObjectPath>())
    return v.value<QDBusObjectPath>().path();
  return v.toString();
}

bool validObjectPath(const QString &s) {
  if (s.size() < 2 || s.at(0) != QLatin1Char('/') ||
      s.endsWith(QLatin1Char('/')) || s.contains(QLatin1String("//")))
    return false;
  for (const QChar c : s) {
    const bool ok = (c.unicode() < 128 && c.isLetterOrNumber()) ||
                    c == QLatin1Char('_') || c == QLatin1Char('/');
    if (!ok)
      return false;
  }
  return true;
}
} // namespace

MprisPlayer::MprisPlayer(const QString &busName, QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::sessionBus()),
      m_busName(busName) {
  m_clock.start();
  m_resync.setInterval(kResyncMs);
  connect(&m_resync, &QTimer::timeout, this, &MprisPlayer::fetchPosition);
  m_tick.setInterval(kTickMs);
  connect(&m_tick, &QTimer::timeout, this, &MprisPlayer::positionChanged);
}

QString MprisPlayer::identity() const {
  if (!m_identity.isEmpty())
    return m_identity;
  QString n = m_busName.mid(kPrefix.size());
  const int i = n.indexOf(QLatin1String(".instance"));
  return i > 0 ? n.left(i) : n;
}

void MprisPlayer::start() {
  m_bus.connect(m_busName, kPath, kProps, QStringLiteral("PropertiesChanged"),
                this,
                SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
  m_bus.connect(m_busName, kPath, kPlayerIface, QStringLiteral("Seeked"), this,
                SLOT(onSeeked(qlonglong)));

  m_loading = 2;
  getAll(kRootIface, [this](bool, const QVariantMap &m) {
    applyRoot(m);
    finishLoad(true);
  });
  getAll(kPlayerIface, [this](bool ok, const QVariantMap &m) {
    if (!ok) {
      finishLoad(false);
      return;
    }
    applyPlayer(m, true);
    finishLoad(true);
  });
}

void MprisPlayer::finishLoad(bool ok) {
  if (m_failed || m_ready)
    return;
  if (!ok) {
    m_failed = true;
    emit failed();
    return;
  }
  if (--m_loading > 0)
    return;
  m_ready = true;
  fetchPosition();
  emit ready();
}

void MprisPlayer::getAll(const QString &iface, Done done) {
  QDBusMessage msg = QDBusMessage::createMethodCall(m_busName, kPath, kProps,
                                                    QStringLiteral("GetAll"));
  msg << iface;
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kTimeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [done = std::move(done)](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QVariantMap> r = *watcher;
            watcher->deleteLater();
            if (r.isError())
              done(false, {});
            else
              done(true, r.value());
          });
}

void MprisPlayer::refetchPlayer() {
  getAll(kPlayerIface, [this](bool ok, const QVariantMap &m) {
    if (ok)
      applyPlayer(m, false);
  });
}

void MprisPlayer::fetchPosition() {
  QDBusMessage msg = QDBusMessage::createMethodCall(m_busName, kPath, kProps,
                                                    QStringLiteral("Get"));
  msg << kPlayerIface << QStringLiteral("Position");
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, kTimeoutMs), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QDBusVariant> r = *watcher;
            watcher->deleteLater();
            if (!r.isError())
              setBase(r.value().variant().toLongLong());
          });
}

void MprisPlayer::onPropertiesChanged(const QString &iface,
                                      const QVariantMap &changed,
                                      const QStringList &invalidated) {
  if (iface == kRootIface) {
    applyRoot(changed);
  } else if (iface == kPlayerIface) {
    applyPlayer(changed, false);
    if (!invalidated.isEmpty())
      refetchPlayer();
  }
}

void MprisPlayer::onSeeked(qlonglong us) { setBase(us); }

void MprisPlayer::applyRoot(const QVariantMap &m) {
  bool changed = false;
  if (auto it = m.constFind(QStringLiteral("Identity")); it != m.cend()) {
    const QString v = it->toString();
    if (v != m_identity) {
      m_identity = v;
      changed = true;
    }
  }
  if (auto it = m.constFind(QStringLiteral("DesktopEntry")); it != m.cend()) {
    const QString v = it->toString();
    if (v != m_desktopEntry) {
      m_desktopEntry = v;
      changed = true;
    }
  }
  if (changed)
    emit identityChanged();
}

void MprisPlayer::applyPlayer(const QVariantMap &m, bool initial) {
  const QString kStatus = QStringLiteral("PlaybackStatus");
  const QString kRate = QStringLiteral("Rate");
  const QString kMeta = QStringLiteral("Metadata");
  const QString kPos = QStringLiteral("Position");

  if (m.contains(kStatus) || m.contains(kRate))
    snapshot();

  bool playback = false, track = false, caps = false, rate = false,
       newTrack = false;
  auto upd = [](auto &field, const auto &v) {
    if (field == v)
      return false;
    field = v;
    return true;
  };

  if (auto it = m.constFind(kStatus); it != m.cend())
    playback = upd(m_status, it->toString());

  if (auto it = m.constFind(kRate); it != m.cend()) {
    double r = it->toDouble();
    if (r <= 0.0)
      r = 1.0;
    rate = upd(m_rate, r);
  }

  if (auto it = m.constFind(kMeta); it != m.cend()) {
    const QVariantMap md = asMap(*it);
    const QString title = md.value(QStringLiteral("xesam:title")).toString();
    const QString artist =
        md.value(QStringLiteral("xesam:artist")).toStringList().join(", ");
    const QString album = md.value(QStringLiteral("xesam:album")).toString();
    const QString art = md.value(QStringLiteral("mpris:artUrl")).toString();
    const qint64 len = md.value(QStringLiteral("mpris:length")).toLongLong();
    const QString id = idOf(md.value(QStringLiteral("mpris:trackid")));

    newTrack = id != m_trackId || (id.isEmpty() && title != m_title);
    m_trackId = id;
    track |= upd(m_title, title);
    track |= upd(m_artist, artist);
    track |= upd(m_album, album);
    track |= upd(m_artUrl, art);
    track |= upd(m_lengthUs, len);
  }

  auto flag = [&](const char *key, bool &field) {
    const auto it = m.constFind(QLatin1String(key));
    if (it != m.cend())
      caps |= upd(field, it->toBool());
  };
  flag("CanGoNext", m_canNext);
  flag("CanGoPrevious", m_canPrev);
  flag("CanSeek", m_canSeek);
  flag("CanPlay", m_canPlay);
  flag("CanPause", m_canPause);
  flag("CanControl", m_canControl);

  if (auto it = m.constFind(kPos); it != m.cend())
    setBase(it->toLongLong());
  else if (newTrack && !initial) {
    setBase(0);
    fetchPosition();
  }

  if (track)
    emit trackChanged();
  if (caps)
    emit capsChanged();
  if (rate)
    emit rateChanged();
  if (playback) {
    emit playbackChanged();
    updateResync();
    if (isPlaying())
      fetchPosition();
  }
}

qint64 MprisPlayer::currentUs() const {
  qint64 us = m_posUs;
  if (isPlaying())
    us += qint64(double(m_clock.elapsed() - m_stamp) * 1000.0 * m_rate);
  if (m_lengthUs > 0)
    us = std::min(us, m_lengthUs);
  return std::max<qint64>(0, us);
}

void MprisPlayer::snapshot() {
  m_posUs = currentUs();
  m_stamp = m_clock.elapsed();
}

void MprisPlayer::setBase(qint64 us) {
  m_posUs = std::max<qint64>(0, us);
  m_stamp = m_clock.elapsed();
  emit positionChanged();
}

void MprisPlayer::updateResync() {
  if (isPlaying()) {
    if (!m_resync.isActive())
      m_resync.start();
    if (!m_tick.isActive())
      m_tick.start();
  } else {
    m_resync.stop();
    m_tick.stop();
  }
}

bool MprisPlayer::hasTrackPath() const {
  return validObjectPath(m_trackId) && m_trackId != kNoTrack;
}

void MprisPlayer::setPosition(double seconds) {
  if (!m_canSeek)
    return;
  const qint64 maxUs =
      m_lengthUs > 0 ? m_lengthUs : std::numeric_limits<qint64>::max();
  const qint64 target = std::clamp<qint64>(qint64(seconds * 1e6), 0, maxUs);

  if (hasTrackPath())
    callPlayer(QStringLiteral("SetPosition"),
               {QVariant::fromValue(QDBusObjectPath(m_trackId)),
                QVariant::fromValue<qlonglong>(target)});
  else
    callPlayer(QStringLiteral("Seek"),
               {QVariant::fromValue<qlonglong>(target - currentUs())});

  setBase(target);
}

void MprisPlayer::setTrackPosition(bool on) {
  if (m_track == on)
    return;
  m_track = on;
  emit trackPositionChanged();
  updateResync();
  if (on && m_ready) {
    fetchPosition();
    emit positionChanged();
  }
}

void MprisPlayer::callPlayer(const QString &method, const QVariantList &args) {
  QDBusMessage msg =
      QDBusMessage::createMethodCall(m_busName, kPath, kPlayerIface, method);
  msg.setArguments(args);
  m_bus.send(msg);
}

void MprisPlayer::togglePlaying() {
  if (canTogglePlaying())
    callPlayer(QStringLiteral("PlayPause"));
}
void MprisPlayer::play() { callPlayer(QStringLiteral("Play")); }
void MprisPlayer::pause() { callPlayer(QStringLiteral("Pause")); }
void MprisPlayer::stop() { callPlayer(QStringLiteral("Stop")); }
void MprisPlayer::next() {
  if (m_canNext)
    callPlayer(QStringLiteral("Next"));
}
void MprisPlayer::previous() {
  if (m_canPrev)
    callPlayer(QStringLiteral("Previous"));
}

Mpris::Mpris(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::sessionBus()) {
  if (!m_bus.isConnected())
    return;

  m_bus.connect(kBusName, kBusPath, kBusName,
                QStringLiteral("NameOwnerChanged"), this,
                SLOT(onNameOwnerChanged(QString, QString, QString)));

  QDBusMessage msg = QDBusMessage::createMethodCall(
      kBusName, kBusPath, kBusName, QStringLiteral("ListNames"));
  auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
  connect(w, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *watcher) {
            const QDBusPendingReply<QStringList> r = *watcher;
            watcher->deleteLater();
            if (r.isError())
              return;
            for (const QString &n : r.value())
              if (!ignored(n))
                add(n);
          });
}

QVariantList Mpris::players() const {
  QVariantList out;
  out.reserve(m_players.size());
  for (MprisPlayer *p : m_players)
    out.append(QVariant::fromValue<QObject *>(p));
  return out;
}

void Mpris::onNameOwnerChanged(const QString &name, const QString &oldOwner,
                               const QString &newOwner) {
  if (ignored(name))
    return;
  if (!oldOwner.isEmpty())
    remove(name);
  if (!newOwner.isEmpty())
    add(name);
}

void Mpris::add(const QString &name) {
  if (m_pending.contains(name))
    return;
  for (MprisPlayer *p : std::as_const(m_players))
    if (p->busName() == name)
      return;

  auto *p = new MprisPlayer(name, this);
  QQmlEngine::setObjectOwnership(p, QQmlEngine::CppOwnership);
  m_pending.insert(name, p);

  connect(p, &MprisPlayer::ready, this, [this, p] { promote(p); });
  connect(p, &MprisPlayer::failed, this, [this, p] {
    m_pending.remove(p->busName());
    p->deleteLater();
  });
  connect(p, &MprisPlayer::playbackChanged, this, [this, p] { onPlayback(p); });
  p->start();
}

void Mpris::remove(const QString &name) {
  if (MprisPlayer *p = m_pending.take(name)) {
    p->deleteLater();
    return;
  }
  for (int i = 0; i < m_players.size(); ++i) {
    if (m_players[i]->busName() != name)
      continue;
    MprisPlayer *p = m_players.takeAt(i);
    emit playersChanged();
    if (m_active == p)
      pickActive();
    p->deleteLater();
    return;
  }
}

void Mpris::promote(MprisPlayer *p) {
  if (m_pending.take(p->busName()) != p)
    return;
  m_players.append(p);
  emit playersChanged();
  if (p->isPlaying())
    setActive(p);
  else if (!m_active)
    pickActive();
}

void Mpris::onPlayback(MprisPlayer *p) {
  if (!m_players.contains(p))
    return;
  if (p->isPlaying())
    setActive(p);
  else if (m_active == p)
    pickActive();
}

void Mpris::pickActive() {
  if (m_active && m_players.contains(m_active) && m_active->isPlaying())
    return;
  MprisPlayer *next = nullptr;
  for (MprisPlayer *p : std::as_const(m_players)) {
    if (p->isPlaying()) {
      next = p;
      break;
    }
  }
  if (!next)
    next = (m_active && m_players.contains(m_active))
               ? m_active
               : (m_players.isEmpty() ? nullptr : m_players.first());
  setActive(next);
}

void Mpris::setActive(MprisPlayer *p) {
  if (m_active == p)
    return;
  m_active = p;
  emit activeChanged();
}
