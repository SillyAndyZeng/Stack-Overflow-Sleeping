#ifndef SLEEP_CHARTS_H
#define SLEEP_CHARTS_H

#pragma once
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QVector>
#include <QDate>
#include <QMap>

// 单天的睡眠数据（供图表使用）
struct SleepDayData {
    QDate date;
    int nightSleepMin = 0;   // 夜间睡眠（分钟）
    int daySleepMin   = 0;   // 午睡（分钟）
    int score         = 0;   // 睡眠评分 (-5 ~ 3)
    bool stayUp       = false;
    bool noNightSleep = false;
    bool oversleep    = false;

    double totalHours()  const { return (nightSleepMin + daySleepMin) / 60.0; }
    double nightHours()  const { return nightSleepMin / 60.0; }
    double dayHours()    const { return daySleepMin / 60.0; }
};

// ============================================================
// SleepBarChart — 定制绘制的柱状图（每日睡眠时长）
// ============================================================
class SleepBarChart : public QWidget {
    Q_OBJECT
    QVector<SleepDayData> m_data;
    int m_minHeightHint = 280;
public:
    explicit SleepBarChart(QWidget *parent = nullptr);

    void setData(const QVector<SleepDayData> &data);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawBackground  (QPainter &p, const QRect &area);
    void drawGrid        (QPainter &p, const QRect &area, double maxVal);
    void drawBars        (QPainter &p, const QRect &area, double maxVal);
    void drawLabels      (QPainter &p, const QRect &area);
    void drawRefLines    (QPainter &p, const QRect &area);
};

// ============================================================
// SleepScoreChart — 定制绘制的折线图（睡眠评分趋势）
// ============================================================
class SleepScoreChart : public QWidget {
    Q_OBJECT
    QVector<SleepDayData> m_data;
    int m_minHeightHint = 200;
public:
    explicit SleepScoreChart(QWidget *parent = nullptr);

    void setData(const QVector<SleepDayData> &data);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawBackground (QPainter &p, const QRect &area);
    void drawGrid       (QPainter &p, const QRect &area);
    void drawLineChart  (QPainter &p, const QRect &area);
    void drawLabels     (QPainter &p, const QRect &area);
};

// ============================================================
// WeekChartDialog — 包含以上两个图表的主对话框
// ============================================================
class WeekChartDialog : public QDialog {
    Q_OBJECT
public:
    explicit WeekChartDialog(const QVector<SleepDayData> &data, QWidget *parent = nullptr);
};

#endif // SLEEP_CHARTS_H
