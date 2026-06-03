#include "key_monitor.h"
#include <QtGlobal>
#include <algorithm>
#include <cstdint>//新加的

#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)//这一段现在可以通过windows实现
extern "C" uint64_t macos_getSystemKeystrokeCount();
#elif defined(Q_OS_WIN)
extern "C" uint64_t windows_getSystemKeystrokeCount();
extern "C" void     windows_stopKeystrokeMonitor();
#endif

static uint64_t getSystemKeystrokeCount()
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return macos_getSystemKeystrokeCount();
#elif defined(Q_OS_WIN)
    return windows_getSystemKeystrokeCount();
#else
    return 0;
#endif
}


KeyMonitor::KeyMonitor(QObject *parent)
    : QObject(parent)
{
    m_slotTimer = new QTimer(this);
    connect(m_slotTimer, &QTimer::timeout, this, &KeyMonitor::advanceMinuteSlot);

    // 高频轮询定时器（每 200ms 检测一次系统按键计数变化）
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &KeyMonitor::onPollTimer);
}

KeyMonitor::~KeyMonitor()
{
    stopMonitoring();
#if defined(Q_OS_WIN)
    windows_stopKeystrokeMonitor();
#endif
}

void KeyMonitor::startMonitoring()
{
    if (m_monitoring) return;
    m_monitoring = true;

    // 重置数据
    m_currentSlot = 0;
    m_slotCount   = 0;
    m_totalKeys   = 0;
    m_lastSysCount = 0;
    std::fill(m_slots, m_slots + MAX_SLOTS, 0);
    m_startTime = QTime::currentTime();

    // 初始化系统按键计数基线
    m_lastSysCount = getSystemKeystrokeCount();//修改了

    // 每分钟推进一个槽位
    m_slotTimer->start(60000);

    // 每 200ms 轮询系统按键计数
    m_pollTimer->start(200);
}

void KeyMonitor::stopMonitoring()
{
    if (!m_monitoring) return;
    m_monitoring = false;

    m_slotTimer->stop();
    m_pollTimer->stop();
}

// 每 200ms 调用：对比系统按键计数的变化，检测新增按键
void KeyMonitor::onPollTimer()
{
    uint64_t current = getSystemKeystrokeCount();//修改了

    if (m_lastSysCount > 0 && current > m_lastSysCount) {
        uint64_t diff = current - m_lastSysCount;
        // 将每次检测到的按键计入当前分钟槽
        for (uint64_t i = 0; i < diff; ++i) {
            m_totalKeys++;
            m_slots[m_currentSlot]++;
        }
        emit keystrokeDetected(m_totalKeys);
    }
    m_lastSysCount = current;
}

void KeyMonitor::advanceMinuteSlot()
{
    m_currentSlot = (m_currentSlot + 1) % MAX_SLOTS;
    m_slots[m_currentSlot] = 0;
    if (m_slotCount < MAX_SLOTS) m_slotCount++;
    emit dataUpdated();
}

QVector<int> KeyMonitor::recentPerMinute(int minutes) const
{
    int n = std::min(minutes, m_slotCount);
    if (n <= 0) return {};

    QVector<int> result(n);
    for (int i = 0; i < n; ++i) {
        int idx = (m_currentSlot - i + MAX_SLOTS) % MAX_SLOTS;
        result[i] = m_slots[idx];
    }
    return result;
}

int KeyMonitor::peakMinute() const
{
    int peak = 0;
    for (int i = 0; i < m_slotCount; ++i)
        peak = std::max(peak, m_slots[i]);
    return peak;
}

int KeyMonitor::activeMinutes() const
{
    int count = 0;
    for (int i = 0; i < m_slotCount; ++i)
        if (m_slots[i] > 0) count++;
    return count;
}

int KeyMonitor::keystrokesLastMinute() const
{
    if (m_slotCount == 0) return 0;
    return m_slots[m_currentSlot];
}

int KeyMonitor::elapsedMinutes() const
{
    return m_slotCount;
}