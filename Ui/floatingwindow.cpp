#include "./floatingwindow.hpp"

#include <QQuickItem>
#include <cmath>

FloatingWindow::FloatingWindow(QWindow *parent) : QQuickWindow(parent) {
  connect(this, &QWindow::minimumWidthChanged, this,
          &FloatingWindow::minimumSizeChanged);
  connect(this, &QWindow::minimumHeightChanged, this,
          &FloatingWindow::minimumSizeChanged);
  connect(this, &QWindow::maximumWidthChanged, this,
          &FloatingWindow::maximumSizeChanged);
  connect(this, &QWindow::maximumHeightChanged, this,
          &FloatingWindow::maximumSizeChanged);
}

QQmlListProperty<QObject> FloatingWindow::contents() {
  return QQmlListProperty<QObject>(
      this, nullptr, &FloatingWindow::appendContent,
      &FloatingWindow::countContent, &FloatingWindow::atContent,
      &FloatingWindow::clearContent);
}

void FloatingWindow::appendContent(QQmlListProperty<QObject> *list,
                                   QObject *object) {
  auto *self = static_cast<FloatingWindow *>(list->object);
  self->m_contents.append(object);
  if (auto *item = qobject_cast<QQuickItem *>(object)) {
    item->setParentItem(self->contentItem());
  } else {
    object->setParent(self);
  }
}

qsizetype FloatingWindow::countContent(QQmlListProperty<QObject> *list) {
  return static_cast<FloatingWindow *>(list->object)->m_contents.size();
}

QObject *FloatingWindow::atContent(QQmlListProperty<QObject> *list,
                                   qsizetype index) {
  return static_cast<FloatingWindow *>(list->object)->m_contents.at(index);
}

void FloatingWindow::clearContent(QQmlListProperty<QObject> *list) {
  static_cast<FloatingWindow *>(list->object)->m_contents.clear();
}

void FloatingWindow::setImplicitWidth(qreal value) {
  if (qFuzzyCompare(m_implicitWidth + 1.0, value + 1.0)) {
    return;
  }
  m_implicitWidth = value;
  emit implicitWidthChanged();
  applySize();
}

void FloatingWindow::setImplicitHeight(qreal value) {
  if (qFuzzyCompare(m_implicitHeight + 1.0, value + 1.0)) {
    return;
  }
  m_implicitHeight = value;
  emit implicitHeightChanged();
  applySize();
}

void FloatingWindow::applySize() {
  if (m_implicitWidth > 0 && m_implicitHeight > 0) {
    resize(int(std::lround(m_implicitWidth)),
           int(std::lround(m_implicitHeight)));
  }
}

void FloatingWindow::setVisibleFlag(bool visible) {
  m_visibleSet = true;
  setVisible(visible);
}

void FloatingWindow::componentComplete() {
  applySize();
  if (!m_visibleSet) {
    setVisible(true);
  }
}
