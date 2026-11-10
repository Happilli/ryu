#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class Backlight : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(qreal brightness READ brightness NOTIFY brightnessChanged)
  Q_PROPERTY(bool available READ available NOTIFY availableChanged)
  Q_PROPERTY(QString device READ device NOTIFY availableChanged)

public:
  explicit Backlight(QObject *parent = nullptr);

  qreal brightness() const { return m_brightness; }
  bool available() const { return m_max > 0; }
  QString device() const { return m_device; }

  Q_INVOKABLE void setBrightness(qreal value);
  Q_INVOKABLE void refresh();

signals:
  void brightnessChanged();
  void availableChanged();

private:
  void discover();
  int readValue(const QString &file) const;
  void send();

  QString m_device;
  int m_max = 0;
  int m_pending = -1;
  bool m_inflight = false;
  qreal m_brightness = 1.0;
};
