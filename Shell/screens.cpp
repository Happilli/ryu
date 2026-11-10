#include "./screens.hpp"
#include <QGuiApplication>
#include <QScreen>

Screens::Screens(QObject *parent) : QObject(parent) {
  rebuild();
  connect(qGuiApp, &QGuiApplication::screenAdded, this,
          [this]() { rebuild(); });
  connect(qGuiApp, &QGuiApplication::screenRemoved, this,
          [this]() { rebuild(); });
  connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this,
          [this]() { rebuild(); });
}

void Screens::rebuild() {
  QVariantList next;
  const auto screens = QGuiApplication::screens();
  for (QScreen *screen : screens) {
    next.append(QVariant::fromValue(screen));
  }
  m_list = next;
  emit listChanged();
}
