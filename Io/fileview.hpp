#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQmlParserStatus>
#include <QString>

class FileViewError : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("FileViewError only provides enum values")

public:
  enum Enum { Success = 0, Unknown, FileNotFound, PermissionDenied, NotAFile };
  Q_ENUM(Enum)
};

class FileView : public QObject, public QQmlParserStatus {
  Q_OBJECT
  Q_INTERFACES(QQmlParserStatus)
  QML_ELEMENT
  Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
  Q_PROPERTY(bool blockLoading READ blockLoading WRITE setBlockLoading NOTIFY
                 blockLoadingChanged)
  Q_PROPERTY(bool atomicWrites READ atomicWrites WRITE setAtomicWrites NOTIFY
                 atomicWritesChanged)
  Q_PROPERTY(bool watchChanges READ watchChanges WRITE setWatchChanges NOTIFY
                 watchChangesChanged)

public:
  explicit FileView(QObject *parent = nullptr);

  void classBegin() override {}
  void componentComplete() override;

  QString path() const { return m_path; }
  void setPath(const QString &path);

  bool blockLoading() const { return m_blockLoading; }
  void setBlockLoading(bool on);

  bool atomicWrites() const { return m_atomicWrites; }
  void setAtomicWrites(bool on);

  bool watchChanges() const { return m_watchChanges; }
  void setWatchChanges(bool on);

  Q_INVOKABLE QString text();
  Q_INVOKABLE void reload();
  Q_INVOKABLE void setText(const QString &text);

signals:
  void pathChanged();
  void blockLoadingChanged();
  void atomicWritesChanged();
  void watchChangesChanged();
  void loaded();
  void loadFailed(FileViewError::Enum error);
  void fileChanged();

private:
  struct Signature {
    bool exists = false;
    qint64 size = 0;
    QDateTime modified;
    bool operator==(const Signature &other) const = default;
  };

  void load(bool blocking);
  void finishLoad(const QByteArray &data, FileViewError::Enum error,
                  quint64 generation);
  void ensureData();
  void write(const QByteArray &data);
  void startWrite(const QByteArray &data);
  void finishWrite(FileViewError::Enum error);
  void syncWatcher();
  void arm();
  Signature signature() const;
  void onFileEvent();
  void onDirectoryEvent();

  QString m_path;
  QByteArray m_data;
  QByteArray m_queued;
  Signature m_signature;
  QPointer<QFileSystemWatcher> m_watcher;
  quint64 m_generation = 0;
  bool m_blockLoading = false;
  bool m_atomicWrites = false;
  bool m_watchChanges = false;
  bool m_hasData = false;
  bool m_hasQueued = false;
  bool m_writing = false;
  bool m_complete = false;
};
