#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <qqmlregistration.h>

#include <spa/utils/hook.h>

struct pw_context;
struct pw_core;
struct pw_loop;
struct pw_metadata;
struct pw_node;
struct pw_registry;
struct spa_dict;
struct spa_pod;

class QProcess;
class QSocketNotifier;
class Pipewire;
class PwNode;

class PwNodeAudio : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
  Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

public:
  explicit PwNodeAudio(PwNode *node);

  qreal volume() const { return m_volume; }
  bool muted() const { return m_muted; }
  void setVolume(qreal volume);
  void setMuted(bool muted);
  void applyVolume(qreal volume);
  void applyMuted(bool muted);

signals:
  void volumeChanged();
  void mutedChanged();

private:
  PwNode *m_node;
  qreal m_volume = 1.0;
  bool m_muted = false;
};

class PwNode : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("PwNode is created by Pipewire")
  Q_PROPERTY(int id READ id CONSTANT)
  Q_PROPERTY(QString name READ name CONSTANT)
  Q_PROPERTY(QString description READ description CONSTANT)
  Q_PROPERTY(QString nickname READ nickname CONSTANT)
  Q_PROPERTY(bool isSink READ isSink CONSTANT)
  Q_PROPERTY(PwNodeAudio *audio READ audio NOTIFY audioChanged)

public:
  PwNode(Pipewire *owner, uint32_t id, const QString &name,
         const QString &description, const QString &nickname, bool sink);
  ~PwNode() override;

  int id() const { return static_cast<int>(m_id); }
  QString name() const { return m_name; }
  QString description() const { return m_description; }
  QString nickname() const { return m_nickname; }
  bool isSink() const { return m_sink; }
  PwNodeAudio *audio() const { return m_audio; }

  void acquire();
  void release();
  void detach();
  void handleParam(const spa_pod *param);
  void pushVolume(qreal volume);
  void pushMuted(bool muted);

signals:
  void audioChanged();

private:
  void bind();
  void unbind();
  void flush();
  bool writing() const;

  Pipewire *m_owner;
  uint32_t m_id;
  QString m_name;
  QString m_description;
  QString m_nickname;
  bool m_sink;
  int m_refs = 0;
  bool m_detached = false;
  qreal m_pendingVolume = -1;
  int m_pendingMute = -1;
  QProcess *m_proc = nullptr;
  pw_node *m_proxy = nullptr;
  spa_hook m_listener{};
  PwNodeAudio *m_audio = nullptr;
};

class PwObjectTracker : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(
      QVariantList objects READ objects WRITE setObjects NOTIFY objectsChanged)

public:
  explicit PwObjectTracker(QObject *parent = nullptr);
  ~PwObjectTracker() override;

  QVariantList objects() const { return m_objects; }
  void setObjects(const QVariantList &objects);

signals:
  void objectsChanged();

private:
  void releaseAll();

  QVariantList m_objects;
  QList<QPointer<PwNode>> m_nodes;
};

class Pipewire : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(PwNode *defaultAudioSink READ defaultAudioSink NOTIFY
                 defaultAudioSinkChanged)
  Q_PROPERTY(PwNode *defaultAudioSource READ defaultAudioSource NOTIFY
                 defaultAudioSourceChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
  explicit Pipewire(QObject *parent = nullptr);
  ~Pipewire() override;

  PwNode *defaultAudioSink() const { return m_sink; }
  PwNode *defaultAudioSource() const { return m_source; }
  bool ready() const { return m_core != nullptr; }

  Q_INVOKABLE void reconnect();

  pw_core *core() const { return m_core; }
  pw_registry *registry() const { return m_registry; }

  void handleGlobal(uint32_t id, const char *type, const spa_dict *props);
  void handleGlobalRemove(uint32_t id);
  void handleMetadata(uint32_t subject, const char *key, const char *value);
  void handleLost();

signals:
  void defaultAudioSinkChanged();
  void defaultAudioSourceChanged();
  void readyChanged();

private:
  bool connectCore();
  void disconnectCore();
  void dropMetadata();
  void resolveDefaults();
  PwNode *find(const QString &name, bool sink) const;

  pw_loop *m_loop = nullptr;
  pw_context *m_context = nullptr;
  pw_core *m_core = nullptr;
  pw_registry *m_registry = nullptr;
  pw_metadata *m_metadata = nullptr;
  uint32_t m_metadataId = UINT32_MAX;
  spa_hook m_coreListener{};
  spa_hook m_registryListener{};
  spa_hook m_metadataListener{};
  QSocketNotifier *m_notifier = nullptr;
  QHash<uint32_t, PwNode *> m_nodes;
  QString m_sinkName;
  QString m_sourceName;
  QPointer<PwNode> m_sink;
  QPointer<PwNode> m_source;
};
