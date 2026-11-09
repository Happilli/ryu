#pragma once
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

class MprisPlayer : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(MprisPlayer)
  QML_UNCREATABLE("MprisPlayer instances come from the Mpris singleton")

  Q_PROPERTY(QString busName READ busName CONSTANT)
  Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
  Q_PROPERTY(QString desktopEntry READ desktopEntry NOTIFY identityChanged)

  Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackChanged)
  Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY playbackChanged)

  Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
  Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY trackChanged)
  Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY trackChanged)
  Q_PROPERTY(QString trackArtUrl READ trackArtUrl NOTIFY trackChanged)
  Q_PROPERTY(bool trackPosition READ trackPosition WRITE setTrackPosition NOTIFY
                 trackPositionChanged)

  Q_PROPERTY(double length READ length NOTIFY trackChanged)
  Q_PROPERTY(
      double position READ position WRITE setPosition NOTIFY positionChanged)
  Q_PROPERTY(double rate READ rate NOTIFY rateChanged)

  Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY capsChanged)
  Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY capsChanged)
  Q_PROPERTY(bool canSeek READ canSeek NOTIFY capsChanged)
  Q_PROPERTY(bool canTogglePlaying READ canTogglePlaying NOTIFY capsChanged)
  Q_PROPERTY(bool canControl READ canControl NOTIFY capsChanged)

public:
  explicit MprisPlayer(const QString &busName, QObject *parent = nullptr);

  QString busName() const { return m_busName; }
  QString identity() const;
  QString desktopEntry() const { return m_desktopEntry; }
  bool isPlaying() const { return m_status == QLatin1String("Playing"); }
  QString playbackStatus() const { return m_status; }
  QString trackTitle() const { return m_title; }
  QString trackArtist() const { return m_artist; }
  QString trackAlbum() const { return m_album; }
  QString trackArtUrl() const { return m_artUrl; }
  double length() const { return double(m_lengthUs) / 1e6; }
  double position() const { return double(currentUs()) / 1e6; }
  double rate() const { return m_rate; }
  bool canGoNext() const { return m_canNext; }
  bool canGoPrevious() const { return m_canPrev; }
  bool canSeek() const { return m_canSeek; }
  bool canTogglePlaying() const { return m_canPlay || m_canPause; }
  bool canControl() const { return m_canControl; }
  bool trackPosition() const { return m_track; }

  void setTrackPosition(bool on);
  void setPosition(double seconds);

  Q_INVOKABLE void togglePlaying();
  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();

  void start();

signals:
  void ready();
  void failed();
  void identityChanged();
  void playbackChanged();
  void trackChanged();
  void trackPositionChanged();
  void positionChanged();
  void rateChanged();
  void capsChanged();

private slots:
  void onPropertiesChanged(const QString &iface, const QVariantMap &changed,
                           const QStringList &invalidated);
  void onSeeked(qlonglong us);

private:
  using Done = std::function<void(bool ok, const QVariantMap &props)>;

  void getAll(const QString &iface, Done done);
  void refetchPlayer();
  void fetchPosition();
  void applyRoot(const QVariantMap &m);
  void applyPlayer(const QVariantMap &m, bool initial);
  void finishLoad(bool ok);
  void callPlayer(const QString &method, const QVariantList &args = {});
  void snapshot();
  void setBase(qint64 us);
  void updateResync();
  qint64 currentUs() const;
  bool hasTrackPath() const;

  QDBusConnection m_bus;
  QString m_busName;

  QString m_identity;
  QString m_desktopEntry;
  QString m_status;
  QString m_title;
  QString m_artist;
  QString m_album;
  QString m_artUrl;
  QString m_trackId;
  qint64 m_lengthUs = 0;
  double m_rate = 1.0;

  bool m_canNext = false;
  bool m_canPrev = false;
  bool m_canSeek = false;
  bool m_canPlay = false;
  bool m_canPause = false;
  bool m_canControl = false;

  qint64 m_posUs = 0;
  qint64 m_stamp = 0;
  QElapsedTimer m_clock;
  QTimer m_resync;
  QTimer m_tick;
  bool m_track = false;

  int m_loading = 0;
  bool m_ready = false;
  bool m_failed = false;
};

class Mpris : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

  Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged)
  Q_PROPERTY(MprisPlayer *active READ active NOTIFY activeChanged)

public:
  explicit Mpris(QObject *parent = nullptr);

  QVariantList players() const;
  MprisPlayer *active() const { return m_active; }

signals:
  void playersChanged();
  void activeChanged();

private slots:
  void onNameOwnerChanged(const QString &name, const QString &oldOwner,
                          const QString &newOwner);

private:
  void add(const QString &name);
  void remove(const QString &name);
  void promote(MprisPlayer *p);
  void onPlayback(MprisPlayer *p);
  void pickActive();
  void setActive(MprisPlayer *p);

  QDBusConnection m_bus;
  QList<MprisPlayer *> m_players;
  QHash<QString, MprisPlayer *> m_pending;
  MprisPlayer *m_active = nullptr;
};
