#include "./clipsh.hpp"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

Clipsh::Clipsh(QObject *parent) : QObject(parent) {}

Clipsh::~Clipsh() = default;

void Clipsh::run(const QString &program, const QStringList &args,
                 const QByteArray &input, Done done) {
  auto *p = new QProcess(this);
  auto *timer = new QTimer(p);
  timer->setSingleShot(true);
  timer->setInterval(kProcessTimeoutMs);

  connect(timer, &QTimer::timeout, p, [p] {
    p->setProperty("timedOut", true);
    p->kill();
  });

  connect(p, &QProcess::started, p, [p, timer, input] {
    timer->start();
    if (!input.isEmpty())
      p->write(input);
    p->closeWriteChannel();
  });

  connect(p, &QProcess::errorOccurred, this,
          [p, program, done](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart)
              return;
            done(false, {}, program + ": failed to start");
            p->deleteLater();
          });

  connect(p, &QProcess::finished, this,
          [p, done](int code, QProcess::ExitStatus status) {
            const bool timedOut = p->property("timedOut").toBool();
            const bool ok =
                !timedOut && status == QProcess::NormalExit && code == 0;
            QString err =
                timedOut
                    ? QStringLiteral("timed out")
                    : QString::fromUtf8(p->readAllStandardError()).trimmed();
            done(ok, p->readAllStandardOutput(), err);
            p->deleteLater();
          });

  p->start(program, args);
}

QString Clipsh::entryId(const QString &entry) {
  const int tab = entry.indexOf('\t');
  return tab >= 0 ? entry.left(tab) : entry;
}

QString Clipsh::cacheDir() const {
  if (!m_cacheDir.isEmpty())
    return m_cacheDir;

  QString base =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (base.isEmpty())
    base = QDir::tempPath();

  const QString dir = base + "/clipsh";
  QDir().mkpath(dir);
  QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                 QFileDevice::ExeOwner);
  m_cacheDir = dir;
  return m_cacheDir;
}

void Clipsh::trimHistory(QStringList &all) {
  if (all.size() <= kMaxHistory)
    return;
  const QStringList excess = all.mid(kMaxHistory);
  all = all.mid(0, kMaxHistory);
  run("cliphist", {"delete"}, excess.join('\n').toUtf8(),
      [this](bool ok, const QByteArray &, const QString &err) {
        if (!ok)
          emit error("failed to trim clipboard history: " + err);
      });
}

void Clipsh::removeLocal(const QString &entry) {
  if (m_history.removeAll(entry) == 0)
    return;

  const QString id = entryId(entry);
  m_imageCache.remove(id);
  for (const QString &f : QDir(cacheDir()).entryList({id + ".*"}, QDir::Files))
    QFile::remove(cacheDir() + "/" + f);

  emit historyChanged();
}

void Clipsh::pruneImageCache() {
  QSet<QString> live;
  for (const QString &e : std::as_const(m_history))
    live.insert(entryId(e));

  for (auto it = m_imageCache.begin(); it != m_imageCache.end();) {
    it = live.contains(it.key()) ? std::next(it) : m_imageCache.erase(it);
  }

  const QDir dir(cacheDir());
  for (const QFileInfo &fi : dir.entryInfoList(QDir::Files)) {
    if (!live.contains(fi.completeBaseName()))
      QFile::remove(fi.absoluteFilePath());
  }
}

void Clipsh::clearImageCache() {
  m_imageCache.clear();
  m_decodeQueue.clear();
  const QDir dir(cacheDir());
  for (const QString &f : dir.entryList(QDir::Files))
    QFile::remove(dir.filePath(f));
}

void Clipsh::fetchHistory() {
  if (m_loading)
    return;
  m_loading = true;
  emit loadingChanged();

  run("cliphist", {"list"}, {},
      [this](bool ok, const QByteArray &out, const QString &err) {
        QStringList all =
            ok ? QString::fromUtf8(out).split('\n', Qt::SkipEmptyParts)
               : QStringList{};
        trimHistory(all);
        m_history = all;
        pruneImageCache();

        m_loading = false;
        emit loadingChanged();
        emit historyChanged();

        if (!ok)
          emit error("failed to fetch clipboard history: " + err);
      });
}

void Clipsh::copyItem(const QString &entry) {
  run("cliphist", {"decode"}, entry.toUtf8(),
      [this, entry](bool ok, const QByteArray &data, const QString &err) {
        if (!ok || data.isEmpty()) {
          emit error("failed to decode clipboard entry: " + err);
          removeLocal(entry);
          emit actionFinished();
          return;
        }

        QByteArray payload = data;
        QStringList args{"wl-copy"};

        if (isImage(entry)) {
          const QImage img = QImage::fromData(data);
          if (!img.isNull()) {
            QByteArray png;
            QBuffer buf(&png);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
            payload = png;
            args << "--type" << "image/png";
          } else {
            args << "--type" << imageMimeType(entry);
          }
        }

        run(args.first(), args.mid(1), payload,
            [this](bool ok, const QByteArray &, const QString &err) {
              if (!ok)
                emit error("wl-copy failed: " + err);
            });

        run("cliphist", {"delete"}, entry.toUtf8(),
            [](bool, const QByteArray &, const QString &) {});

        removeLocal(entry);
        emit itemCopied(entry);
        emit actionFinished();
      });
}
void Clipsh::deleteItem(const QString &entry) {
  run("cliphist", {"delete"}, entry.toUtf8(),
      [this, entry](bool ok, const QByteArray &, const QString &err) {
        removeLocal(entry);
        if (!ok) {
          emit error("failed to delete clipboard entry: " + err);
        } else {
          emit itemDeleted(entry);
        }
        emit actionFinished();
      });
}

void Clipsh::wipe() {
  run("cliphist", {"wipe"}, {},
      [this](bool ok, const QByteArray &, const QString &err) {
        m_history.clear();
        clearImageCache();
        emit historyChanged();
        emit wiped();
        if (!ok)
          emit error("failed to wipe clipboard history: " + err);
      });
}

bool Clipsh::isImage(const QString &entry) const {
  return entry.contains("[[ binary data") &&
         (entry.contains("jpeg") || entry.contains("jpg") ||
          entry.contains("png") || entry.contains("gif") ||
          entry.contains("webp"));
}

QString Clipsh::imageMimeType(const QString &entry) const {
  if (entry.contains("jpeg") || entry.contains("jpg"))
    return "image/jpeg";
  if (entry.contains("png"))
    return "image/png";
  if (entry.contains("gif"))
    return "image/gif";
  if (entry.contains("webp"))
    return "image/webp";
  return "image/jpeg";
}

QString Clipsh::previewText(const QString &entry, int maxLen) const {
  const int tab = entry.indexOf('\t');
  QString text = (tab >= 0) ? entry.mid(tab + 1) : entry;
  text = text.trimmed();
  if (text.length() > maxLen)
    return text.left(maxLen) + "...";
  return text;
}

QString Clipsh::tempImagePath(const QString &entry) {
  const QString id = entryId(entry);

  if (const auto it = m_imageCache.constFind(id); it != m_imageCache.constEnd())
    return it.value();

  if (!m_decoding.contains(id)) {
    m_decoding.insert(id);
    m_decodeQueue.enqueue(entry);
    pumpDecodeQueue();
  }
  return {};
}

void Clipsh::pumpDecodeQueue() {
  while (m_activeDecodes < kMaxParallelDecodes && !m_decodeQueue.isEmpty()) {
    const QString entry = m_decodeQueue.dequeue();
    const QString id = entryId(entry);
    ++m_activeDecodes;

    run("cliphist", {"decode"}, entry.toUtf8(),
        [this, entry, id](bool ok, const QByteArray &data, const QString &err) {
          --m_activeDecodes;
          m_decoding.remove(id);

          if (!m_history.contains(entry)) {
            pumpDecodeQueue();
            return;
          }

          if (!ok || data.isEmpty()) {
            emit error("failed to decode image entry for preview: " + err);
            removeLocal(entry);
            pumpDecodeQueue();
            return;
          }

          const QString ext = imageMimeType(entry).section('/', 1);
          const QString path = cacheDir() + "/" + id + "." + ext;

          QFile f(path);
          if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit error("failed to write image preview cache");
            pumpDecodeQueue();
            return;
          }
          f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
          f.write(data);
          f.close();

          const QString url = "file://" + path;
          m_imageCache.insert(id, url);
          emit imageReady(entry, url);
          pumpDecodeQueue();
        });
  }
}
