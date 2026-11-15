#include "./systemclock.hpp"

SystemClock::SystemClock(QObject *parent) : QObject(parent) {
  m_timer.setSingleShot(true);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, &SystemClock::tick);
  m_date = truncated(QDateTime::currentDateTime());
  tick();
}

void SystemClock::setEnabled(bool enabled) {
  if (m_enabled == enabled) {
    return;
  }
  m_enabled = enabled;
  emit enabledChanged();
  if (m_enabled) {
    tick();
  } else {
    m_timer.stop();
  }
}

void SystemClock::setPrecision(Precision precision) {
  if (m_precision == precision) {
    return;
  }
  m_precision = precision;
  emit precisionChanged();
  tick();
}

QDateTime SystemClock::truncated(const QDateTime &now) const {
  const QTime time = now.time();
  QDateTime out = now;
  out.setTime(QTime(time.hour(), m_precision >= Minutes ? time.minute() : 0,
                    m_precision == Seconds ? time.second() : 0, 0));
  return out;
}

void SystemClock::tick() {
  if (!m_enabled) {
    return;
  }
  const QDateTime now = QDateTime::currentDateTime();
  const QDateTime current = truncated(now);
  if (current != m_date) {
    m_date = current;
    emit dateChanged();
  }
  const qint64 step = m_precision == Hours     ? 60 * 60 * 1000
                      : m_precision == Minutes ? 60 * 1000
                                               : 1000;
  m_timer.start(int(qMax<qint64>(now.msecsTo(current.addMSecs(step)), 1)));
}
