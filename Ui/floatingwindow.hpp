#pragma once

#include <QList>
#include <QQmlListProperty>
#include <QQmlParserStatus>
#include <QQuickWindow>
#include <QSize>
#include <QtQml/qqmlregistration.h>

class FloatingWindow : public QQuickWindow, public QQmlParserStatus {
  Q_OBJECT
  QML_ELEMENT
  Q_INTERFACES(QQmlParserStatus)
  Q_CLASSINFO("DefaultProperty", "contents")
  Q_PROPERTY(QQmlListProperty<QObject> contents READ contents)
  Q_PROPERTY(qreal implicitWidth READ implicitWidth WRITE setImplicitWidth
                 NOTIFY implicitWidthChanged)
  Q_PROPERTY(qreal implicitHeight READ implicitHeight WRITE setImplicitHeight
                 NOTIFY implicitHeightChanged)
  Q_PROPERTY(QSize minimumSize READ minimumSize WRITE setMinimumSize NOTIFY
                 minimumSizeChanged)
  Q_PROPERTY(QSize maximumSize READ maximumSize WRITE setMaximumSize NOTIFY
                 maximumSizeChanged)

public:
  explicit FloatingWindow(QWindow *parent = nullptr);

  QQmlListProperty<QObject> contents();

  qreal implicitWidth() const { return m_implicitWidth; }
  qreal implicitHeight() const { return m_implicitHeight; }
  void setImplicitWidth(qreal value);
  void setImplicitHeight(qreal value);

  void classBegin() override {}
  void componentComplete() override;

signals:
  void implicitWidthChanged();
  void implicitHeightChanged();
  void minimumSizeChanged();
  void maximumSizeChanged();

private:
  static void appendContent(QQmlListProperty<QObject> *list, QObject *object);
  static qsizetype countContent(QQmlListProperty<QObject> *list);
  static QObject *atContent(QQmlListProperty<QObject> *list, qsizetype index);
  static void clearContent(QQmlListProperty<QObject> *list);

  void applySize();

  QList<QObject *> m_contents;
  qreal m_implicitWidth = 0;
  qreal m_implicitHeight = 0;
};
