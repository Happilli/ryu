#include "./display.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <cmath>

namespace {
const QString kRoot = QStringLiteral("/sys/class/backlight/");
}

Backlight::Backlight(QObject *parent) : QObject(parent) {
  discover();
  refresh();
}

int Backlight::readValue(const QString &file) const {
  QFile f(kRoot + m_device + QLatin1Char('/') + file);
  if (!f.open(QIODevice::ReadOnly)) {
    return -1;
  }
  bool ok = false;
  const int v = f.readAll().trimmed().toInt(&ok);
  return ok ? v : -1;
}

void Backlight::discover() {
  const QStringList names =
      QDir(kRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  int best = 0;
  QString chosen;
  for (const QString &name : names) {
    m_device = name;
    const int max = readValue(QStringLiteral("max_brightness"));
    if (max > best) {
      best = max;
      chosen = name;
    }
  }
  m_device = chosen;
  m_max = best;
  if (m_max > 0) {
    emit availableChanged();
  }
}

void Backlight::refresh() {
  if (m_max <= 0 || m_inflight || m_pending >= 0) {
    return;
  }
  const int cur = readValue(QStringLiteral("brightness"));
  if (cur < 0) {
    return;
  }
  const qreal value = std::clamp(qreal(cur) / qreal(m_max), 0.0, 1.0);
  if (!qFuzzyCompare(value + 1.0, m_brightness + 1.0)) {
    m_brightness = value;
    emit brightnessChanged();
  }
}

void Backlight::setBrightness(qreal value) {
  if (m_max <= 0) {
    return;
  }
  value = std::clamp(value, 0.01, 1.0);
  if (!qFuzzyCompare(value + 1.0, m_brightness + 1.0)) {
    m_brightness = value;
    emit brightnessChanged();
  }
  m_pending = std::max(1, int(std::lround(value * m_max)));
  if (!m_inflight) {
    send();
  }
}

void Backlight::send() {
  if (m_pending < 0) {
    return;
  }
  const int target = m_pending;
  m_pending = -1;
  m_inflight = true;

  QDBusMessage msg = QDBusMessage::createMethodCall(
      QStringLiteral("org.freedesktop.login1"),
      QStringLiteral("/org/freedesktop/login1/session/auto"),
      QStringLiteral("org.freedesktop.login1.Session"),
      QStringLiteral("SetBrightness"));
  msg << QStringLiteral("backlight") << m_device << quint32(target);

  auto *watcher = new QDBusPendingCallWatcher(
      QDBusConnection::systemBus().asyncCall(msg), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<> reply = *w;
            if (reply.isError()) {
              qWarning("ryu display: SetBrightness failed: %s",
                       qPrintable(reply.error().message()));
            }
            w->deleteLater();
            m_inflight = false;
            send();
          });
}
