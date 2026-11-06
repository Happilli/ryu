#pragma once
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QList>
#include <QLocalSocket>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class Discord : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(
      QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)

  Q_PROPERTY(ActivityType activityType READ activityType WRITE setActivityType
                 NOTIFY activityChanged)
  Q_PROPERTY(
      QString details READ details WRITE setDetails NOTIFY activityChanged)
  Q_PROPERTY(QString state READ state WRITE setState NOTIFY activityChanged)
  Q_PROPERTY(QString largeImage READ largeImage WRITE setLargeImage NOTIFY
                 activityChanged)
  Q_PROPERTY(QString largeText READ largeText WRITE setLargeText NOTIFY
                 activityChanged)
  Q_PROPERTY(QString smallImage READ smallImage WRITE setSmallImage NOTIFY
                 activityChanged)
  Q_PROPERTY(QString smallText READ smallText WRITE setSmallText NOTIFY
                 activityChanged)
  Q_PROPERTY(qint64 startTimestamp READ startTimestamp WRITE setStartTimestamp
                 NOTIFY activityChanged)
  Q_PROPERTY(qint64 endTimestamp READ endTimestamp WRITE setEndTimestamp NOTIFY
                 activityChanged)
  Q_PROPERTY(
      QVariantList buttons READ buttons WRITE setButtons NOTIFY activityChanged)

public:
  enum ActivityType { Playing = 0, Listening = 2, Watching = 3, Competing = 5 };
  Q_ENUM(ActivityType)

  explicit Discord(QObject *parent = nullptr);
  ~Discord() override;

  QString clientId() const { return m_clientId; }
  bool enabled() const { return m_enabled; }
  bool connected() const { return m_ready; }
  ActivityType activityType() const { return m_type; }
  QString details() const { return m_details; }
  QString state() const { return m_state; }
  QString largeImage() const { return m_largeImage; }
  QString largeText() const { return m_largeText; }
  QString smallImage() const { return m_smallImage; }
  QString smallText() const { return m_smallText; }
  qint64 startTimestamp() const { return m_start; }
  qint64 endTimestamp() const { return m_end; }
  QVariantList buttons() const { return m_buttons; }

  void setClientId(const QString &v);
  void setEnabled(bool v);
  void setActivityType(ActivityType v) { assign(m_type, v); }
  void setDetails(const QString &v) { assign(m_details, v); }
  void setState(const QString &v) { assign(m_state, v); }
  void setLargeImage(const QString &v) { assign(m_largeImage, v); }
  void setLargeText(const QString &v) { assign(m_largeText, v); }
  void setSmallImage(const QString &v) { assign(m_smallImage, v); }
  void setSmallText(const QString &v) { assign(m_smallText, v); }
  void setStartTimestamp(qint64 v) { assign(m_start, v); }
  void setEndTimestamp(qint64 v) { assign(m_end, v); }
  void setButtons(const QVariantList &v) { assign(m_buttons, v); }

  Q_INVOKABLE void pushActivity(const QVariantMap &overrides);

signals:
  void clientIdChanged();
  void enabledChanged();
  void connectedChanged();
  void activityChanged();
  void error(const QString &message);

private:
  enum Opcode : quint32 {
    Handshake = 0,
    Frame = 1,
    Close = 2,
    Ping = 3,
    Pong = 4
  };
  struct Queued {
    QJsonObject activity;
    QByteArray key;
  };

  static constexpr qint64 kMinIntervalMs = 16000;
  static constexpr int kDebounceMs = 300;
  static constexpr int kMaxQueued = 3;

  template <typename T> void assign(T &field, const T &v) {
    if (field == v)
      return;
    field = v;
    emit activityChanged();
    refreshLatest();
  }

  QVariantMap currentMap() const;
  static QJsonObject makeActivity(const QVariantMap &m);
  static QByteArray keyOf(const QJsonObject &o);

  void watchRuntimeDirs();
  void connectToDiscord();
  void tryNextCandidate();
  void teardown();
  void onConnected();
  void onReadyRead();
  void handleFrame(quint32 op, const QByteArray &data);
  void send(quint32 op, const QJsonObject &payload);

  void refreshLatest();
  bool hasWork() const;
  void requestFlush();
  void flush();
  void sendActivity(const QJsonObject &activity);

  QLocalSocket m_socket;
  QFileSystemWatcher m_dirWatcher;
  QTimer m_flushTimer;
  QElapsedTimer m_sendClock;
  QByteArray m_buffer;
  QStringList m_candidates;
  int m_candidateIdx = -1;

  QString m_clientId;
  bool m_enabled = false;
  bool m_ready = false;
  bool m_attemptConnected = false;

  ActivityType m_type = Playing;
  QString m_details, m_state;
  QString m_largeImage, m_largeText, m_smallImage, m_smallText;
  qint64 m_start = 0, m_end = 0;
  QVariantList m_buttons;

  QList<Queued> m_queue;
  QJsonObject m_latest;
  QByteArray m_latestKey;
  QByteArray m_sentKey;
};
