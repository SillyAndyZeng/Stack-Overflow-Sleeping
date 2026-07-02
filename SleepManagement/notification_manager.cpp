#include "notification_manager.h"
#include "mainwindow.h"
#include "settings_dialog.h"
#include <QMainWindow>
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QTime>
#include <QDate>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>

// ============================================================
// 生成半月形睡眠图标（纯代码绘制，无需外部图片）
// ============================================================
static QIcon generateSleepIcon()
{
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    // 背景圆（深蓝月亮）
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x4D, 0x96, 0xFF));
    p.drawEllipse(6, 6, 36, 36);

    // 半月高光（遮罩出月牙）
    p.setBrush(Qt::white);
    p.drawEllipse(16, 4, 30, 30);

    // 星星点缀
    p.setBrush(QColor(0xFF, 0xD9, 0x3D));
    p.drawEllipse(10, 12, 4, 4);
    p.drawEllipse(28, 8, 3, 3);
    p.drawEllipse(22, 30, 3, 3);

    p.end();
    return QIcon(pixmap);
}

// 获取应用数据目录
static QString dataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

// ==========================================
// 音效辅助函数（macOS afplay 系统声音）
// ==========================================
namespace {
    void playSound(const QString &name) {
#ifdef Q_OS_MACOS
        QProcess::startDetached("afplay", {"/System/Library/Sounds/" + name + ".aiff"});
#else
        Q_UNUSED(name);
#endif
    }
}

// ============================================================
// NotificationManager 实现
// ============================================================
NotificationManager::NotificationManager(QMainWindow *mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
    , m_trayIcon(nullptr)
    , m_lateNightTimer(nullptr)
    , m_trayMenu(nullptr)
{
    setupTrayIcon();

    // 夜间检测定时器：每分钟检测一次
    m_lateNightTimer = new QTimer(this);
    connect(m_lateNightTimer, &QTimer::timeout, this, &NotificationManager::checkLateNightCondition);
}

NotificationManager::~NotificationManager()
{
    stopMonitoring();
}

void NotificationManager::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(generateSleepIcon());
    m_trayIcon->setToolTip("睡眠守护 · Stack Overflow");

    // 托盘右键菜单
    m_trayMenu = new QMenu(m_mainWindow);

    auto *showAction = m_trayMenu->addAction("📂 显示主窗口");
    connect(showAction, &QAction::triggered, this, &NotificationManager::requestShowWindow);

    m_trayMenu->addSeparator();

    auto *quitAction = m_trayMenu->addAction("🚪 退出程序");
    // 💡 修改：先将主窗口的退出标记设为 true，再执行退出
    connect(quitAction, &QAction::triggered, this, [this]() {
        if (MainWindow *mw = qobject_cast<MainWindow*>(m_mainWindow)) {
            mw->prepareToQuit();
        }
        QApplication::quit();
    });

    m_trayIcon->setContextMenu(m_trayMenu);

    // 左键双击显示窗口
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &NotificationManager::onTrayActivated);

    m_trayIcon->show();
}

void NotificationManager::startMonitoring()
{
    // 每 60 秒检查一次夜间条件
    m_lateNightTimer->start(60000);
}

void NotificationManager::stopMonitoring()
{
    m_lateNightTimer->stop();
}

void NotificationManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        emit requestShowWindow();
    }
}

// ==========================================
// 凌晨条件检测：如果已过 2:00 AM 且当天无睡眠记录
// ==========================================
void NotificationManager::checkLateNightCondition()
{
    // 拼接出配置文件的完整存放路径
    QString cfgPath = dataDir() + "/config.json";
    // 调用全局函数去读文件。如果文件在，currentConfig 就是老配置；如果不在，就是默认设置
    QJsonObject currentCfg = loadUserConfig(cfgPath);

    double stayupBe = currentCfg["stayup_begin"].toDouble(0.0);
    double stayupEn = currentCfg["stayup_end"].toDouble(8.0);

    QTime now = QTime::currentTime();
    // 只在设定的熬夜区间内区间检测，默认0-8点
    if (now.hour() < stayupBe || now.hour() >= stayupEn) {
        m_lateNightNotified = false; // 重置标记
        return;
    }

    // 如果已经弹过通知，不再重复
    if (m_lateNightNotified) return;

    // 检查今天和昨天的作息日记录（凌晨入睡的数据存到昨天的作息日中）
    QDate today = QDate::currentDate();
    QString filePathToday     = dataDir() + "/" + today.toString("yyyy-MM-dd") + ".json";
    QString filePathYesterday = dataDir() + "/" + today.addDays(-1).toString("yyyy-MM-dd") + ".json";
    if (!QFile::exists(filePathToday) && !QFile::exists(filePathYesterday)) {
        // 还没有任何记录，可能是熬夜了
        playSound("Basso"); // 警告音效
        showGenericNotification("🌙 夜深了",
                                "还没睡嘛？快休息吧！",
                                QSystemTrayIcon::Warning);
        m_lateNightNotified = true;
    }
}

// ==========================================
// 各类气泡提醒
// ==========================================
void NotificationManager::showSleepNotification(const QString &sleepTime)
{
    showGenericNotification("💤 晚安守护",
                            QString("入睡时间已记录：%1\n系统已进入静默模式，好好休息～").arg(sleepTime));
}

void NotificationManager::showWakeNotification(const QString &wakeTime)
{
    showGenericNotification("🌅 早安，打工人",
                            QString("起床时间：%1\n新的一天开始啦！").arg(wakeTime));
}

void NotificationManager::showAchievementNotification(const QString &badgeText)
{
    showGenericNotification("🎖 成就解锁！", badgeText);
}

void NotificationManager::showGenericNotification(const QString &title, const QString &message,
                                                    QSystemTrayIcon::MessageIcon icon)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, icon, 5000);
    }
}

void NotificationManager::playButtonClick()
{
    playSound("Pop");
}

void NotificationManager::playAchievement()
{
    playSound("Hero");
}

void NotificationManager::playWarning()
{
    playSound("Basso");
}
