#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QTimer>

class SystemClock : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
  Q_PROPERTY(Precision precision READ precision WRITE setPrecision NOTIFY
                 precisionChanged)
  Q_PROPERTY(QDateTime date READ date NOTIFY dateChanged)
  Q_PROPERTY(int hours READ hours NOTIFY dateChanged)
  Q_PROPERTY(int minutes READ minutes NOTIFY dateChanged)
  Q_PROPERTY(int seconds READ seconds NOTIFY dateChanged)

public:
  enum Precision { Hours = 1, Minutes = 2, Seconds = 3 };
  Q_ENUM(Precision)

  explicit SystemClock(QObject *parent = nullptr);

  bool enabled() const { return m_enabled; }
  void setEnabled(bool enabled);
  Precision precision() const { return m_precision; }
  void setPrecision(Precision precision);
  QDateTime date() const { return m_date; }
  int hours() const { return m_date.time().hour(); }
  int minutes() const { return m_date.time().minute(); }
  int seconds() const { return m_date.time().second(); }

signals:
  void enabledChanged();
  void precisionChanged();
  void dateChanged();

private:
  QDateTime truncated(const QDateTime &now) const;
  void tick();

  QTimer m_timer;
  QDateTime m_date;
  Precision m_precision = Seconds;
  bool m_enabled = true;
};
