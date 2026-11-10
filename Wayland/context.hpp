#pragma once

#include <QObject>

#include "./wlbuffer.hpp"

namespace ryu::wl {

class ScreencopyContext : public QObject {
  Q_OBJECT

public:
  Swapchain &swapchain() { return m_swapchain; }
  virtual void captureFrame() = 0;

  bool allowDma = true;

signals:
  void frameCaptured();
  void stopped();

protected:
  ScreencopyContext() = default;

  Swapchain m_swapchain;
};

ScreencopyContext *makeContext(QObject *source, bool hardware);

} // namespace ryu::wl
