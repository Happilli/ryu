#include "./desktop.hpp"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QLocale>
#include <QMetaObject>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QSet>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>
#include <algorithm>

namespace {

const QString kProvider = QStringLiteral("ryuapp");
const QString kProviderPrefix = QStringLiteral("image://ryuapp/");

class AppIconProvider : public QQuickImageProvider {
public:
  AppIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

  QPixmap requestPixmap(const QString &id, QSize *size,
                        const QSize &requestedSize) override {
    QPixmap pixmap;
    const QIcon icon = QIcon::fromTheme(id);
    if (!icon.isNull()) {
      const int edge = qMax(requestedSize.width(), requestedSize.height());
      const int side = edge > 0 ? edge : 64;
      pixmap = icon.pixmap(QSize(side, side));
    }
    if (size) {
      *size = pixmap.size();
    }
    return pixmap;
  }
};

void ensureProvider(QQmlEngine *engine) {
  if (!engine || engine->imageProvider(kProvider)) {
    return;
  }
  engine->addImageProvider(kProvider, new AppIconProvider);
}

QString resolveIcon(const QString &name) {
  if (name.isEmpty()) {
    return {};
  }
  if (name.startsWith(QLatin1Char('/'))) {
    return QUrl::fromLocalFile(name).toString();
  }
  if (name.contains(QLatin1String("://"))) {
    return name;
  }
  if (!QIcon::fromTheme(name).isNull()) {
    return kProviderPrefix + name;
  }
  return {};
}

QStringList localeSuffixes() {
  QStringList out;
  const QString name = QLocale::system().name();
  if (name.isEmpty() || name == QLatin1String("C")) {
    return out;
  }
  out << name;
  const int sep = name.indexOf(QLatin1Char('_'));
  if (sep > 0) {
    out << name.left(sep);
  }
  return out;
}

QString unescapeValue(const QString &value) {
  QString out;
  out.reserve(value.size());
  for (int i = 0; i < value.size(); ++i) {
    const QChar c = value.at(i);
    if (c == QLatin1Char('\\') && i + 1 < value.size()) {
      const QChar next = value.at(++i);
      switch (next.unicode()) {
      case 's':
        out += QLatin1Char(' ');
        break;
      case 'n':
        out += QLatin1Char('\n');
        break;
      case 't':
        out += QLatin1Char('\t');
        break;
      case 'r':
        out += QLatin1Char('\r');
        break;
      case '\\':
        out += QLatin1Char('\\');
        break;
      default:
        out += c;
        out += next;
        break;
      }
      continue;
    }
    out += c;
  }
  return out;
}

QStringList splitList(const QString &raw) {
  QStringList out;
  QString current;
  for (int i = 0; i < raw.size(); ++i) {
    const QChar c = raw.at(i);
    if (c == QLatin1Char('\\') && i + 1 < raw.size()) {
      const QChar next = raw.at(++i);
      if (next == QLatin1Char(';')) {
        current += next;
      } else {
        current += c;
        current += next;
      }
      continue;
    }
    if (c == QLatin1Char(';')) {
      if (!current.isEmpty()) {
        out << unescapeValue(current);
      }
      current.clear();
      continue;
    }
    current += c;
  }
  if (!current.isEmpty()) {
    out << unescapeValue(current);
  }
  return out;
}

QStringList parseExec(const QString &exec, const QString &icon,
                      const QString &name) {
  QStringList tokens;
  QString current;
  bool inQuote = false;
  bool have = false;
  auto push = [&]() {
    if (have) {
      tokens << current;
    }
    current.clear();
    have = false;
  };
  for (int i = 0; i < exec.size(); ++i) {
    const QChar c = exec.at(i);
    if (inQuote) {
      if (c == QLatin1Char('\\') && i + 1 < exec.size()) {
        current += exec.at(++i);
      } else if (c == QLatin1Char('"')) {
        inQuote = false;
      } else {
        current += c;
      }
      continue;
    }
    if (c == QLatin1Char('"')) {
      inQuote = true;
      have = true;
    } else if (c.isSpace()) {
      push();
    } else {
      current += c;
      have = true;
    }
  }
  push();

  QStringList out;
  for (const QString &token : std::as_const(tokens)) {
    if (token == QLatin1String("%i")) {
      if (!icon.isEmpty()) {
        out << QStringLiteral("--icon") << icon;
      }
      continue;
    }
    QString result;
    for (int i = 0; i < token.size(); ++i) {
      const QChar c = token.at(i);
      if (c == QLatin1Char('%') && i + 1 < token.size()) {
        const QChar code = token.at(++i);
        if (code == QLatin1Char('%')) {
          result += QLatin1Char('%');
        } else if (code == QLatin1Char('c')) {
          result += name;
        }
        continue;
      }
      result += c;
    }
    if (result.isEmpty() && !token.isEmpty()) {
      continue;
    }
    out << result;
  }
  return out;
}

bool parseFile(const QString &path, const QString &id,
               const QStringList &locales, DesktopEntryData &data,
               bool &hidden) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  const QString content = QString::fromUtf8(file.readAll());
  const QStringList lines = content.split(QLatin1Char('\n'));

  QHash<QString, QString> fields;
  bool inEntry = false;
  for (const QString &rawLine : lines) {
    const QString line = rawLine.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
      continue;
    }
    if (line.startsWith(QLatin1Char('['))) {
      inEntry = line == QLatin1String("[Desktop Entry]");
      continue;
    }
    if (!inEntry) {
      continue;
    }
    const int eq = line.indexOf(QLatin1Char('='));
    if (eq <= 0) {
      continue;
    }
    fields.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
  }

  auto get = [&](const QString &key) {
    for (const QString &locale : locales) {
      const auto it =
          fields.constFind(key + QLatin1Char('[') + locale + QLatin1Char(']'));
      if (it != fields.constEnd()) {
        return it.value();
      }
    }
    return fields.value(key);
  };

  const QString type = fields.value(QStringLiteral("Type"));
  if (!type.isEmpty() && type != QLatin1String("Application")) {
    return false;
  }

  hidden = fields.value(QStringLiteral("Hidden")) == QLatin1String("true");

  const QString name = unescapeValue(get(QStringLiteral("Name")));
  if (name.isEmpty()) {
    return false;
  }

  data.id = id;
  data.name = name;
  data.genericName = unescapeValue(get(QStringLiteral("GenericName")));
  data.comment = unescapeValue(get(QStringLiteral("Comment")));
  data.icon = unescapeValue(fields.value(QStringLiteral("Icon")));
  data.categories = splitList(fields.value(QStringLiteral("Categories")));
  data.keywords = splitList(get(QStringLiteral("Keywords")));
  data.noDisplay =
      fields.value(QStringLiteral("NoDisplay")) == QLatin1String("true");
  data.execString = fields.value(QStringLiteral("Exec"));
  data.command = parseExec(data.execString, data.icon, data.name);
  data.workingDirectory = unescapeValue(fields.value(QStringLiteral("Path")));
  data.runInTerminal =
      fields.value(QStringLiteral("Terminal")) == QLatin1String("true");
  data.startupClass =
      unescapeValue(fields.value(QStringLiteral("StartupWMClass")));
  return true;
}

DesktopScan scanApplications() {
  DesktopScan result;
  const QStringList locales = localeSuffixes();
  QSet<QString> seen;

  const QStringList roots =
      QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
  for (const QString &root : roots) {
    const QDir dir(root);
    if (!dir.exists()) {
      continue;
    }
    result.dirs << dir.absolutePath();

    QDirIterator it(root, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories |
                        QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
      const QString path = it.next();
      const QFileInfo info = it.fileInfo();
      if (info.isDir()) {
        result.dirs << info.absoluteFilePath();
        continue;
      }
      if (!path.endsWith(QLatin1String(".desktop"))) {
        continue;
      }
      QString id = dir.relativeFilePath(path);
      id.chop(8);
      id.replace(QLatin1Char('/'), QLatin1Char('-'));
      if (seen.contains(id)) {
        continue;
      }
      DesktopEntryData data;
      bool hidden = false;
      if (!parseFile(path, id, locales, data, hidden)) {
        continue;
      }
      seen.insert(id);
      if (hidden) {
        continue;
      }
      result.entries.append(data);
    }
  }

  std::sort(result.entries.begin(), result.entries.end(),
            [](const DesktopEntryData &a, const DesktopEntryData &b) {
              return a.id < b.id;
            });
  return result;
}

} // namespace

DesktopEntry::DesktopEntry(const DesktopEntryData &data, QObject *parent)
    : QObject(parent), m_data(data) {}

bool DesktopEntry::execute() const {
  if (m_data.command.isEmpty()) {
    return false;
  }
  QStringList args = m_data.command;
  const QString program = args.takeFirst();
  return QProcess::startDetached(program, args, m_data.workingDirectory);
}

DesktopEntryList::DesktopEntryList(QObject *parent) : QObject(parent) {}

void DesktopEntryList::setEntries(const QList<DesktopEntry *> &entries) {
  QVariantList values;
  values.reserve(entries.size());
  for (DesktopEntry *entry : entries) {
    values.append(QVariant::fromValue(entry));
  }
  m_values = values;
  emit valuesChanged();
}

DesktopEntries::DesktopEntries(QObject *parent)
    : QObject(parent), m_list(new DesktopEntryList(this)) {
  m_debounce.setSingleShot(true);
  m_debounce.setInterval(300);
  connect(&m_debounce, &QTimer::timeout, this, &DesktopEntries::startScan);
}

DesktopEntryList *DesktopEntries::applications() {
  ensureLoaded();
  return m_list;
}

bool DesktopEntries::ready() {
  ensureLoaded();
  return m_ready;
}

DesktopEntry *DesktopEntries::byId(const QString &id) {
  ensureLoaded();
  return m_byId.value(id);
}

DesktopEntry *DesktopEntries::heuristicLookup(const QString &name) {
  ensureLoaded();
  if (name.isEmpty()) {
    return nullptr;
  }
  if (DesktopEntry *entry = m_byId.value(name)) {
    return entry;
  }

  QString lower = name.toLower();
  if (lower.endsWith(QLatin1String(".desktop"))) {
    lower.chop(8);
  }

  if (DesktopEntry *entry = m_byLowerId.value(lower)) {
    return entry;
  }
  if (DesktopEntry *entry = m_byClass.value(lower)) {
    return entry;
  }
  if (DesktopEntry *entry = m_byName.value(lower)) {
    return entry;
  }
  if (DesktopEntry *entry = m_byLastSegment.value(lower)) {
    return entry;
  }

  QString dashed = lower;
  dashed.replace(QLatin1Char(' '), QLatin1Char('-'));
  if (DesktopEntry *entry = m_byLowerId.value(dashed)) {
    return entry;
  }

  const int dot = lower.lastIndexOf(QLatin1Char('.'));
  if (dot >= 0) {
    const QString tail = lower.mid(dot + 1);
    if (DesktopEntry *entry = m_byLowerId.value(tail)) {
      return entry;
    }
    if (DesktopEntry *entry = m_byLastSegment.value(tail)) {
      return entry;
    }
    if (DesktopEntry *entry = m_byName.value(tail)) {
      return entry;
    }
  }
  return nullptr;
}

void DesktopEntries::reload() {
  ensureLoaded();
  startScan();
}

void DesktopEntries::ensureLoaded() {
  if (m_started) {
    return;
  }
  m_started = true;
  m_watcher = new QFileSystemWatcher(this);
  connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this]() { m_debounce.start(); });
  startScan();
}

void DesktopEntries::startScan() {
  if (m_scanning) {
    m_dirty = true;
    return;
  }
  m_scanning = true;
  QPointer<DesktopEntries> self(this);
  QThreadPool::globalInstance()->start([self]() {
    const DesktopScan result = scanApplications();
    if (!self) {
      return;
    }
    QMetaObject::invokeMethod(
        self.data(),
        [self, result]() {
          if (self) {
            self->apply(result);
          }
        },
        Qt::QueuedConnection);
  });
}

void DesktopEntries::apply(const DesktopScan &scan) {
  QHash<QString, DesktopEntry *> existing;
  for (DesktopEntry *entry : std::as_const(m_entries)) {
    existing.insert(entry->id(), entry);
  }

  QList<DesktopEntry *> next;
  next.reserve(scan.entries.size());
  QSet<DesktopEntry *> kept;
  bool changed = !m_ready;

  for (const DesktopEntryData &data : scan.entries) {
    DesktopEntry *old = existing.value(data.id);
    if (old && old->data() == data) {
      next.append(old);
      kept.insert(old);
      continue;
    }
    next.append(new DesktopEntry(data, this));
    changed = true;
  }

  for (DesktopEntry *entry : std::as_const(m_entries)) {
    if (!kept.contains(entry)) {
      entry->deleteLater();
      changed = true;
    }
  }

  m_entries = next;
  rebuildIndex();

  const QStringList watched = m_watcher->directories();
  QStringList stale;
  for (const QString &dir : watched) {
    if (!scan.dirs.contains(dir)) {
      stale << dir;
    }
  }
  if (!stale.isEmpty()) {
    m_watcher->removePaths(stale);
  }
  QStringList fresh;
  for (const QString &dir : scan.dirs) {
    if (!watched.contains(dir)) {
      fresh << dir;
    }
  }
  if (!fresh.isEmpty()) {
    m_watcher->addPaths(fresh);
  }

  if (changed) {
    m_list->setEntries(m_entries);
  }

  if (!m_ready) {
    m_ready = true;
    emit readyChanged();
  }

  m_scanning = false;
  if (m_dirty) {
    m_dirty = false;
    startScan();
  }
}

void DesktopEntries::rebuildIndex() {
  m_byId.clear();
  m_byLowerId.clear();
  m_byClass.clear();
  m_byName.clear();
  m_byLastSegment.clear();

  for (DesktopEntry *entry : std::as_const(m_entries)) {
    const QString id = entry->id();
    const QString lowerId = id.toLower();
    m_byId.insert(id, entry);
    if (!m_byLowerId.contains(lowerId)) {
      m_byLowerId.insert(lowerId, entry);
    }
    const QString cls = entry->startupClass().toLower();
    if (!cls.isEmpty() && !m_byClass.contains(cls)) {
      m_byClass.insert(cls, entry);
    }
    const QString name = entry->name().toLower();
    if (!name.isEmpty() && !m_byName.contains(name)) {
      m_byName.insert(name, entry);
    }
    const int dot = lowerId.lastIndexOf(QLatin1Char('.'));
    if (dot >= 0) {
      const QString tail = lowerId.mid(dot + 1);
      if (!tail.isEmpty() && !m_byLastSegment.contains(tail)) {
        m_byLastSegment.insert(tail, entry);
      }
    }
  }
}

DesktopIcons::DesktopIcons(QObject *parent) : QObject(parent) {}

DesktopIcons *DesktopIcons::create(QQmlEngine *engine, QJSEngine *) {
  ensureProvider(engine);
  return new DesktopIcons;
}

QString DesktopIcons::path(const QString &name, const QString &fallback) const {
  ensureProvider(qmlEngine(this));

  QString result = resolveIcon(name);
  if (result.isEmpty() && !fallback.isEmpty()) {
    result = resolveIcon(fallback);
  }
  return result;
}

bool DesktopIcons::has(const QString &name) const {
  return !resolveIcon(name).isEmpty();
}
