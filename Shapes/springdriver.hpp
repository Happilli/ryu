#pragma once
#include <QAbstractAnimation>
#include <functional>

class SpringDriver : public QAbstractAnimation {
public:
  std::function<void(qreal)> tick;

  explicit SpringDriver(QObject *parent) : QAbstractAnimation(parent) {}

  int duration() const override { return -1; }

protected:
  void updateCurrentTime(int t) override {
    const qreal dt = qBound(0.0, (t - m_last) / 1000.0, 1.0 / 30.0);
    m_last = t;
    if (tick)
      tick(dt);
  }

  void updateState(State newState, State oldState) override {
    if (newState == Running && oldState == Stopped)
      m_last = 0;
  }

private:
  int m_last = 0;
};
