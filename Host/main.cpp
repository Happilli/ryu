#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QQmlApplicationEngine>
#include <QSocketNotifier>
#include <QStringList>
#include <QUrl>
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr QLatin1String kPrefix("px");

int g_signalPipe[2] = {-1, -1};

void handleSignal(int) {
  const char byte = 1;
  const ssize_t written = write(g_signalPipe[1], &byte, 1);
  (void)written;
}

bool installSignals() {
  if (pipe2(g_signalPipe, O_CLOEXEC | O_NONBLOCK) != 0) {
    return false;
  }
  struct sigaction action{};
  action.sa_handler = handleSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;
  sigaction(SIGTERM, &action, nullptr);
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGHUP, &action, nullptr);
  return true;
}

bool isSingleton(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  while (!file.atEnd()) {
    const QByteArray line = file.readLine().trimmed();
    if (line.startsWith("pragma Singleton")) {
      return true;
    }
  }
  return false;
}

bool mirrorTree(const QString &source, const QString &target,
                const QString &module) {
  if (!QDir().mkpath(target)) {
    return false;
  }
  const QFileInfoList entries = QDir(source).entryInfoList(
      QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QByteArray registry;
  for (const QFileInfo &info : entries) {
    const QString name = info.fileName();
    if (name.startsWith(QLatin1Char('.'))) {
      continue;
    }
    const QString dest = target + QLatin1Char('/') + name;
    if (info.isDir()) {
      if (!mirrorTree(info.absoluteFilePath(), dest,
                      module + QLatin1Char('.') + name)) {
        return false;
      }
      continue;
    }
    if (name == QLatin1String("qmldir")) {
      continue;
    }
    if (!QFile::link(info.absoluteFilePath(), dest)) {
      return false;
    }
    if (name.endsWith(QLatin1String(".qml")) && name.at(0).isUpper()) {
      const QString type = info.completeBaseName();
      if (isSingleton(info.absoluteFilePath())) {
        registry += "singleton ";
      }
      registry += type.toUtf8() + ' ' + name.toUtf8() + '\n';
    }
  }
  if (registry.isEmpty()) {
    return true;
  }
  QFile qmldir(target + QStringLiteral("/qmldir"));
  if (!qmldir.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  qmldir.write("module " + module.toUtf8() + '\n');
  qmldir.write(registry);
  return true;
}

bool writeQmllsIni(const QString &shellDir, const QString &imports) {
  QStringList paths;
  paths << imports << QCoreApplication::applicationDirPath()
        << QLibraryInfo::path(QLibraryInfo::QmlImportsPath);
  paths.removeDuplicates();

  QByteArray content;
  content += "[General]\n";
  content += "no-cmake-calls=true\n";
  content += "buildDir=\"" + imports.toUtf8() + "\"\n";
  content += "importPaths=\"" + paths.join(QLatin1Char(':')).toUtf8() + "\"\n";

  const QString path = shellDir + QStringLiteral("/.qmlls.ini");
  QFile existing(path);
  if (existing.open(QIODevice::ReadOnly)) {
    const QByteArray current = existing.readAll();
    existing.close();
    if (current == content) {
      return true;
    }
  }

  QFile out(path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return out.write(content) == content.size();
}

} // namespace

int main(int argc, char **argv) {
  if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
    qputenv("QSG_RENDER_LOOP", "basic");
  }
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "wayland");
  }

  QGuiApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("phoenix"));
  app.setQuitOnLastWindowClosed(false);

  const QStringList args = QCoreApplication::arguments();
  if (args.size() < 2) {
    std::fprintf(stderr, "usage: phoenix <shell directory>\n");
    return 2;
  }

  const QDir shellDir(QDir(args.at(1)).absolutePath());
  const QString shellFile = shellDir.filePath(QStringLiteral("shell.qml"));
  if (!QFile::exists(shellFile)) {
    std::fprintf(stderr, "phoenix: %s not found\n", qPrintable(shellFile));
    return 2;
  }

  const QString runtime =
      qEnvironmentVariable("XDG_RUNTIME_DIR", QStringLiteral("/tmp"));
  const QString imports = runtime + QStringLiteral("/phoenix-imports");
  QDir().mkpath(imports);
  const QString mirror = imports + QLatin1Char('/') + QString(kPrefix);
  QFile::remove(mirror);
  QDir(mirror).removeRecursively();
  if (!mirrorTree(shellDir.absolutePath(), mirror, QString(kPrefix))) {
    std::fprintf(stderr, "phoenix: cannot mirror %s\n", qPrintable(mirror));
    return 1;
  }

  if (!writeQmllsIni(shellDir.absolutePath(), imports)) {
    std::fprintf(stderr, "phoenix: could not write .qmlls.ini\n");
  }

  if (!installSignals()) {
    std::fprintf(stderr, "phoenix: signal setup failed\n");
    return 1;
  }

  QSocketNotifier signalNotifier(g_signalPipe[0], QSocketNotifier::Read);
  QObject::connect(&signalNotifier, &QSocketNotifier::activated, &app,
                   [&signalNotifier]() {
                     signalNotifier.setEnabled(false);
                     QCoreApplication::quit();
                   });

  QQmlApplicationEngine engine;
  engine.addImportPath(imports);
  engine.addImportPath(QCoreApplication::applicationDirPath());

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

  engine.load(QUrl::fromLocalFile(shellFile));
  if (engine.rootObjects().isEmpty()) {
    return 1;
  }

  return app.exec();
}
