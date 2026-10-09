#include "warsa.hpp"

static const char *MONTH_NAMES[] = {"baisakh", "jestha", "asar",   "shaun",
                                    "vadau",   "ashoj",  "kartik", "mangsir",
                                    "poush",   "magh",   "falgun", "chaitra"};

static const char *DAY_NAMES[] = {"aitawar", "somwar",   "mangawar", "budhawar",
                                  "vihiwar", "sukrawar", "saniwar"};

const int CURRENT_BS_YEAR = 2083;
const int MONTH_DAYS[12] = {31, 31, 32, 31, 31, 31, 30, 29, 30, 29, 30, 30};

const QDate REFERENCE_AD_DATE(2026, 4, 14);
const int REFERENCE_BS_MONTH = 1;
const int REFERENCE_BS_DAY = 1;

const int REFERENCE_WEEKDAY = 2;

Warsa::Warsa(QObject *parent)
    : QObject(parent), m_year(CURRENT_BS_YEAR), m_month(1), m_day(1),
      m_weekDay(0) {

  QDate today = QDate::currentDate();
  int daysDiff = REFERENCE_AD_DATE.daysTo(today);

  int month = REFERENCE_BS_MONTH;
  int day = REFERENCE_BS_DAY + daysDiff;

  while (day > MONTH_DAYS[month - 1]) {
    day -= MONTH_DAYS[month - 1];
    month++;
    if (month > 12) {
      month = 1;
    }
  }

  m_todayMonth = month;
  m_todayDay = day;
}

bool Warsa::isToday(int year, int month, int day) const {
  return (month == m_todayMonth && day == m_todayDay);
}

void Warsa::setToday() {
  QDate today = QDate::currentDate();
  int daysDiff = REFERENCE_AD_DATE.daysTo(today);

  int year = CURRENT_BS_YEAR;
  int month = REFERENCE_BS_MONTH;
  int day = REFERENCE_BS_DAY + daysDiff;

  while (day < 1) {
    month--;
    if (month < 1) {
      month = 12;
      year--;
    }
    day += MONTH_DAYS[month - 1];
  }

  while (day > MONTH_DAYS[month - 1]) {
    day -= MONTH_DAYS[month - 1];
    month++;
    if (month > 12) {
      month = 1;
      year++;
    }
  }

  m_year = year;
  m_month = month;
  m_day = day;
  updateWeekDay();

  emit yearChanged();
  emit monthChanged();
  emit dayChanged();
}

void Warsa::setYear(int year) {
  if (m_year == year) {
    return;
  }
  m_year = year;
  emit yearChanged();
  emit monthChanged();
}

void Warsa::setDate(int year, int month, int day) {
  if (m_year != year) {
    setYear(year);
  }

  if (month < 1 || month > 12) {
    return;
  }

  if (day < 1 || day > MONTH_DAYS[month - 1]) {
    return;
  }

  bool monthChanged = (m_month != month);
  bool dayChanged = (m_day != day);

  m_month = month;
  m_day = day;
  updateWeekDay();

  if (monthChanged)
    emit this->monthChanged();
  if (dayChanged)
    emit this->dayChanged();
}

void Warsa::nextMonth() {
  m_month++;
  if (m_month > 12) {
    m_month = 12;
  }
  int maxDays = MONTH_DAYS[m_month - 1];
  if (m_day > maxDays) {
    m_day = maxDays;
    emit dayChanged();
  }
  updateWeekDay();
  emit monthChanged();
}

void Warsa::previousMonth() {
  m_month--;
  if (m_month < 1) {
    m_month = 1;
  }
  int maxDays = MONTH_DAYS[m_month - 1];
  if (m_day > maxDays) {
    m_day = maxDays;
    emit dayChanged();
  }
  updateWeekDay();
  emit monthChanged();
}

void Warsa::nextDay() {
  m_day++;
  if (m_day > MONTH_DAYS[m_month - 1]) {
    m_day = 1;
    nextMonth();
  }
  updateWeekDay();
  emit dayChanged();
}

void Warsa::previousDay() {
  m_day--;
  if (m_day < 1) {
    previousMonth();
    m_day = MONTH_DAYS[m_month - 1];
  }
  updateWeekDay();
  emit dayChanged();
}

int Warsa::getDaysInMonth(int month) const {
  if (month < 1 || month > 12)
    return 30;
  return MONTH_DAYS[month - 1];
}

int Warsa::daysInCurrentMonth() const { return MONTH_DAYS[m_month - 1]; }

QString Warsa::monthName() const {
  if (m_month < 1 || m_month > 12)
    return "Unknown";
  return MONTH_NAMES[m_month - 1];
}

QString Warsa::dayName() const {
  if (m_weekDay < 0 || m_weekDay > 6)
    return "Unknown";
  return DAY_NAMES[m_weekDay];
}

void Warsa::updateWeekDay() {
  int totalDays = 0;
  for (int i = 0; i < m_month - 1; i++) {
    totalDays += MONTH_DAYS[i];
  }
  totalDays += m_day - 1;
  m_weekDay = (REFERENCE_WEEKDAY + totalDays) % 7;
}

QVariantList Warsa::getMonthDays(int month) const {
  QVariantList days;
  if (month < 1 || month > 12)
    return days;
  int totalDays = MONTH_DAYS[month - 1];
  for (int day = 1; day <= totalDays; day++) {
    QVariantMap dayInfo;
    dayInfo["day"] = day;
    // dayInfo["isToday"] = (day == m_day && month == m_month);
    dayInfo["isToday"] = isToday(CURRENT_BS_YEAR, month, day);
    int totalFromStart = 0;
    for (int i = 0; i < month - 1; i++) {
      totalFromStart += MONTH_DAYS[i];
    }
    totalFromStart += day - 1;
    int weekDay = (REFERENCE_WEEKDAY + totalFromStart) % 7;
    dayInfo["weekDay"] = weekDay;
    dayInfo["isSaturday"] = (weekDay == 6);
    days.append(dayInfo);
  }
  return days;
}

int Warsa::getFirstDayOfMonth(int month) const {
  if (month < 1 || month > 12)
    return 0;
  int totalDays = 0;
  for (int i = 0; i < month - 1; i++) {
    totalDays += MONTH_DAYS[i];
  }
  return (REFERENCE_WEEKDAY + totalDays) % 7;
}

bool Warsa::isValidYear(int year) const { return year == CURRENT_BS_YEAR; }
