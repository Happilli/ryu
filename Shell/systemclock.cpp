#include "./systemclock.hpp"

SystemClock::SystemClock(QObject *parent) : QObject(parent) {
  m_timer.setSingleShot(true);
  m_timer.setTimerType(Qt::PreciseTimer);
  connect(&m_timer, &QTimer::timeout, this, [this]() { tick(); });
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
  QDateTime out = now;
  QTime time = now.time();
  switch (m_precision) {
  case Hours:
    time = QTime(time.hour(), 0, 0, 0);
    break;
  case Minutes:
    time = QTime(time.hour(), time.minute(), 0, 0);
    break;
  case Seconds:
    time = QTime(time.hour(), time.minute(), time.second(), 0);
    break;
  }
  out.setTime(time);
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
  qint64 step = 1000;
  if (m_precision == Minutes) {
    step = 60 * 1000;
  } else if (m_precision == Hours) {
    step = 60 * 60 * 1000;
  }
  const qint64 wait = now.msecsTo(current.addMSecs(step));
  m_timer.start(int(qMax<qint64>(wait, 1)));
}
