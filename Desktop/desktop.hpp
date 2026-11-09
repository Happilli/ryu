#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class QFileSystemWatcher;

struct DesktopEntryData {
  QString id;
  QString name;
  QString genericName;
  QString comment;
  QString icon;
  QString execString;
  QString workingDirectory;
  QString startupClass;
  QStringList categories;
  QStringList keywords;
  QStringList command;
  bool noDisplay = false;
  bool runInTerminal = false;

  bool operator==(const DesktopEntryData &other) const = default;
};

struct DesktopScan {
  QList<DesktopEntryData> entries;
  QStringList dirs;
};

class DesktopEntry : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("DesktopEntry objects are provided by DesktopEntries")
  Q_PROPERTY(QString id READ id CONSTANT)
  Q_PROPERTY(QString name READ name CONSTANT)
  Q_PROPERTY(QString genericName READ genericName CONSTANT)
  Q_PROPERTY(QString comment READ comment CONSTANT)
  Q_PROPERTY(QString icon READ icon CONSTANT)
  Q_PROPERTY(QStringList categories READ categories CONSTANT)
  Q_PROPERTY(QStringList keywords READ keywords CONSTANT)
  Q_PROPERTY(bool noDisplay READ noDisplay CONSTANT)
  Q_PROPERTY(QString execString READ execString CONSTANT)
  Q_PROPERTY(QStringList command READ command CONSTANT)
  Q_PROPERTY(QString workingDirectory READ workingDirectory CONSTANT)
  Q_PROPERTY(bool runInTerminal READ runInTerminal CONSTANT)
  Q_PROPERTY(QString startupClass READ startupClass CONSTANT)

public:
  DesktopEntry(const DesktopEntryData &data, QObject *parent);

  const DesktopEntryData &data() const { return m_data; }
  QString id() const { return m_data.id; }
  QString name() const { return m_data.name; }
  QString genericName() const { return m_data.genericName; }
  QString comment() const { return m_data.comment; }
  QString icon() const { return m_data.icon; }
  QStringList categories() const { return m_data.categories; }
  QStringList keywords() const { return m_data.keywords; }
  bool noDisplay() const { return m_data.noDisplay; }
  QString execString() const { return m_data.execString; }
  QStringList command() const { return m_data.command; }
  QString workingDirectory() const { return m_data.workingDirectory; }
  bool runInTerminal() const { return m_data.runInTerminal; }
  QString startupClass() const { return m_data.startupClass; }

  Q_INVOKABLE bool execute() const;

private:
  DesktopEntryData m_data;
};

class DesktopEntryList : public QObject {
  Q_OBJECT
  QML_ANONYMOUS
  Q_PROPERTY(QVariantList values READ values NOTIFY valuesChanged)

public:
  explicit DesktopEntryList(QObject *parent = nullptr);

  QVariantList values() const { return m_values; }
  void setEntries(const QList<DesktopEntry *> &entries);

signals:
  void valuesChanged();

private:
  QVariantList m_values;
};

class DesktopEntries : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(DesktopEntryList *applications READ applications CONSTANT)
  Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
  explicit DesktopEntries(QObject *parent = nullptr);

  DesktopEntryList *applications();
  bool ready();

  Q_INVOKABLE DesktopEntry *byId(const QString &id);
  Q_INVOKABLE DesktopEntry *heuristicLookup(const QString &name);
  Q_INVOKABLE void reload();

signals:
  void readyChanged();

private:
  void ensureLoaded();
  void startScan();
  void apply(const DesktopScan &scan);
  void rebuildIndex();

  DesktopEntryList *m_list;
  QFileSystemWatcher *m_watcher = nullptr;
  QTimer m_debounce;
  QList<DesktopEntry *> m_entries;
  QHash<QString, DesktopEntry *> m_byId;
  QHash<QString, DesktopEntry *> m_byLowerId;
  QHash<QString, DesktopEntry *> m_byClass;
  QHash<QString, DesktopEntry *> m_byName;
  QHash<QString, DesktopEntry *> m_byLastSegment;
  bool m_started = false;
  bool m_scanning = false;
  bool m_dirty = false;
  bool m_ready = false;
};

class DesktopIcons : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON

public:
  explicit DesktopIcons(QObject *parent = nullptr);
  static DesktopIcons *create(QQmlEngine *engine, QJSEngine *scriptEngine);

  Q_INVOKABLE QString path(const QString &name,
                           const QString &fallback = QString()) const;
  Q_INVOKABLE bool has(const QString &name) const;
};
