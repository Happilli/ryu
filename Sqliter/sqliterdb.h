#pragma once
#include <QSqlDatabase>
#include <QString>

class SqliterDb {
public:
  static QSqlDatabase connection();

private:
  static void ensureSchema(QSqlDatabase &db);
};
