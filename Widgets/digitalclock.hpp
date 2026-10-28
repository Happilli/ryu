#pragma once
#include "../Core/sgitem.hpp"
#include <QColor>
#include <QFont>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QString>
#include <QTimer>

class DigitalClock : public SgItem {
  Q_OBJECT
  QML_ELEMENT

  Q_PROPERTY(QString hours READ hours NOTIFY timeChanged)
  Q_PROPERTY(QString minutes READ minutes NOTIFY timeChanged)
  Q_PROPERTY(QString period READ period NOTIFY timeChanged)
  Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation
                 NOTIFY orientationChanged)
  Q_PROPERTY(
      bool use24Hour READ use24Hour WRITE setUse24Hour NOTIFY use24HourChanged)
  Q_PROPERTY(
      bool showAmPm READ showAmPm WRITE setShowAmPm NOTIFY showAmPmChanged)
  Q_PROPERTY(AmPmStyle amPmStyle READ amPmStyle WRITE setAmPmStyle NOTIFY
                 amPmStyleChanged)
  Q_PROPERTY(bool showSeparator READ showSeparator WRITE setShowSeparator NOTIFY
                 showSeparatorChanged)
  Q_PROPERTY(
      QString testTime READ testTime WRITE setTestTime NOTIFY testTimeChanged)
  Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)

  Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
  Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
  Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY
                 fontWeightChanged)
  Q_PROPERTY(qreal gap READ gap WRITE setGap NOTIFY gapChanged)
  Q_PROPERTY(qreal overlap READ overlap WRITE setOverlap NOTIFY overlapChanged)
  Q_PROPERTY(
      qreal groupGap READ groupGap WRITE setGroupGap NOTIFY groupGapChanged)
  Q_PROPERTY(qreal letterSpacing READ letterSpacing WRITE setLetterSpacing
                 NOTIFY letterSpacingChanged)
  Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY paddingChanged)

public:
  enum Orientation { Vertical, Horizontal };
  Q_ENUM(Orientation)
  enum AmPmStyle { Beside, Below, Superscript };
  Q_ENUM(AmPmStyle)

  explicit DigitalClock(QQuickItem *parent = nullptr);

  QString hours() const { return m_hours; }
  QString minutes() const { return m_minutes; }
  QString period() const { return m_period; }
  Orientation orientation() const { return m_orientation; }
  bool use24Hour() const { return m_24h; }
  bool showAmPm() const { return m_showAmPm; }
  AmPmStyle amPmStyle() const { return m_amPmStyle; }
  bool showSeparator() const { return m_showSep; }
  QString testTime() const { return m_testTime; }
  bool running() const { return m_running; }
  QColor color() const { return m_color; }
  QFont font() const { return m_font; }
  int fontWeight() const { return m_fontWeight; }
  qreal gap() const { return m_gap; }
  qreal overlap() const { return m_overlap; }
  qreal groupGap() const { return m_groupGap; }
  qreal letterSpacing() const { return m_spacing; }
  qreal padding() const { return m_padding; }

  void setOrientation(Orientation v);
  void setUse24Hour(bool v);
  void setShowAmPm(bool v);
  void setAmPmStyle(AmPmStyle v);
  void setShowSeparator(bool v);
  void setTestTime(const QString &v);
  void setRunning(bool v);
  void setColor(const QColor &v);
  void setFont(const QFont &v);
  void setFontWeight(int v);
  void setGap(qreal v);
  void setOverlap(qreal v);
  void setGroupGap(qreal v);
  void setLetterSpacing(qreal v);
  void setPadding(qreal v);

  Q_INVOKABLE void refresh();

signals:
  void timeChanged();
  void orientationChanged();
  void use24HourChanged();
  void showAmPmChanged();
  void amPmStyleChanged();
  void showSeparatorChanged();
  void testTimeChanged();
  void runningChanged();
  void colorChanged();
  void fontChanged();
  void fontWeightChanged();
  void gapChanged();
  void overlapChanged();
  void groupGapChanged();
  void letterSpacingChanged();
  void paddingChanged();

protected:
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *d) override;
  void componentComplete() override;
  void itemChange(ItemChange change, const ItemChangeData &data) override;
  void geometryChange(const QRectF &n, const QRectF &o) override;
  void propertyAssigned() override;

private:
  struct Piece {
    QPainterPath path;
    qreal cut = 0.0;
  };

  bool wantsPeriod() const { return !m_24h && m_showAmPm; }
  bool timerWanted() const;
  bool stale() const;
  void syncTimer();
  void rebuild();
  void rebuildVertical(const QFont &f);
  void rebuildHorizontal(const QFont &f);
  QImage render(qreal dpr);

  QString m_hours;
  QString m_minutes;
  QString m_period;

  Orientation m_orientation = Vertical;
  bool m_24h = true;
  bool m_showAmPm = false;
  AmPmStyle m_amPmStyle = Beside;
  bool m_showSep = true;
  QString m_testTime;
  bool m_running = true;

  QColor m_color = QColor("#99CCFF");
  QFont m_font;
  int m_fontWeight = 900;
  qreal m_gap = 0.045;
  qreal m_overlap = 0.03;
  qreal m_groupGap = 0.14;
  qreal m_spacing = -0.04;
  qreal m_padding = 0.0;

  QList<Piece> m_pieces;
  bool m_dirty = true;

  qreal m_bw = 0.0;
  qreal m_bh = 0.0;
  qreal m_aspect = 0.0;
  qreal m_builtW = 0.0;

  bool m_imgStale = true;
  qreal m_imgDpr = 0.0;

  QTimer m_timer;
};
