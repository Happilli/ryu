#pragma once

#include <QHash>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QScreen;

namespace ryu::wl {
class GammaOutput;
}

class NightLight : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
  Q_PROPERTY(qreal temperature READ temperature WRITE setTemperature NOTIFY
                 temperatureChanged)
  Q_PROPERTY(qreal gamma READ gamma WRITE setGamma NOTIFY gammaChanged)

public:
  explicit NightLight(QObject *parent = nullptr);
  ~NightLight() override;

  bool active() const { return m_active; }
  qreal temperature() const { return m_temperature; }
  qreal gamma() const { return m_gamma; }

  void setActive(bool on);
  void setTemperature(qreal kelvin);
  void setGamma(qreal value);

signals:
  void activeChanged();
  void temperatureChanged();
  void gammaChanged();

private:
  void engage();
  void release();
  void attach(QScreen *screen);
  void detach(QScreen *screen);
  void schedule();
  void flush();
  void push(ryu::wl::GammaOutput *output);

  QHash<QScreen *, ryu::wl::GammaOutput *> m_outputs;
  bool m_active = false;
  bool m_scheduled = false;
  bool m_warned = false;
  qreal m_temperature = 6500;
  qreal m_gamma = 1.0;
};
