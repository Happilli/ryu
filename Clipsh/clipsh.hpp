#pragma once
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

class Clipsh : public QObject {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(int count READ count NOTIFY historyChanged)

public:
  explicit Clipsh(QObject *parent = nullptr);
  ~Clipsh() override;

  QStringList history() const { return m_history; }
  bool loading() const { return m_loading; }
  int count() const { return m_history.size(); }

  Q_INVOKABLE void fetchHistory();
  Q_INVOKABLE void copyItem(const QString &entry);
  Q_INVOKABLE void deleteItem(const QString &entry);
  Q_INVOKABLE void wipe();
  Q_INVOKABLE bool isImage(const QString &entry) const;
  Q_INVOKABLE QString imageMimeType(const QString &entry) const;
  Q_INVOKABLE QString previewText(const QString &entry, int maxLen = 80) const;
  Q_INVOKABLE QString tempImagePath(const QString &entry);

signals:
  void historyChanged();
  void loadingChanged();
  void itemCopied(const QString &entry);
  void itemDeleted(const QString &entry);
  void wiped();
  void error(const QString &message);
  void actionFinished();
  void imageReady(const QString &entry, const QString &url);

private:
  using Done =
      std::function<void(bool ok, const QByteArray &out, const QString &err)>;

  static constexpr int kMaxHistory = 25;
  static constexpr int kProcessTimeoutMs = 5000;
  static constexpr int kMaxParallelDecodes = 3;

  void run(const QString &program, const QStringList &args,
           const QByteArray &input, Done done);
  void trimHistory(QStringList &all);
  void removeLocal(const QString &entry);
  void pruneImageCache();
  void clearImageCache();
  void pumpDecodeQueue();
  QString cacheDir() const;
  static QString entryId(const QString &entry);

  QStringList m_history;
  bool m_loading = false;

  QHash<QString, QString> m_imageCache;
  QSet<QString> m_decoding;
  QQueue<QString> m_decodeQueue;
  int m_activeDecodes = 0;
  mutable QString m_cacheDir;
};
