#ifndef KEYBOARD_CHART_H
#define KEYBOARD_CHART_H

#pragma once
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QLinearGradient>

class KeyMonitor;

// ============================================================
// KeyActivityChart — 键盘活跃度柱状图（自定义绘制）
// ============================================================
class KeyActivityChart : public QWidget {
    Q_OBJECT
    QVector<int> m_data;   // 每分钟敲击数（最新在前）
    int m_peak = 0;
public:
    explicit KeyActivityChart(QWidget *parent = nullptr);
    void setData(const QVector<int> &data);
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// ============================================================
// KeyMonitorDialog — 键盘活跃度分析对话框
// ============================================================
class KeyMonitorDialog : public QDialog {
    Q_OBJECT
public:
    explicit KeyMonitorDialog(KeyMonitor *monitor, QWidget *parent = nullptr);
};

#endif // KEYBOARD_CHART_H
