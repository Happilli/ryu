#include "./audio.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QProcess>
#include <QSocketNotifier>

#include <pipewire/extensions/metadata.h>
#include <pipewire/pipewire.h>
#include <spa/param/audio/raw.h>
#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/pod/iter.h>
#include <spa/utils/dict.h>

#include <cerrno>
#include <cmath>
#include <cstring>

namespace {

void onNodeParam(void *data, int, uint32_t id, uint32_t, uint32_t,
                 const spa_pod *param) {
  if (id == SPA_PARAM_Props)
    static_cast<PwNode *>(data)->handleParam(param);
}

const pw_node_events *nodeEvents() {
  static const pw_node_events events = [] {
    pw_node_events e{};
    e.version = PW_VERSION_NODE_EVENTS;
    e.param = onNodeParam;
    return e;
  }();
  return &events;
}

void onCoreError(void *data, uint32_t id, int, int res, const char *) {
  if (id == PW_ID_CORE && res == -EPIPE)
    static_cast<Pipewire *>(data)->handleLost();
}

const pw_core_events *coreEvents() {
  static const pw_core_events events = [] {
    pw_core_events e{};
    e.version = PW_VERSION_CORE_EVENTS;
    e.error = onCoreError;
    return e;
  }();
  return &events;
}

void onRegistryGlobal(void *data, uint32_t id, uint32_t, const char *type,
                      uint32_t, const spa_dict *props) {
  static_cast<Pipewire *>(data)->handleGlobal(id, type, props);
}

void onRegistryGlobalRemove(void *data, uint32_t id) {
  static_cast<Pipewire *>(data)->handleGlobalRemove(id);
}

const pw_registry_events *registryEvents() {
  static const pw_registry_events events = [] {
    pw_registry_events e{};
    e.version = PW_VERSION_REGISTRY_EVENTS;
    e.global = onRegistryGlobal;
    e.global_remove = onRegistryGlobalRemove;
    return e;
  }();
  return &events;
}

int onMetadataProperty(void *data, uint32_t subject, const char *key,
                       const char *, const char *value) {
  static_cast<Pipewire *>(data)->handleMetadata(subject, key, value);
  return 0;
}

const pw_metadata_events *metadataEvents() {
  static const pw_metadata_events events = [] {
    pw_metadata_events e{};
    e.version = PW_VERSION_METADATA_EVENTS;
    e.property = onMetadataProperty;
    return e;
  }();
  return &events;
}

} // namespace

PwNodeAudio::PwNodeAudio(PwNode *node) : QObject(node), m_node(node) {}

void PwNodeAudio::setVolume(qreal volume) {
  if (qAbs(m_volume - volume) < 1e-6)
    return;
  m_volume = volume;
  emit volumeChanged();
  m_node->pushVolume(volume);
}

void PwNodeAudio::setMuted(bool muted) {
  if (m_muted == muted)
    return;
  m_muted = muted;
  emit mutedChanged();
  m_node->pushMuted(muted);
}

void PwNodeAudio::applyVolume(qreal volume) {
  if (qAbs(m_volume - volume) < 1e-6)
    return;
  m_volume = volume;
  emit volumeChanged();
}

void PwNodeAudio::applyMuted(bool muted) {
  if (m_muted == muted)
    return;
  m_muted = muted;
  emit mutedChanged();
}

PwNode::PwNode(Pipewire *owner, uint32_t id, const QString &name,
               const QString &description, const QString &nickname, bool sink)
    : QObject(owner), m_owner(owner), m_id(id), m_name(name),
      m_description(description), m_nickname(nickname), m_sink(sink) {}

PwNode::~PwNode() { unbind(); }

void PwNode::acquire() {
  if (m_refs++ == 0 && !m_detached)
    bind();
}

void PwNode::release() {
  if (m_refs > 0 && --m_refs == 0)
    unbind();
}

void PwNode::detach() {
  m_detached = true;
  unbind();
}

void PwNode::bind() {
  if (m_proxy || !m_owner->registry())
    return;
  auto *proxy = static_cast<pw_node *>(pw_registry_bind(
      m_owner->registry(), m_id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
  if (!proxy)
    return;
  m_proxy = proxy;
  pw_node_add_listener(m_proxy, &m_listener, nodeEvents(), this);
  uint32_t ids[] = {SPA_PARAM_Props};
  pw_node_subscribe_params(m_proxy, ids, 1);
}

void PwNode::unbind() {
  if (!m_proxy)
    return;
  spa_hook_remove(&m_listener);
  pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_proxy));
  m_proxy = nullptr;
}

bool PwNode::writing() const {
  return (m_proc && m_proc->state() != QProcess::NotRunning) ||
         m_pendingVolume >= 0 || m_pendingMute >= 0;
}

void PwNode::handleParam(const spa_pod *param) {
  if (writing() || !param ||
      !spa_pod_is_object_type(param, SPA_TYPE_OBJECT_Props))
    return;

  const auto *obj = reinterpret_cast<const spa_pod_object *>(param);
  const spa_pod_prop *prop = nullptr;
  bool hasVolume = false;
  bool hasMute = false;
  bool mute = false;
  qreal volume = 0;

  SPA_POD_OBJECT_FOREACH(obj, prop) {
    if (prop->key == SPA_PROP_channelVolumes) {
      float values[SPA_AUDIO_MAX_CHANNELS];
      const uint32_t n = spa_pod_copy_array(&prop->value, SPA_TYPE_Float,
                                            values, SPA_AUDIO_MAX_CHANNELS);
      if (n > 0) {
        float sum = 0;
        for (uint32_t i = 0; i < n; ++i)
          sum += values[i];
        volume = std::cbrt(static_cast<double>(sum / n));
        hasVolume = true;
      }
    } else if (prop->key == SPA_PROP_mute) {
      bool b = false;
      if (spa_pod_get_bool(&prop->value, &b) == 0) {
        mute = b;
        hasMute = true;
      }
    }
  }

  if (!hasVolume && !hasMute)
    return;

  const bool created = !m_audio;
  if (created)
    m_audio = new PwNodeAudio(this);
  if (hasVolume)
    m_audio->applyVolume(volume);
  if (hasMute)
    m_audio->applyMuted(mute);
  if (created)
    emit audioChanged();
}

void PwNode::pushVolume(qreal volume) {
  m_pendingVolume = qBound<qreal>(0, volume, 1);
  flush();
}

void PwNode::pushMuted(bool muted) {
  m_pendingMute = muted ? 1 : 0;
  flush();
}

void PwNode::flush() {
  if (m_proc && m_proc->state() != QProcess::NotRunning)
    return;

  QStringList args;
  if (m_pendingVolume >= 0) {
    args = {QStringLiteral("set-volume"), QString::number(m_id),
            QString::number(m_pendingVolume, 'f', 4)};
    m_pendingVolume = -1;
  } else if (m_pendingMute >= 0) {
    args = {QStringLiteral("set-mute"), QString::number(m_id),
            m_pendingMute ? QStringLiteral("1") : QStringLiteral("0")};
    m_pendingMute = -1;
  } else {
    return;
  }

  if (!m_proc) {
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, [this] { flush(); });
    connect(m_proc, &QProcess::errorOccurred, this, [this] {
      m_pendingVolume = -1;
      m_pendingMute = -1;
    });
  }
  m_proc->start(QStringLiteral("wpctl"), args);
}

PwObjectTracker::PwObjectTracker(QObject *parent) : QObject(parent) {}

PwObjectTracker::~PwObjectTracker() { releaseAll(); }

void PwObjectTracker::releaseAll() {
  for (const QPointer<PwNode> &node : std::as_const(m_nodes)) {
    if (node)
      node->release();
  }
  m_nodes.clear();
}

void PwObjectTracker::setObjects(const QVariantList &objects) {
  QList<QPointer<PwNode>> next;
  for (const QVariant &v : objects) {
    if (auto *node = qobject_cast<PwNode *>(v.value<QObject *>()))
      next.append(node);
  }
  for (const QPointer<PwNode> &node : std::as_const(next))
    node->acquire();
  releaseAll();
  m_nodes = next;
  m_objects = objects;
  emit objectsChanged();
}

Pipewire::Pipewire(QObject *parent) : QObject(parent) {
  pw_init(nullptr, nullptr);
  connectCore();
}

Pipewire::~Pipewire() { disconnectCore(); }

void Pipewire::reconnect() {
  disconnectCore();
  connectCore();
}

bool Pipewire::connectCore() {
  m_loop = pw_loop_new(nullptr);
  if (!m_loop)
    return false;
  m_context = pw_context_new(m_loop, nullptr, 0);
  if (!m_context) {
    disconnectCore();
    return false;
  }
  m_core = pw_context_connect(m_context, nullptr, 0);
  if (!m_core) {
    disconnectCore();
    return false;
  }

  pw_core_add_listener(m_core, &m_coreListener, coreEvents(), this);
  m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
  pw_registry_add_listener(m_registry, &m_registryListener, registryEvents(),
                           this);

  pw_loop_enter(m_loop);
  m_notifier =
      new QSocketNotifier(pw_loop_get_fd(m_loop), QSocketNotifier::Read, this);
  connect(m_notifier, &QSocketNotifier::activated, this, [this] {
    if (m_loop)
      pw_loop_iterate(m_loop, 0);
  });
  pw_loop_iterate(m_loop, 0);

  emit readyChanged();
  return true;
}

void Pipewire::disconnectCore() {
  const bool wasReady = m_core != nullptr;

  delete m_notifier;
  m_notifier = nullptr;

  const QList<PwNode *> nodes = m_nodes.values();
  m_nodes.clear();
  for (PwNode *node : nodes) {
    node->detach();
    node->deleteLater();
  }
  m_sinkName.clear();
  m_sourceName.clear();
  resolveDefaults();

  dropMetadata();

  if (m_registry) {
    spa_hook_remove(&m_registryListener);
    pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_registry));
    m_registry = nullptr;
  }
  if (m_core) {
    spa_hook_remove(&m_coreListener);
    pw_core_disconnect(m_core);
    m_core = nullptr;
  }
  if (m_loop && m_context)
    pw_loop_leave(m_loop);
  if (m_context) {
    pw_context_destroy(m_context);
    m_context = nullptr;
  }
  if (m_loop) {
    pw_loop_destroy(m_loop);
    m_loop = nullptr;
  }

  if (wasReady)
    emit readyChanged();
}

void Pipewire::dropMetadata() {
  if (!m_metadata)
    return;
  spa_hook_remove(&m_metadataListener);
  pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_metadata));
  m_metadata = nullptr;
  m_metadataId = UINT32_MAX;
}

void Pipewire::handleGlobal(uint32_t id, const char *type,
                            const spa_dict *props) {
  if (!props)
    return;

  auto str = [props](const char *key) {
    const char *v = spa_dict_lookup(props, key);
    return v ? QString::fromUtf8(v) : QString();
  };

  if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
    const QByteArray cls = str(PW_KEY_MEDIA_CLASS).toUtf8();
    const bool sink = cls.startsWith("Audio/Sink");
    const bool source = cls.startsWith("Audio/Source");
    if ((!sink && !source) || m_nodes.contains(id))
      return;
    const QString name = str(PW_KEY_NODE_NAME);
    if (name.isEmpty())
      return;
    auto *node = new PwNode(this, id, name, str(PW_KEY_NODE_DESCRIPTION),
                            str(PW_KEY_NODE_NICK), sink);
    m_nodes.insert(id, node);
    resolveDefaults();
    return;
  }

  if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0 && !m_metadata &&
      str(PW_KEY_METADATA_NAME) == QLatin1String("default")) {
    auto *proxy = static_cast<pw_metadata *>(pw_registry_bind(
        m_registry, id, PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, 0));
    if (!proxy)
      return;
    m_metadata = proxy;
    m_metadataId = id;
    pw_metadata_add_listener(m_metadata, &m_metadataListener, metadataEvents(),
                             this);
  }
}

void Pipewire::handleGlobalRemove(uint32_t id) {
  if (PwNode *node = m_nodes.take(id)) {
    node->detach();
    resolveDefaults();
    node->deleteLater();
    return;
  }
  if (m_metadata && id == m_metadataId) {
    dropMetadata();
    m_sinkName.clear();
    m_sourceName.clear();
    resolveDefaults();
  }
}

void Pipewire::handleMetadata(uint32_t subject, const char *key,
                              const char *value) {
  if (subject != PW_ID_CORE)
    return;

  bool changed = false;
  if (!key) {
    changed = !m_sinkName.isEmpty() || !m_sourceName.isEmpty();
    m_sinkName.clear();
    m_sourceName.clear();
  } else {
    QString *target = nullptr;
    if (std::strcmp(key, "default.audio.sink") == 0)
      target = &m_sinkName;
    else if (std::strcmp(key, "default.audio.source") == 0)
      target = &m_sourceName;
    if (!target)
      return;
    QString name;
    if (value)
      name = QJsonDocument::fromJson(QByteArray(value))
                 .object()
                 .value(QStringLiteral("name"))
                 .toString();
    if (*target != name) {
      *target = name;
      changed = true;
    }
  }

  if (changed)
    resolveDefaults();
}

void Pipewire::handleLost() {
  QMetaObject::invokeMethod(this, &Pipewire::disconnectCore,
                            Qt::QueuedConnection);
}

PwNode *Pipewire::find(const QString &name, bool sink) const {
  if (name.isEmpty())
    return nullptr;
  for (PwNode *node : m_nodes) {
    if (node->isSink() == sink && node->name() == name)
      return node;
  }
  return nullptr;
}

void Pipewire::resolveDefaults() {
  PwNode *sink = find(m_sinkName, true);
  if (sink != m_sink.data()) {
    m_sink = sink;
    emit defaultAudioSinkChanged();
  }
  PwNode *source = find(m_sourceName, false);
  if (source != m_source.data()) {
    m_source = source;
    emit defaultAudioSourceChanged();
  }
}
