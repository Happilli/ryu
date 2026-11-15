#include "./screens.hpp"
#include <QGuiApplication>
#include <QScreen>

Screens::Screens(QObject *parent) : QObject(parent) {
  rebuild();
  connect(qGuiApp, &QGuiApplication::screenAdded, this, &Screens::rebuild);
  connect(qGuiApp, &QGuiApplication::screenRemoved, this, &Screens::rebuild);
  connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this,
          &Screens::rebuild);
}

void Screens::rebuild() {
  QVariantList next;
  for (QScreen *screen : QGuiApplication::screens()) {
    next.append(QVariant::fromValue(screen));
  }
  if (next == m_list) {
    return;
  }
  m_list = next;
  emit listChanged();
}
