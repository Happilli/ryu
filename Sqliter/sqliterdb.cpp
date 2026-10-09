#include "./sqliterdb.h"
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

QSqlDatabase SqliterDb::connection() {
  if (QSqlDatabase::contains("sqliter_conn")) {
    return QSqlDatabase::database("sqliter_conn");
  }
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/QML/OfflineStorage/Databases";
  QDir().mkpath(dir);

  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "sqliter_conn");
  db.setDatabaseName(dir + "/ryu.sqlite");

  if (!db.open()) {
    qWarning() << "failed to open db" << db.lastError().text();
    return db;
  }

  ensureSchema(db);
  return db;
}

void SqliterDb::ensureSchema(QSqlDatabase &db) {
  QSqlQuery q(db);
  if (!q.exec("create table if not exists events ("
              "month int, day int, title text, description text, "
              "unique(month, day))")) {
    qWarning() << "Failed to ensure events table" << q.lastError().text();
  }
}
