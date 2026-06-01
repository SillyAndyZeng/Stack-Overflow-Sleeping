#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QIcon>

class QMainWindow;

// 系统托盘 & 桌面气泡提醒管理器
class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(QMainWindow *mainWindow);
    ~NotificationManager() override;

    // 启动定时检测
    void startMonitoring();
    void stopMonitoring();

    // 展示气泡提醒
    void showSleepNotification(const QString &sleepTime);
    void showWakeNotification(const QString &wakeTime);
    void showAchievementNotification(const QString &badgeText);
    void showGenericNotification(const QString &title, const QString &message,
                                 QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    // 音效（macOS 系统声音）
    void playButtonClick();
    void playAchievement();
    void playWarning();

    QSystemTrayIcon* trayIcon() const { return m_trayIcon; }

signals:
    void requestShowWindow();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void checkLateNightCondition();

private:
    void setupTrayIcon();

    QSystemTrayIcon *m_trayIcon;
    QMainWindow     *m_mainWindow;
    QTimer          *m_lateNightTimer;   // 凌晨检测（每分钟）
    QMenu           *m_trayMenu;

    // 避免重复弹窗去重
    bool m_lateNightNotified = false;
};

#endif // NOTIFICATION_MANAGER_H
