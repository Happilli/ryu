#include "./fileview.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSaveFile>
#include <QThreadPool>

namespace {

struct ReadResult {
  QByteArray data;
  FileViewError::Enum error = FileViewError::Success;
};

ReadResult readFile(const QString &path) {
  ReadResult result;
  const QFileInfo info(path);
  if (!info.exists()) {
    result.error = FileViewError::FileNotFound;
    return result;
  }
  if (!info.isFile()) {
    result.error = FileViewError::NotAFile;
    return result;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    result.error = file.error() == QFileDevice::PermissionsError
                       ? FileViewError::PermissionDenied
                       : FileViewError::Unknown;
    return result;
  }
  result.data = file.readAll();
  if (file.error() != QFileDevice::NoError) {
    result.data.clear();
    result.error = FileViewError::Unknown;
  }
  return result;
}

FileViewError::Enum classifyWrite(QFileDevice::FileError error,
                                  const QString &path) {
  if (error == QFileDevice::PermissionsError) {
    return FileViewError::PermissionDenied;
  }
  if (!QFileInfo(path).absoluteDir().exists()) {
    return FileViewError::FileNotFound;
  }
  return FileViewError::Unknown;
}

FileViewError::Enum writeFile(const QString &path, const QByteArray &data,
                              bool atomic) {
  if (atomic) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
      return classifyWrite(file.error(), path);
    }
    if (file.write(data) != data.size()) {
      file.cancelWriting();
      return FileViewError::Unknown;
    }
    return file.commit() ? FileViewError::Success : FileViewError::Unknown;
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return classifyWrite(file.error(), path);
  }
  if (file.write(data) != data.size()) {
    return FileViewError::Unknown;
  }
  file.close();
  return file.error() == QFileDevice::NoError ? FileViewError::Success
                                              : FileViewError::Unknown;
}

} // namespace

FileView::FileView(QObject *parent) : QObject(parent) {}

void FileView::componentComplete() {
  m_complete = true;
  syncWatcher();
  if (m_preload && !m_path.isEmpty()) {
    load(m_blockLoading || m_blockAllReads);
  }
}

void FileView::setPath(const QString &path) {
  if (m_path == path) {
    return;
  }
  m_path = path;
  ++m_generation;
  m_data.clear();
  m_hasData = false;
  emit pathChanged();
  if (!m_complete) {
    return;
  }
  syncWatcher();
  if (m_preload && !m_path.isEmpty()) {
    load(m_blockLoading || m_blockAllReads);
  }
}

void FileView::setPreload(bool on) {
  if (m_preload == on) {
    return;
  }
  m_preload = on;
  emit preloadChanged();
  if (on && m_complete && !m_hasData && !m_path.isEmpty()) {
    load(m_blockLoading || m_blockAllReads);
  }
}

void FileView::setBlockLoading(bool on) {
  if (m_blockLoading == on) {
    return;
  }
  m_blockLoading = on;
  emit blockLoadingChanged();
}

void FileView::setBlockAllReads(bool on) {
  if (m_blockAllReads == on) {
    return;
  }
  m_blockAllReads = on;
  emit blockAllReadsChanged();
}

void FileView::setBlockWrites(bool on) {
  if (m_blockWrites == on) {
    return;
  }
  m_blockWrites = on;
  emit blockWritesChanged();
}

void FileView::setAtomicWrites(bool on) {
  if (m_atomicWrites == on) {
    return;
  }
  m_atomicWrites = on;
  emit atomicWritesChanged();
}

void FileView::setWatchChanges(bool on) {
  if (m_watchChanges == on) {
    return;
  }
  m_watchChanges = on;
  emit watchChangesChanged();
  syncWatcher();
}

void FileView::setPrintErrors(bool on) {
  if (m_printErrors == on) {
    return;
  }
  m_printErrors = on;
  emit printErrorsChanged();
}

QString FileView::text() {
  ensureData();
  return QString::fromUtf8(m_data);
}

QByteArray FileView::data() {
  ensureData();
  return m_data;
}

void FileView::ensureData() {
  if (m_hasData || m_path.isEmpty()) {
    return;
  }
  const ReadResult result = readFile(m_path);
  if (result.error == FileViewError::Success) {
    m_data = result.data;
    m_hasData = true;
  }
}

void FileView::reload() {
  if (m_path.isEmpty()) {
    return;
  }
  load(m_blockAllReads);
}

void FileView::load(bool blocking) {
  const quint64 generation = ++m_generation;
  if (blocking) {
    const ReadResult result = readFile(m_path);
    finishLoad(result.data, result.error, generation);
    return;
  }
  QPointer<FileView> guard(this);
  const QString path = m_path;
  QThreadPool::globalInstance()->start([guard, path, generation]() {
    const ReadResult result = readFile(path);
    if (!guard) {
      return;
    }
    QMetaObject::invokeMethod(
        guard.data(),
        [guard, result, generation]() {
          if (guard) {
            guard->finishLoad(result.data, result.error, generation);
          }
        },
        Qt::QueuedConnection);
  });
}

void FileView::finishLoad(const QByteArray &data, FileViewError::Enum error,
                          quint64 generation) {
  if (generation != m_generation) {
    return;
  }
  if (error != FileViewError::Success) {
    if (m_printErrors) {
      qWarning() << "FileView: failed to read" << m_path << "error"
                 << static_cast<int>(error);
    }
    emit loadFailed(error);
    return;
  }
  m_data = data;
  m_hasData = true;
  emit loaded();
}

void FileView::setText(const QString &text) { write(text.toUtf8()); }

void FileView::setData(const QByteArray &data) { write(data); }

void FileView::write(const QByteArray &data) {
  if (m_path.isEmpty()) {
    return;
  }
  m_data = data;
  m_hasData = true;
  if (m_blockWrites) {
    finishWrite(writeFile(m_path, data, m_atomicWrites));
    return;
  }
  if (m_writing) {
    m_queued = data;
    m_hasQueued = true;
    return;
  }
  startWrite(data);
}

void FileView::startWrite(const QByteArray &data) {
  m_writing = true;
  QPointer<FileView> guard(this);
  const QString path = m_path;
  const bool atomic = m_atomicWrites;
  QThreadPool::globalInstance()->start([guard, path, data, atomic]() {
    const FileViewError::Enum error = writeFile(path, data, atomic);
    if (!guard) {
      return;
    }
    QMetaObject::invokeMethod(
        guard.data(),
        [guard, error]() {
          if (!guard) {
            return;
          }
          guard->m_writing = false;
          guard->finishWrite(error);
          if (guard->m_hasQueued) {
            const QByteArray next =
                std::exchange(guard->m_queued, QByteArray());
            guard->m_hasQueued = false;
            guard->startWrite(next);
          }
        },
        Qt::QueuedConnection);
  });
}

void FileView::finishWrite(FileViewError::Enum error) {
  if (m_watcher) {
    m_signature = signature();
    arm();
  }
  if (error != FileViewError::Success) {
    if (m_printErrors) {
      qWarning() << "FileView: failed to write" << m_path << "error"
                 << static_cast<int>(error);
    }
    emit saveFailed(error);
    return;
  }
  emit saved();
}

FileView::Signature FileView::signature() const {
  const QFileInfo info(m_path);
  Signature sig;
  sig.exists = info.exists();
  if (sig.exists) {
    sig.size = info.size();
    sig.modified = info.lastModified();
  }
  return sig;
}

void FileView::syncWatcher() {
  if (!m_watchChanges || m_path.isEmpty() || !m_complete) {
    if (m_watcher) {
      m_watcher->deleteLater();
      m_watcher = nullptr;
    }
    return;
  }
  if (!m_watcher) {
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
            &FileView::onFileEvent);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &FileView::onDirectoryEvent);
  }
  const QStringList old = m_watcher->files() + m_watcher->directories();
  if (!old.isEmpty()) {
    m_watcher->removePaths(old);
  }
  m_signature = signature();
  arm();
}

void FileView::arm() {
  if (!m_watcher) {
    return;
  }
  const QFileInfo info(m_path);
  const QString dir = info.absolutePath();
  const QString file = info.absoluteFilePath();
  if (!m_watcher->directories().contains(dir)) {
    m_watcher->addPath(dir);
  }
  if (info.exists() && !m_watcher->files().contains(file)) {
    m_watcher->addPath(file);
  }
}

void FileView::onFileEvent() {
  arm();
  m_signature = signature();
  emit fileChanged();
}

void FileView::onDirectoryEvent() {
  arm();
  const Signature current = signature();
  if (current == m_signature) {
    return;
  }
  m_signature = current;
  emit fileChanged();
}
