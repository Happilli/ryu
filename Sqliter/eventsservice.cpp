#include "./eventsservice.h"
#include "./sqliterdb.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

EventsService::EventsService(QObject *parent) : QObject(parent) {}

QVariantMap EventsService::eventsGet(int month, int day) {
  QVariantMap result;
  QSqlDatabase db = SqliterDb::connection();

  if (!db.isOpen())
    return result;

  QSqlQuery q(db);
  if (!q.prepare("select title, description from events where month = ? and "
                 "day = ?")) {
    qWarning() << "Prepare Failed" << q.lastError().text();
    return result;
  }
  q.addBindValue(month);
  q.addBindValue(day);

  if (!q.exec()) {
    qWarning() << "Failed to get: " << q.lastError().text();
    return result;
  }

  if (q.next()) {
    result["title"] = q.value(0).toString();
    result["description"] = q.value(1).toString();
  }

  return result;
}

void EventsService::eventsSave(int month, int day, const QString &title,
                               const QString &description) {
  QSqlDatabase db = SqliterDb::connection();
  if (!db.isOpen()) {
    return;
  }

  QSqlQuery ins(db);
  ins.prepare("insert or replace into events (month, day, title, description) "
              "values (?, ?, ?, ?)");
  ins.addBindValue(month);
  ins.addBindValue(day);
  ins.addBindValue(title);
  ins.addBindValue(description);
  if (!ins.exec()) {
    qWarning() << "Insertion failure: " << ins.lastError().text();
  }
}

void EventsService::eventsRemove(int month, int day) {
  QSqlDatabase db = SqliterDb::connection();
  if (!db.isOpen()) {
    return;
  }

  QSqlQuery q(db);
  q.prepare("delete from events where month = ? and day = ?");
  q.addBindValue(month);
  q.addBindValue(day);
  if (!q.exec()) {
    qWarning() << "Events removal failure: " << q.lastError().text();
  }
}
