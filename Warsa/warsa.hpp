/*
Nepali< calendar converter BS <-> AD: it will be useful in something like a
qml component...
*/
#pragma once
#include <QDate>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class Warsa : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(int year READ year WRITE setYear NOTIFY yearChanged)
  Q_PROPERTY(int month READ month NOTIFY monthChanged)
  Q_PROPERTY(int day READ day NOTIFY dayChanged)
  Q_PROPERTY(QString monthName READ monthName NOTIFY monthChanged)
  Q_PROPERTY(QString dayName READ dayName NOTIFY dayChanged)
  Q_PROPERTY(int daysInCurrentMonth READ daysInCurrentMonth NOTIFY monthChanged)

public:
  explicit Warsa(QObject *parent = nullptr);

  int year() const { return m_year; }
  int month() const { return m_month; }
  int day() const { return m_day; }
  int daysInCurrentMonth() const;
  QString monthName() const;
  QString dayName() const;

  void setYear(int year);

  Q_INVOKABLE void setToday();
  Q_INVOKABLE void setDate(int year, int month, int day);
  Q_INVOKABLE void nextDay();
  Q_INVOKABLE void previousDay();
  Q_INVOKABLE void nextMonth();
  Q_INVOKABLE void previousMonth();
  Q_INVOKABLE int getDaysInMonth(int month) const;

  Q_INVOKABLE QVariantList getMonthDays(int month) const;
  Q_INVOKABLE int getFirstDayOfMonth(int month) const;
  Q_INVOKABLE bool isValidYear(int year) const;

  Q_INVOKABLE bool isToday(int year, int month, int day) const;
signals:
  void yearChanged();
  void monthChanged();
  void dayChanged();

private:
  void updateWeekDay();
  int m_todayMonth;
  int m_todayDay;
  int m_year;
  int m_month;
  int m_day;
  int m_weekDay;
};
