#ifndef KEY_MONITOR_H
#define KEY_MONITOR_H

#pragma once
#include <QObject>
#include <QTimer>
#include <QTime>
#include <QVector>
#include <QMutex>

// 全局键盘敲击频率实时监测（macOS）
// 使用 CGEventSourceCounterForEventType 获取系统级按键计数，
// 无需输入监控权限即可跨应用记录
class KeyMonitor : public QObject {
    Q_OBJECT
public:
    explicit KeyMonitor(QObject *parent = nullptr);
    ~KeyMonitor() override;

    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_monitoring; }

    // 获取最近 N 分钟每分钟敲击次数（索引 0 = 当前分钟，最新）
    QVector<int> recentPerMinute(int minutes = 60) const;

    int  totalKeystrokes() const { return m_totalKeys; }
    int  peakMinute()      const;
    int  activeMinutes()   const; // 有敲击的分钟数
    int  keystrokesLastMinute() const;
    bool hasData()         const { return m_totalKeys > 0; }

    // 经过的分钟数（从启动到现在）
    int  elapsedMinutes()  const;

signals:
    void dataUpdated();          // 每分钟更新一次
    void keystrokeDetected(int total); // 检测到按键

private slots:
    void advanceMinuteSlot();    // 每分钟推进一次槽位
    void onPollTimer();         // 高频轮询（检测系统按键计数变化）

private:
    bool m_monitoring = false;

    // 环形缓冲区：最多保存 480 分钟（8 小时）
    static const int MAX_SLOTS = 480;
    int m_slots[MAX_SLOTS] = {0};
    int m_currentSlot = 0;
    int m_slotCount   = 0;   // 实际已使用的分钟数
    int m_totalKeys   = 0;

    QTimer *m_slotTimer = nullptr;
    QTimer *m_pollTimer = nullptr;  // 高频轮询定时器（每 200ms）
    QTime   m_startTime;

    // 上一次轮询到的系统按键总数
    uint64_t m_lastSysCount = 0;
};

#endif // KEY_MONITOR_H
