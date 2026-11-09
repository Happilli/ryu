#include "notifications.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QMetaObject>
#include <QPixmap>
#include <QPointer>
#include <QQuickImageProvider>
#include <limits>

namespace {

const QString kService = QStringLiteral("org.freedesktop.Notifications");
const QString kPath = QStringLiteral("/org/freedesktop/Notifications");
const QString kProvider = QStringLiteral("ryuicon");
const QString kProviderPrefix = QStringLiteral("image://ryuicon/");
const QString kDataPrefix = QStringLiteral("notif/");

class ImageStore {
public:
  static ImageStore &instance() {
    static ImageStore store;
    return store;
  }

  void put(const QString &key, const QImage &image) {
    m_images.insert(key, image);
  }

  void remove(const QString &key) {
    if (!key.isEmpty()) {
      m_images.remove(key);
    }
  }

  QImage get(const QString &key) const { return m_images.value(key); }

private:
  QHash<QString, QImage> m_images;
};

class RyuImageProvider : public QQuickImageProvider {
public:
  RyuImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override {
    QPixmap pixmap;
    if (id.startsWith(kDataPrefix)) {
      pixmap = QPixmap::fromImage(ImageStore::instance().get(id));
    } else {
      const QIcon icon = QIcon::fromTheme(id);
      if (!icon.isNull()) {
        const int edge = requestedSize.width() > 0 ? requestedSize.width() : 64;
        pixmap = icon.pixmap(QSize(edge, edge));
      }
    }
    if (size) {
      *size = pixmap.size();
    }
    return pixmap;
  }
};

void ensureImageProvider(QQmlEngine *engine) {
  if (!engine || engine->imageProvider(kProvider)) {
    return;
  }
  engine->addImageProvider(kProvider, new RyuImageProvider);
}

QString resolveIconSource(const QString &name) {
  if (name.isEmpty()) {
    return {};
  }
  if (name.startsWith(QLatin1Char('/'))) {
    return QStringLiteral("file://") + name;
  }
  if (name.contains(QLatin1String("://"))) {
    return name;
  }
  if (QIcon::hasThemeIcon(name)) {
    return kProviderPrefix + name;
  }
  return {};
}

QImage decodeImage(const QVariant &value) {
  if (value.userType() != qMetaTypeId<QDBusArgument>()) {
    return {};
  }
  const QDBusArgument arg = value.value<QDBusArgument>();
  int width = 0;
  int height = 0;
  int stride = 0;
  int bits = 0;
  int channels = 0;
  bool alpha = false;
  QByteArray data;
  arg.beginStructure();
  arg >> width >> height >> stride >> alpha >> bits >> channels >> data;
  arg.endStructure();
  if (width <= 0 || height <= 0 || bits != 8 ||
      (channels != 3 && channels != 4)) {
    return {};
  }
  if (stride < width * channels) {
    return {};
  }
  const qsizetype needed =
      qsizetype(stride) * (height - 1) + qsizetype(width) * channels;
  if (data.size() < needed) {
    return {};
  }
  const QImage::Format format =
      channels == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
  return QImage(reinterpret_cast<const uchar *>(data.constData()), width,
                height, stride, format)
      .copy();
}

QImage imageFromHints(const QVariantMap &hints) {
  static const QStringList keys = {QStringLiteral("image-data"),
                                   QStringLiteral("image_data"),
                                   QStringLiteral("icon_data")};
  for (const QString &key : keys) {
    const auto it = hints.constFind(key);
    if (it != hints.constEnd()) {
      const QImage image = decodeImage(it.value());
      if (!image.isNull()) {
        return image;
      }
    }
  }
  return {};
}

QString pathFromHints(const QVariantMap &hints) {
  const QString path = hints.value(QStringLiteral("image-path")).toString();
  if (!path.isEmpty()) {
    return path;
  }
  return hints.value(QStringLiteral("image_path")).toString();
}

NotificationData parseData(const QString &appName, const QString &appIcon,
                           const QString &summary, const QString &body,
                           const QStringList &actions, const QVariantMap &hints,
                           int expireTimeout) {
  NotificationData data;
  data.appName = appName;
  data.appIcon = appIcon;
  data.summary = summary;
  data.body = body;
  data.expireTimeout = expireTimeout;
  data.urgency =
      qBound(0, hints.value(QStringLiteral("urgency"), 1).toInt(), 2);
  data.transient = hints.value(QStringLiteral("transient")).toBool();
  data.resident = hints.value(QStringLiteral("resident")).toBool();
  data.desktopEntry = hints.value(QStringLiteral("desktop-entry")).toString();
  data.replyPlaceholder =
      hints.value(QStringLiteral("x-kde-reply-placeholder-text")).toString();
  for (int i = 0; i + 1 < actions.size(); i += 2) {
    const QString &key = actions.at(i);
    const QString &label = actions.at(i + 1);
    if (key == QLatin1String("inline-reply")) {
      data.hasInlineReply = true;
      continue;
    }
    data.actions.append({key, label});
  }
  return data;
}

} // namespace

NotificationAction::NotificationAction(Notification *notification,
                                       const QString &identifier,
                                       const QString &text)
    : QObject(notification), m_notification(notification),
      m_identifier(identifier), m_text(text) {}

void NotificationAction::invoke() {
  m_notification->invokeAction(m_identifier);
}

Notification::Notification(NotificationServer *server, int id)
    : QObject(server), m_server(server), m_id(id) {}

Notification::~Notification() { ImageStore::instance().remove(m_imageKey); }

void Notification::setTracked(bool on) {
  if (m_tracked == on) {
    return;
  }
  m_tracked = on;
  emit trackedChanged();
  if (m_announced && m_server) {
    m_server->syncTracked(this);
  }
}

QVariantList Notification::actions() const {
  QVariantList out;
  out.reserve(m_actions.size());
  for (NotificationAction *action : m_actions) {
    out.append(QVariant::fromValue(action));
  }
  return out;
}

void Notification::dismiss() {
  if (m_server) {
    m_server->closeNotification(this, Dismissed);
  }
}

void Notification::expire() {
  if (m_server) {
    m_server->closeNotification(this, Expired);
  }
}

void Notification::sendInlineReply(const QString &text) {
  if (m_server) {
    m_server->sendReply(this, text);
  }
}

void Notification::invokeAction(const QString &identifier) {
  if (m_server) {
    m_server->invokeAction(this, identifier);
  }
}

void Notification::assign(const NotificationData &data,
                          const QString &imageKey) {
  m_appName = data.appName;
  m_appIcon = data.appIcon;
  m_summary = data.summary;
  m_body = data.body;
  m_image = data.image;
  if (m_imageKey != imageKey) {
    ImageStore::instance().remove(m_imageKey);
    m_imageKey = imageKey;
  }
  m_urgency = static_cast<Urgency>(data.urgency);
  m_expireTimeout = data.expireTimeout;
  m_desktopEntry = data.desktopEntry;
  m_replyPlaceholder = data.replyPlaceholder;
  m_transient = data.transient;
  m_resident = data.resident;
  m_hasInlineReply = data.hasInlineReply;

  for (NotificationAction *action : std::as_const(m_actions)) {
    action->deleteLater();
  }
  m_actions.clear();
  for (const auto &pair : data.actions) {
    m_actions.append(new NotificationAction(this, pair.first, pair.second));
  }

  emit contentChanged();
  emit urgencyChanged();
  emit expireTimeoutChanged();
  emit metaChanged();
  emit actionsChanged();
}

NotificationIcons::NotificationIcons(QObject *parent) : QObject(parent) {}

NotificationIcons *NotificationIcons::create(QQmlEngine *engine, QJSEngine *) {
  ensureImageProvider(engine);
  return new NotificationIcons;
}

QString NotificationIcons::resolve(const QString &name) const {
  return resolveIconSource(name);
}

NotificationServer::NotificationServer(QObject *parent) : QObject(parent) {
  m_adaptor = new NotificationsAdaptor(this);
}

NotificationServer::~NotificationServer() {
  if (!m_registered) {
    return;
  }
  QDBusConnection bus = QDBusConnection::sessionBus();
  bus.unregisterObject(kPath);
  if (m_running) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"),
        QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("ReleaseName"));
    message << kService;
    bus.asyncCall(message);
  }
}

void NotificationServer::componentComplete() {
  ensureImageProvider(qmlEngine(this));
  start();
}

void NotificationServer::start() {
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    emit error(QStringLiteral("Session bus is not available"));
    return;
  }
  if (!bus.registerObject(kPath, this, QDBusConnection::ExportAdaptors)) {
    emit error(QStringLiteral("Failed to export the notifications object"));
    return;
  }
  m_registered = true;

  QDBusMessage message = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.DBus"),
      QStringLiteral("/org/freedesktop/DBus"),
      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("RequestName"));
  message << kService << uint(4);

  auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *w) {
            const QDBusPendingReply<uint> reply = *w;
            w->deleteLater();
            if (reply.isError()) {
              emit error(reply.error().message());
              return;
            }
            const uint code = reply.value();
            if (code == 1 || code == 4) {
              setRunning(true);
              return;
            }
            emit error(QStringLiteral("org.freedesktop.Notifications is "
                                      "already owned by another daemon"));
          });
}

void NotificationServer::setRunning(bool running) {
  if (m_running == running) {
    return;
  }
  m_running = running;
  emit runningChanged();
}

void NotificationServer::assignCapability(bool &field, bool value) {
  if (field == value) {
    return;
  }
  field = value;
  emit capabilitiesChanged();
}

void NotificationServer::setExtraHints(const QStringList &hints) {
  if (m_extraHints == hints) {
    return;
  }
  m_extraHints = hints;
  emit capabilitiesChanged();
}

void NotificationServer::setKeepOnReload(bool on) {
  if (m_keepOnReload == on) {
    return;
  }
  m_keepOnReload = on;
  emit keepOnReloadChanged();
}

void NotificationServer::setMaxTracked(int count) {
  if (m_maxTracked == count) {
    return;
  }
  m_maxTracked = count;
  emit maxTrackedChanged();
  trim();
}

QVariantList NotificationServer::trackedNotifications() const {
  QVariantList out;
  out.reserve(m_tracked.size());
  for (Notification *notification : m_tracked) {
    out.append(QVariant::fromValue(notification));
  }
  return out;
}

QStringList NotificationServer::capabilities() const {
  QStringList caps;
  if (m_actionsSupported) {
    caps << QStringLiteral("actions");
  }
  if (m_actionIconsSupported) {
    caps << QStringLiteral("action-icons");
  }
  if (m_bodySupported) {
    caps << QStringLiteral("body");
  }
  if (m_bodyHyperlinksSupported) {
    caps << QStringLiteral("body-hyperlinks");
  }
  if (m_bodyImagesSupported) {
    caps << QStringLiteral("body-images");
  }
  if (m_bodyMarkupSupported) {
    caps << QStringLiteral("body-markup");
  }
  if (m_imageSupported) {
    caps << QStringLiteral("icon-static");
  }
  if (m_persistenceSupported) {
    caps << QStringLiteral("persistence");
  }
  if (m_inlineReplySupported) {
    caps << QStringLiteral("inline-reply");
  }
  caps << m_extraHints;
  return caps;
}

int NotificationServer::allocateId() {
  if (m_nextId <= 0 || m_nextId == std::numeric_limits<int>::max()) {
    m_nextId = 1;
  }
  return m_nextId++;
}

Notification *NotificationServer::findTracked(int id) const {
  for (Notification *notification : m_tracked) {
    if (notification->id() == id && !notification->m_closing) {
      return notification;
    }
  }
  return nullptr;
}

void NotificationServer::trim() {
  while (m_maxTracked > 0 && m_tracked.size() > m_maxTracked) {
    closeNotification(m_tracked.first(), Notification::Expired);
  }
}

uint NotificationServer::handleNotify(
    const QString &appName, uint replacesId, const QString &appIcon,
    const QString &summary, const QString &body, const QStringList &actions,
    const QVariantMap &hints, int expireTimeout) {
  NotificationData data =
      parseData(appName, appIcon, summary, body, actions, hints, expireTimeout);

  Notification *target =
      replacesId != 0 ? findTracked(static_cast<int>(replacesId)) : nullptr;
  const int id = target ? target->id() : allocateId();

  QString imageKey;
  const QImage image = imageFromHints(hints);
  if (!image.isNull()) {
    imageKey = kDataPrefix + QString::number(id) + QLatin1Char('/') +
               QString::number(++m_imageSerial);
    ImageStore::instance().put(imageKey, image);
    data.image = kProviderPrefix + imageKey;
  } else {
    data.image = resolveIconSource(pathFromHints(hints));
  }

  if (target) {
    target->assign(data, imageKey);
    return static_cast<uint>(id);
  }

  auto *created = new Notification(this, id);
  created->assign(data, imageKey);
  emit notification(created);
  created->m_announced = true;

  if (created->m_tracked) {
    m_tracked.append(created);
    trim();
    emit trackedNotificationsChanged();
  } else {
    QPointer<Notification> guard(created);
    QMetaObject::invokeMethod(
        this,
        [this, guard]() {
          if (guard) {
            closeNotification(guard, Notification::Dismissed);
          }
        },
        Qt::QueuedConnection);
  }
  return static_cast<uint>(id);
}

void NotificationServer::handleClose(uint id) {
  if (Notification *notification = findTracked(static_cast<int>(id))) {
    closeNotification(notification, Notification::Closed);
  }
}

void NotificationServer::closeNotification(Notification *notification,
                                           Notification::CloseReason reason) {
  if (!notification || notification->m_closing) {
    return;
  }
  notification->m_closing = true;
  const bool listed = m_tracked.removeOne(notification);
  emit notification->closed(static_cast<int>(reason));
  emit m_adaptor->NotificationClosed(static_cast<uint>(notification->id()),
                                     static_cast<uint>(reason));
  notification->deleteLater();
  if (listed) {
    emit trackedNotificationsChanged();
  }
}

void NotificationServer::invokeAction(Notification *notification,
                                      const QString &identifier) {
  emit m_adaptor->ActionInvoked(static_cast<uint>(notification->id()),
                                identifier);
  if (!notification->resident()) {
    closeNotification(notification, Notification::Dismissed);
  }
}

void NotificationServer::sendReply(Notification *notification,
                                   const QString &text) {
  emit m_adaptor->NotificationReplied(static_cast<uint>(notification->id()),
                                      text);
}

void NotificationServer::syncTracked(Notification *notification) {
  if (!notification->m_tracked) {
    closeNotification(notification, Notification::Dismissed);
  }
}

NotificationsAdaptor::NotificationsAdaptor(NotificationServer *server)
    : QDBusAbstractAdaptor(server), m_server(server) {}

QStringList NotificationsAdaptor::GetCapabilities() {
  return m_server->capabilities();
}

uint NotificationsAdaptor::Notify(const QString &appName, uint replacesId,
                                  const QString &appIcon,
                                  const QString &summary, const QString &body,
                                  const QStringList &actions,
                                  const QVariantMap &hints, int expireTimeout) {
  return m_server->handleNotify(appName, replacesId, appIcon, summary, body,
                                actions, hints, expireTimeout);
}

void NotificationsAdaptor::CloseNotification(uint id) {
  m_server->handleClose(id);
}

QString NotificationsAdaptor::GetServerInformation(QString &vendor,
                                                   QString &version,
                                                   QString &specVersion) {
  vendor = QStringLiteral("Ryu");
  version = QStringLiteral("1.0");
  specVersion = QStringLiteral("1.2");
  return QStringLiteral("Ryu");
}
