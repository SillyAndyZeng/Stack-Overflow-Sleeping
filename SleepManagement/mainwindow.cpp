#include "mainwindow.h" // 引入对应的头文件声明
#include <QApplication>  // 用于 qApp 宏和 aboutToQuit 信号
#include "ui_mainwindow.h"  //引入由 mainwindow.ui 编译生成的底层界面头文件，不引它就无法通过 ui-> 指针访问控件
#include "achievement_manager.h"
#include "sleep_core.h"       // 算分引擎
#include "sleep_charts.h"     // 可视化图表
#include "settings_dialog.h"  // 用户自定义设置
#include "notification_manager.h" // 系统托盘与通知
#include "key_monitor.h"      // 键盘敲击监测
#include "keyboard_chart.h"   // 键盘活跃度图表
#include <QMessageBox>        // 引入 Qt 官方的弹窗对话框类，用于实现各种警告、信息提示弹窗
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>  // 引入时间编辑器控件的类声明
#include <QDateEdit>
#include <QSpinBox>         // 引入数字微调框控件的类声明
#include <QCalendarWidget>  // 引入日历控件的类声明
#include <QFile>
#include <QDir>
#include <QPropertyAnimation> // 按钮动效
#include <QParallelAnimationGroup> // 日历淡入动画组
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox> // 美化的手动编辑时间窗口
#include <QFormLayout> // 菜单相关
#include <QTextBrowser>
#include <QCheckBox>
// 用于非阻塞（非sleep式）的停顿
#include <QEventLoop>
#include <QTimer>
// 用于 PDF 导出
#include <QTextDocument>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QFileDialog>
// 用于流式读取外部文件数据
#include <QTextStream>
//用于窗口缩放
#include <QResizeEvent>
#include <QShowEvent>

/*
当界面上的按钮被点击时，具体要做什么计算、弹出什么提示，全部写在这里。
什么是 ui->？
 ui是由 .ui 文件编译生成的界面类对象。
 界面上所有的组件（例如 calendarWidget 、timeEdit_sleep 、spinBox_nap  等）都挂在 ui 指针下面。在 .cpp 里输入 ui-> 就能调出它们。
什么是 .value() 和 .time()？
 不同控件取值的方式不同：QSpinBox（数字微调框）用 .value() 拿数字 ；QTimeEdit（时间组件）用 .time().hour() 拿小时数 。
数据的流转闭环：
 用户在界面（ui->...）上输入数据 -> 槽函数通过代码把数据捞出来 -> 喂给纯 C++ 类（SleepAnalyzer）去算分 -> 把算出来的分数包装成弹窗（QMessageBox）反馈渲染给用户。

*/
// 构造函数的具体实现：执行窗口的初始化
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)   // 先调用基类 QMainWindow 的构造函数，完成最基础的通用窗口初始化
    , ui(new Ui::MainWindow) // 动态分配内存，实例化负责管理 UI 控件的界面类
{
    ui->setupUi(this); // 核心！这个函数把在图形化界面里拖拽的所有按钮、日历、输入框等，真正地绘制、布置到当前的这个MainWindow窗口上


    // 限制日历最大可选日期为今天，未来的日期将被置灰且无法点击
    ui->calendarWidget->setMaximumDate(QDate::currentDate());

    // 引入 QTimer 每分钟动态刷新日历
    QTimer *calendarRefreshTimer = new QTimer(this);
    //connect：连接信号与槽的电线。可以手动写，另外如果槽函数的名称符合on_控件名_信号名()的格式，qt会自动实现connect
    //第一个参数：谁发送的信号；第二个参数：发出的信号本身是什么；第三个：接收信号的对象；第四个：槽，也就是要执行的代码
    //这里的槽用了lambda表达式，函数体就是大括号里的内容
    connect(calendarRefreshTimer, &QTimer::timeout, this, [this]() {
        QDate realToday = QDate::currentDate();

        // 1. 刷新最大可选日期
        ui->calendarWidget->setMaximumDate(realToday);

        // 2. 如果此时日历选中的还是昨天（说明刚好跨天了），就自动跳到今天
        // （加上判断是为了防止用户正在查看更早的历史记录时被强行拉回今天）
        if (ui->calendarWidget->selectedDate() == realToday.addDays(-1)) {
            ui->calendarWidget->setSelectedDate(realToday);
        }
    });
    calendarRefreshTimer->start(60000); // 60000毫秒 = 1分钟检查一次，性能开销不大

    connect(ui->calendarWidget, &QCalendarWidget::selectionChanged,
        this, &MainWindow::onCalendarDateSelected);//构造函数绑定日历信号

    // 【新增】创建下拉菜单并绑定到清理按钮上
    QMenu *clearMenu = new QMenu(this);

    // 创建三个具体的菜单动作（Action）
    QAction *actClearToday = new QAction("🗑️ 清空当前选中日期", this);
    QAction *actClearRange = new QAction("📅 清空指定日期区间", this);
    QAction *actClearAll   = new QAction("🔥 清空全部数据 (跑路)", this);

    // 把动作装进菜单里
    clearMenu->addAction(actClearToday);
    clearMenu->addAction(actClearRange);
    clearMenu->addAction(actClearAll);

    // 将菜单挂载到在UI里建的清理工具按钮上
    ui->btn_clear_data->setMenu(clearMenu);

    // 将这三个动作的触发信号与要写的槽函数连接起来
    connect(actClearToday, &QAction::triggered, this, &MainWindow::on_action_clear_today_triggered);
    connect(actClearRange, &QAction::triggered, this, &MainWindow::on_action_clear_range_triggered);
    connect(actClearAll,   &QAction::triggered, this, &MainWindow::on_action_clear_all_triggered);

    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // connect函数：当收到大模型回信时，自动把信（reply）交给 on_api_reply_finished 函数去拆解
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::on_api_reply_finished);

    // 【初始化时】把四个显示框设为只读，不允许键盘修改
    ui->lineEdit_sleep_disp->setReadOnly(true);
    ui->lineEdit_wake_disp->setReadOnly(true);
    ui->lineEdit_duration_disp->setReadOnly(true);
    ui->lineEdit_selected_date->setReadOnly(true);

    // SpinBox 数值范围限制（防呆）
    ui->spinBox_nap->setRange(0, 480);      // 午睡上限 8 小时
    ui->spinBox_exercise->setRange(0, 480); // 运动上限 8 小时
    ui->spinBox_sit->setRange(0, 1440);     // 久坐上限 24 小时
    ui->spinBox_nap->setSuffix(" 分钟");
    ui->spinBox_exercise->setSuffix(" 分钟");
    ui->spinBox_sit->setSuffix(" 分钟");
    // 默认隐藏历史修改按钮（只有点选过去的日子才展示）
    ui->btn_edit_sleep->setVisible(false);
    ui->btn_edit_wake->setVisible(false);
    // 默认隐藏手动保存按钮
    // ui->btn_save_report->setVisible(false);

    // 添加"？"帮助按钮
    auto *btnHelp = new QPushButton("?", ui->centralwidget);//改成ui->centralwidget为了缩放
    btnHelp->setObjectName("btn_help");
    btnHelp->setFixedSize(28, 28);
    btnHelp->move(850, 12);
    btnHelp->setStyleSheet(
        "QPushButton { background-color: #E0E0E0; color: #555; border-radius: 14px; "
        "font-size: 16px; font-weight: bold; border: none; }"
        "QPushButton:hover { background-color: #4D96FF; color: white; }");
    connect(btnHelp, &QPushButton::clicked, this, [this]() {
        if (showWelcomeDialog(true))
            QApplication::quit();
    });

    refreshCalendarColors();//新增:启动时加载历史颜色
    updateAchievementDisplay(); // 启动时加载并显示成就

    // ==========================================================
    // 【新增】动态创建高级日历色彩图例
    // ==========================================================
    // 1. 给我们在 UI 里留好的 widget_legend 容器设置一个网格布局
    QGridLayout *legendLayout = new QGridLayout(ui->widget_legend);
    legendLayout->setContentsMargins(10, 5, 10, 5); // 设置内边距
    legendLayout->setHorizontalSpacing(15);         // 设置色块和文字的间距
    legendLayout->setVerticalSpacing(8);            // 设置行间距

    // 2. 定义我们的图例数据（颜色数值要和 refreshCalendarColors 里完全一致）
    struct LegendItem {
        unsigned int color;
        QString text;
    };

    QList<LegendItem> legendList = {
        {0x6BCB77, "良好：睡眠充足 (score=3)"},
        {0x1CFCF4, "正常：score=2"},
        {0xFFFF66, "一般：score=1"},
        {0xC8A2C8, "懒觉：起床晚于一般起床时间过久"},
        {0xFF6B6B, "通宵：修仙暴击"}
    };

    // 3. 循环将这些图例塞进布局中
    for (int i = 0; i < legendList.size(); ++i) {
        // 创建左侧的小色块（用 QLabel 配合 CSS 绘制出圆角）
        QLabel *colorLabel = new QLabel(this);
        colorLabel->setFixedSize(16, 16); // 固定大小
        colorLabel->setStyleSheet(QString(
                                      "background-color: #%1;"
                                      "border-radius: 8px;"        // 圆角半径为大小的一半，完美切成正圆形
                                      "border: 1px solid #DCDCDC;" // 加一个淡淡的灰色边框，防止浅色块在白底上看不清
                                      ).arg(legendList[i].color, 6, 16, QChar('0'))); // 将整型转为16进制颜色字符串

        // 创建右侧的文本解释
        QLabel *textLabel = new QLabel(legendList[i].text, this);
        // 可以稍微美化下字体，让它看起来更高级
        textLabel->setStyleSheet("font-family: 'Microsoft YaHei'; font-size: 11px; color: #555555;");

        // 塞进网格布局：第 i 行，第 0 列放色块；第 i 行，第 1 列放文字
        legendLayout->addWidget(colorLabel, i, 0, Qt::AlignVCenter | Qt::AlignLeft);
        legendLayout->addWidget(textLabel, i, 1, Qt::AlignVCenter | Qt::AlignLeft);
    }
    // 让布局里的元素都靠上对齐，不要散开
    legendLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    // ==========================================================

    // ==========================================================
    // 【新增：PDF 导出按钮】放在 AI 文本框下方
    auto *btnExportPdf = new QPushButton("📄 导出 PDF", ui->centralwidget);//改成ui->centralwidget为了缩放
    btnExportPdf->setObjectName("btn_export_pdf");
    btnExportPdf->setGeometry(740, 635, 101, 28);
    btnExportPdf->setStyleSheet(
        "QPushButton { background-color: #E8F0FE; color: #1A73E8; border: 1px solid #1A73E8; "
        "border-radius: 6px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #D2E3FC; }");
    connect(btnExportPdf, &QPushButton::clicked, this, &MainWindow::on_btn_export_pdf_clicked);
    // ==========================================================
    // 【启动时加载用户配置】
    QString cfgPath = dataDir() + "/config.json";
    QJsonObject userCfg = loadUserConfig(cfgPath);
    applyUserConfig(userCfg);
    // ==========================================================

    // ==========================================================
    // 【保存到系统托盘与气泡通知】
    m_notificationMgr = new NotificationManager(this);
    connect(m_notificationMgr, &NotificationManager::requestShowWindow, this, [this]() {
        QDate realToday = QDate::currentDate();
        // 【新增修复】每次从托盘唤醒主窗口时，强制刷新一次日期限制
        ui->calendarWidget->setMaximumDate(realToday);
        // 可选：每次点开都强制回到“今天”，方便打卡
        ui->calendarWidget->setSelectedDate(realToday);
        showNormal();
        activateWindow();
        raise();
        // 从托盘恢复时按 config 决定是否弹窗，若用户选退出则关闭程序
        if (showWelcomeDialog())
            QApplication::quit();
    });
    m_notificationMgr->startMonitoring();
    // ==========================================================

    // ==========================================================
    // 【初始化键盘敲击频率监测】
    m_keyMonitor = new KeyMonitor(this);
    m_keyMonitor->startMonitoring();
    // ==========================================================

    // ==========================================================
    // 【设置显示区域透明度动效】（日历切换时淡入淡出）
    auto setupFade = [&](QWidget *w) {
        auto *effect = new QGraphicsOpacityEffect(this);
        effect->setOpacity(1.0);
        w->setGraphicsEffect(effect);
        m_displayEffects.append(effect);
    };
    setupFade(ui->lineEdit_selected_date);
    setupFade(ui->lineEdit_sleep_disp);
    setupFade(ui->lineEdit_wake_disp);
    setupFade(ui->lineEdit_duration_disp);
    setupFade(ui->spinBox_nap);
    setupFade(ui->spinBox_exercise);
    setupFade(ui->spinBox_sit);
    // ==========================================================

    // 启动时强制显示欢迎说明书（首次启动必弹）
    // 如果用户选择了"退出程序"，设置标记供 main() 判断
    m_shouldQuitOnStart = showWelcomeDialog(true);

    // 监听程序即将退出信号，确保 closeEvent 不误弹托盘通知（覆盖所有退出路径）
    connect(qApp, &QApplication::aboutToQuit, this, [this]() {
        m_isQuitting = true;
    });
    // ==========================================
    // 窗口缩放：记录初始控件坐标
    // 注意：必须放在所有动态创建的按钮之后，例如 btnHelp、btnExportPdf 之后
    // ==========================================
    setMinimumSize(600, 450);
    rememberBaseGeometry();


}
MainWindow::~MainWindow()
{
    delete ui; //析构函数的具体实现：回收在构造函数中 new 出来的 ui 指针占用的内存，防止内存泄漏
}

// ==========================================
// 欢迎说明书弹窗
// ==========================================
bool MainWindow::showWelcomeDialog(bool force)
{
    // 检查是否已勾选"不再显示"（force 为 true 时忽略该设置）
    QString cfgPath = dataDir() + "/config.json";
    QJsonObject cfg = loadUserConfig(cfgPath);
    if (!force && !cfg["show_welcome"].toBool(true)) return false;

    QDialog dialog(this);
    dialog.setWindowTitle("📖 睡眠守护 · 用户说明书");
    dialog.setMinimumSize(620, 520);
    dialog.resize(680, 560);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 12, 16, 12);

    auto *browser = new QTextBrowser(&dialog);
    browser->setOpenExternalLinks(true);
    browser->setHtml(buildManualHtml());
    browser->setStyleSheet(
        "QTextBrowser { "
    "background-color: #FAFAFA; "
    "color: #222222; "
    "border: 1px solid #E0E0E0; "
    "border-radius: 8px; "
    "padding: 12px; "
    "font-size: 14px; "
    "line-height: 1.6; "
    "}");
    layout->addWidget(browser, 1);

    // 底部：勾选框 + 关闭按钮
    auto *bottomBar = new QHBoxLayout();
    auto *dontShowAgain = new QCheckBox("不再显示此欢迎页（可在设置区点击 ? 重新打开）", &dialog);
    dontShowAgain->setStyleSheet("font-size: 12px; color: #888;");
    bottomBar->addWidget(dontShowAgain);
    bottomBar->addStretch();

    auto *closeBtn = new QPushButton("开始使用", &dialog);
    closeBtn->setFixedSize(110, 32);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #4D96FF; color: white; border-radius: 6px; "
        "font-size: 13px; font-weight: bold; border: none; }"
        "QPushButton:hover { background-color: #3A7BD5; }");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    bottomBar->addWidget(closeBtn);
    // 增加一个完全退出程序的按钮
    auto *quitBtn = new QPushButton("退出程序", &dialog);
    quitBtn->setFixedSize(110, 32);
    quitBtn->setStyleSheet(
        "QPushButton { background-color: #E74C3C; color: white; border-radius: 6px; "
        "font-size: 13px; font-weight: bold; border: none; }"
        "QPushButton:hover { background-color: #C0392B; }");
    bool shouldQuit = false;
    connect(quitBtn, &QPushButton::clicked, &dialog, [&]() {
        shouldQuit = true;
        dialog.accept();
    });
    bottomBar->addWidget(quitBtn);

    layout->addLayout(bottomBar);

    dialog.exec();

    cfg["show_welcome"] = !dontShowAgain->isChecked();
    saveUserConfig(cfgPath, cfg);

    return shouldQuit;
}

// ==========================================
// 说明书 HTML 内容
// ==========================================
QString MainWindow::buildManualHtml()
{
    // Qt 中以 ":/" 开头的路径，代表读取我们打包进程序里的 QRC 内嵌文件资源
    // ":/manual.html" 是一个虚拟路径，冒号表示 QRC 资源树根目录。
    // 当 QFile 寻找这个虚拟路径时，它将从可执行程序内部载入数据，而不是读取硬盘上的实际文件
    QFile file(":/manual.html");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 如果文件读取失败，放一个保底的 HTML 格式文本，防止软件直接开天窗
        return QStringLiteral("<h2 style='color:red; text-align:center;'>⚠️ 无法加载说明书内容，请检查项目编译设置。</h2>");
    }
    // 使用 QTextStream 流式读取文件
    QTextStream in(&file);
    // Qt 6 中 QTextStream 默认是以 UTF-8 读取文本文件，此处可以安全省心
    return in.readAll();
}


// ==========================================
// 关闭窗口 → 最小化到系统托盘
// ==========================================
void MainWindow::closeEvent(QCloseEvent *event)
{
    // 如果程序正在主动退出，不弹托盘通知，直接关闭
    if (!m_notificationMgr || m_isQuitting) {
        event->accept();
        return;
    }
    hide();
    m_notificationMgr->showGenericNotification("💤 睡眠守护",
        "程序已最小化到系统托盘，守护仍在继续～");
    event->ignore(); // 不真正关闭
}
// ==========================================
// 窗口缩放
// mainwindow.ui 目前使用的是绝对坐标 geometry。
// 所以这里不再用 QGraphicsView 缩放整个界面，
// 而是记录所有顶层子控件的原始 geometry，
// 在窗口 resize 时按比例重新计算位置和大小。
// ==========================================
void MainWindow::rememberBaseGeometry()
{
    if (!ui || !ui->centralwidget)
        return;

    m_baseGeometry.clear();

    // mainwindow.ui 里的设计尺寸是 900 x 680
    // 如果后面你在 Qt Designer 里改了主窗口初始尺寸，这里也要同步改。
    m_baseSize = QSize(900, 680);

    const auto children = ui->centralwidget->findChildren<QWidget*>(
        QString(),
        Qt::FindDirectChildrenOnly
        );

    for (QWidget *w : children) {
        if (!w)
            continue;

        m_baseGeometry.insert(w, w->geometry());
    }
}

void MainWindow::applyResponsiveGeometry()
{
    if (!ui || !ui->centralwidget || m_baseGeometry.isEmpty())
        return;

    const QSize curSize = ui->centralwidget->size();

    if (curSize.width() <= 0 || curSize.height() <= 0)
        return;

    const double sx = double(curSize.width()) / double(m_baseSize.width());
    const double sy = double(curSize.height()) / double(m_baseSize.height());

    for (auto it = m_baseGeometry.constBegin(); it != m_baseGeometry.constEnd(); ++it) {
        QWidget *w = it.key();

        if (!w)
            continue;

        const QRect r = it.value();

        const int newX = qRound(r.x() * sx);
        const int newY = qRound(r.y() * sy);
        const int newW = qRound(r.width() * sx);
        const int newH = qRound(r.height() * sy);

        w->setGeometry(newX, newY, newW, newH);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    applyResponsiveGeometry();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    // 第一次 show 时窗口尺寸可能还没完全稳定，
    // 所以延迟到事件循环空闲时再重排一次。
    QTimer::singleShot(0, this, [this]() {
        applyResponsiveGeometry();
    });
}

// ==========================================
// 按钮缩放动效（脉冲动画）
// ==========================================
void MainWindow::pulseButton(QPushButton *btn)
{
    if (!btn) return;
    QRect orig = btn->geometry();
    // 向四周各扩大 6px
    QRect expanded = orig.adjusted(-6, -6, 6, 6);

    auto *anim = new QPropertyAnimation(btn, "geometry", this);
    anim->setDuration(180);
    anim->setStartValue(orig);
    anim->setKeyValueAt(0.5, expanded);
    anim->setEndValue(orig);
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QString MainWindow::dataDir()//将文件存到固定的应用数据目录
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);   // 目录不存在时自动创建
    return dir;
}

// ==========================================
// 日历颜色刷新：扫描 dataDir 内所有 json 文件，按状态染色
// 颜色规则：红=熬穿  黄=熬夜  紫=久坐超标  绿=睡眠良好  蓝=正常
// ==========================================
void MainWindow::refreshCalendarColors()
{
    // 先清掉所有旧颜色，避免脏数据残留
    ui->calendarWidget->setDateTextFormat(QDate(), QTextCharFormat());

    QDir dir(dataDir());
    const QStringList files = dir.entryList({"*.json"}, QDir::Files);

    for (const QString &fileName : files) {
        // 从文件名（yyyy-MM-dd.json）解析日期
        QString dateStr = fileName;
        dateStr.chop(5);  // 去掉 ".json"
        QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
        if (!date.isValid()) continue;

        QFile file(dataDir() + "/" + fileName);
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (!doc.isObject()) continue;

        QJsonObject obj = doc.object();

        // 读取状态字段
        // 如果起床或入睡存在一个是未记录状态，则不染色，直接退出函数
        int s_h = obj["sleep_hour"].toInt();
        int s_m = obj["sleep_min"].toInt();
        int w_h = obj["wake_hour"].toInt();
        int w_m = obj["wake_min"].toInt();
        if (s_h == -1 || s_m == -1 || w_h == -1 || w_m == -1)
            return;
        // no_night_sleep / oversleep 是你即将补充的布尔字段，
        // 若文件里尚未存在，toBool(false) 会安全返回 false，不影响现有记录
        bool noNightSleep = obj["no_night_sleep"].toBool(false);
        // bool stayUp       = obj["stay_up_late"].toBool(false);
        bool oversleep    = obj["oversleep"].toBool(false);
        // int  sit          = obj["sit_min"].toInt(0);
        // int  exe          = obj["exercise_min"].toInt(0);
        int  score        = obj["sleep_score"].toInt(0);

        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);

        // 使用十六进制数表示RGB颜色
        QColor bgColor;
        if (noNightSleep)                          bgColor = QColor(0xFF6B6B); // 红：熬穿
        else if (oversleep || score == 0)           bgColor = QColor(0xC8A2C8); // 紫：懒觉
        else if (score == 1)                        bgColor = QColor(0xFFFF66); //橙：一分：小于6h或大于10h
        else if (score == 2)                        bgColor = QColor(0x1CFCF4); //蓝：两分：6-7h或9-10h
        else if (score == 3)                        bgColor = QColor(0x6BCB77); // 绿：三分，7-9分

        fmt.setBackground(QBrush(bgColor)); //显式用 QBrush
        fmt.setForeground(QBrush(Qt::black)); //设置前景色(文字颜色)作为保底，保证文字可读

        ui->calendarWidget->setDateTextFormat(date, fmt);
    }
}

// ==========================================
// 成就检查：在每次打卡后调用，达成条件时弹出提示
// ==========================================
void MainWindow::checkAndShowAchievements()
{
    AchievementManager am(dataDir());
    QString badge = am.currentBadge();
    if (badge.isEmpty()) return;

    int ci = am.checkInDays();
    int es = am.earlySleepDays();

    // 只在刚好达到里程碑的那一天弹出，避免每次都弹
    bool isMilestone = (ci == 7 || ci == 30 || es == 3 || es == 7);
    if (!isMilestone) return;

    m_notificationMgr->playAchievement(); // 成就音效
    QString msg = QString("恭喜解锁新成就：\n\n%1\n\n连续打卡 %2 天 · 连续早睡 %3 天\n\n继续保持！")
                      .arg(badge).arg(ci).arg(es);
    QMessageBox::information(this, "🎖 成就解锁！", msg);
    m_notificationMgr->showAchievementNotification(badge);
}

// ==========================================
// 读取成就逻辑并实时更新到主界面 UI
// ==========================================
void MainWindow::updateAchievementDisplay()
{
    // 实例化你的成就管理器，传入数据文件夹路径（dataDir()为你定义获取路径的函数）
    AchievementManager am(dataDir());

    // 获取当前解锁的所有徽章文本
    QString badges = am.currentBadge();

    if (badges.isEmpty()) {
        ui->label_achievements->setText("✨ 暂无成就，规律作息即可解锁徽章！");
        ui->label_achievements->setStyleSheet("color: #888888; font-family: 'Microsoft YaHei';");
    } else {
        // 如果有成就，给它套上一个精致的样式
        ui->label_achievements->setText("已获成就: " + badges);
        ui->label_achievements->setStyleSheet("color: #FF8C00; font-weight: bold; font-family: 'Microsoft YaHei';");
    }
}

// 🎓【新增辅助函数】动态读取界面上的时间，计算晚间睡眠时长并显示
void MainWindow::updateDurationDisplay()
{
    QString s_text = ui->lineEdit_sleep_disp->text();
    QString w_text = ui->lineEdit_wake_disp->text();

    // 1. 如果还没记录，或者通宵了
    if (s_text == "未记录" || w_text == "未记录" || s_text.isEmpty() || w_text.isEmpty()) {
        ui->lineEdit_duration_disp->setText("--:--");
        return;
    }
    if (s_text == "通宵" || w_text == "通宵") {
        ui->lineEdit_duration_disp->setText("00:00");
        return;
    }

    // 2. 将文本转换回 QTime 时间对象
    QTime s_time = QTime::fromString(s_text, "HH:mm");
    QTime w_time = QTime::fromString(w_text, "HH:mm");

    // 3. 计算时间差
    // SleepAnalyzer类里有一个类似的函数calculateNightSleep()能实现此功能，但是要实现的话还得实例化一个临时的SleepAnalyzer对象，效率不高
    if (s_time.isValid() && w_time.isValid()) {
        int s_min = s_time.hour() * 60 + s_time.minute();
        int w_min = w_time.hour() * 60 + w_time.minute();

        // 如果起床时间在数字上小于入睡时间（比如 23:00 睡，08:00 起），说明跨天了
        if (w_min < s_min) {
            w_min += 24 * 60;
        }

        int diff = w_min - s_min;
        int h = diff / 60;
        int m = diff % 60;

        // 格式化为 08:30 这样的格式
        ui->lineEdit_duration_disp->setText(QString("%1:%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')));
    }
}

//数据回填
void MainWindow::onCalendarDateSelected()
{
    QDate date = ui->calendarWidget->selectedDate();
    // 显示选择的日期
    QString dateStr = date.toString("yyyy-MM-dd");
    ui->lineEdit_selected_date->setText(dateStr);

    QDate today = QDate::currentDate();

    // 【手动编辑】如果选中的不是今天（即手动编辑之前的日子）
    if (date != today) {
        // 实时打卡按钮变成不可选中
        ui->btn_sleep->setEnabled(false);
        ui->btn_wake->setEnabled(false);
        // 手动修改按钮显现出来
        ui->btn_edit_sleep->setVisible(true);
        ui->btn_edit_wake->setVisible(true);
        ui->btn_save_report->setVisible(true); // 显示手动保存按钮
    } else {
        // 如果日期是今天，则恢复打卡按钮，手动修改按钮其实也不用隐藏
        ui->btn_sleep->setEnabled(true);
        ui->btn_wake->setEnabled(true);
        // ui->btn_edit_sleep->setVisible(false);
        // ui->btn_edit_wake->setVisible(false);
        ui->btn_save_report->setVisible(false); // 隐藏手动保存按钮
    }

    // 尝试寻找本地文件
    QString filePath = dataDir() + "/" + date.toString("yyyy-MM-dd") + ".json";
    QFile file(filePath);

    // 🎓【诉求四】如果这一天没有数据
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { //[cite: 43]
        // 清空所有控件，显示框显示“未记录”
        ui->spinBox_nap->setValue(0);
        ui->spinBox_exercise->setValue(0);
        ui->spinBox_sit->setValue(0);
        ui->lineEdit_sleep_disp->setText("未记录");
        ui->lineEdit_wake_disp->setText("未记录");
        // 刷新晚间睡眠时长显示！
        updateDurationDisplay();
        return;
    }

    // 如果有数据，则读取回填
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    int s_h = obj["sleep_hour"].toInt(); //[cite: 43]
    int s_m = obj["sleep_min"].toInt(); //[cite: 43]
    int w_h = obj["wake_hour"].toInt(); //[cite: 43]
    int w_m = obj["wake_min"].toInt(); //[cite: 43]

    // 把读取到的入睡、起床、白天活动数据回填输入控件[cite: 43]
    // 通宵的晚上，起床和入睡时间会被设置为-1，具体见sleep_core.h
    if (s_h == -1 && w_h == -1) {
        ui->lineEdit_sleep_disp->setText("通宵");
        ui->lineEdit_wake_disp->setText("通宵");
    } 
    //dyq 260529 2300添加 依旧是为了解决没有把数据读入日历
    else if (w_h == -2) {
    // 只记录了入睡，尚未起床
    ui->lineEdit_sleep_disp->setText(
        QString("%1:%2").arg(s_h, 2, 10, QChar('0')).arg(s_m, 2, 10, QChar('0')));
    ui->lineEdit_wake_disp->setText("未记录");
    }
    //
    else {
        ui->lineEdit_sleep_disp->setText(QString("%1:%2").arg(s_h, 2, 10, QChar('0')).arg(s_m, 2, 10, QChar('0')));
        ui->lineEdit_wake_disp->setText(QString("%1:%2").arg(w_h, 2, 10, QChar('0')).arg(w_m, 2, 10, QChar('0')));
    }
    ui->spinBox_nap->setValue(obj["day_sleep_min"].toInt()); //[cite: 43]
    ui->spinBox_exercise->setValue(obj["exercise_min"].toInt()); //[cite: 43]
    ui->spinBox_sit->setValue(obj["sit_min"].toInt()); //[cite: 43]

    // 刷新晚间睡眠时长显示！
    updateDurationDisplay();

    // ★ 日历切换数据：淡入刷新动效（透明度回弹脉冲）
    if (!m_displayEffects.isEmpty()) {
        auto *group = new QParallelAnimationGroup(this);
        // *effect显式提示effect是指针。不加*，effect也是指针
        for (auto *effect : std::as_const(m_displayEffects)) {
            auto *anim = new QPropertyAnimation(effect, "opacity", this);
            anim->setDuration(300);
            anim->setStartValue(1.0);
            anim->setKeyValueAt(0.4, 0.3);
            anim->setEndValue(1.0);
            anim->setEasingCurve(QEasingCurve::OutQuad);
            group->addAnimation(anim);
        }
        group->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

// ==========================================
// 手动设置通宵按钮，仅仅是设置通宵，下一步可以按我起床了，也可以按生成报告和保存数据，毕竟这俩是一样的
void MainWindow::on_btn_set_nosleep_clicked(){
    // 首先弹出一个带按钮的提示框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认通宵了吗");
    msgBox.setText(QString("嘛，真没睡假没睡"));
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *btnCancel = msgBox.addButton("❌ 算了算了", QMessageBox::RejectRole);
    QPushButton *btnAccept = msgBox.addButton("我愿意。", QMessageBox::AcceptRole);

    msgBox.exec(); // 阻塞程序，等待用户在弹窗上做出点击选择

    if (msgBox.clickedButton() == btnCancel) return;
    else if (msgBox.clickedButton() == btnAccept){
        // 用户通宵了，强行把时间全部设为 -1
        // 会触发你们 sleep_core.h 里构造函数的变量设置：noNightSleep = true
        ui->lineEdit_sleep_disp->setText("通宵");
        ui->lineEdit_wake_disp->setText("通宵");
        updateDurationDisplay();
    }
}

// ==========================================
// 动作 1：用户点击【准备入睡】按钮，通过信号-槽机制触发此函数
// ==========================================
void MainWindow::on_btn_sleep_clicked()
{
    // 0. 按钮动效 + 音效
    pulseButton(ui->btn_sleep);
    m_notificationMgr->playButtonClick();

    // 1. 获取当前系统的精确时间（时、分、秒）
    QTime currentTime = QTime::currentTime();
    ui->lineEdit_sleep_disp->setText(currentTime.toString("HH:mm"));

    // 偷偷读取“今天早上”残留的起床时间，用来算一卦今天如果和昨天一个时候起，大概能睡多久
    // 后期这个逻辑可能改成，和用户设置的常规起床时间比，能睡多久
    QString w_text = ui->lineEdit_wake_disp->text();

    // 准备默认提示语（默认版本是为了防止今早没有起床记录）
    QString estimateMsg = "系统已进入静默模式，请放下手机，好好休息哦~";

    if (w_text != "未记录" && w_text != "通宵" && !w_text.isEmpty()) {
        QTime w_time = QTime::fromString(w_text, "HH:mm");
        if (w_time.isValid()) {
            // 白嫖SleepAnalyzer的函数：把现在的入睡时间和今早的起床时间喂进去
            SleepAnalyzer tempAnalyzer(currentTime.hour(), currentTime.minute(), w_time.hour(), w_time.minute(), 0, 0, 0);
            int diffMinutes = tempAnalyzer.calculateNightSleep();

            int h = diffMinutes / 60;
            int m = diffMinutes % 60;

            // 拼装一个充满仪式感的预估提示语
            estimateMsg = QString("参考您今早 %1 的起床习惯：\n如果您明早也在这个时间醒来，您将获得约【%2小时 %3分钟】的睡眠。\n\n放平心态，放下手机，晚安！")
                              .arg(w_text).arg(h).arg(m);
        }
    }
    // 只要点入睡，就把起床记录清空，确保时长一定显示 --:--，就算早上不点起床，连点两次睡眠也没关系
    ui->lineEdit_wake_disp->setText("未记录");
    updateDurationDisplay(); // 刷新时长

    // ui->btn_sleep->setEnabled(false); // 没必要

    // 在弹窗之前，立即把入睡时间持久化到磁盘 dyq 260529晚2300添加
    // 直接写最小化JSON，不走SleepJsonExporter算分管道（避免-2占位符扰乱计算）
    QDate sleepDay = getSleepDay(QDateTime::currentDateTime());
    QString dateStr = sleepDay.toString("yyyy-MM-dd");
    QJsonObject partialObj;
    partialObj["date"]          = dateStr;
    partialObj["sleep_hour"]    = currentTime.hour();
    partialObj["sleep_min"]     = currentTime.minute();
    partialObj["wake_hour"]     = -2;
    partialObj["wake_min"]      = -2;
    partialObj["day_sleep_min"] = 0;
    partialObj["exercise_min"]  = 0;
    partialObj["sit_min"]       = 0;
    QString fullPath = dataDir() + "/" + dateStr + ".json";
    QFile partialFile(fullPath);
    if (partialFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        partialFile.write(QJsonDocument(partialObj).toJson(QJsonDocument::Indented));
        partialFile.close();
    }

    // 3. 弹出一个温馨的提示框 + 系统托盘通知
    QMessageBox::information(this, "晚安守护", "已记录入睡时间！\n系统已进入静默模式，请放下手机，好好休息哦~\n再次点按这个按钮会覆盖上次的数据，不用急");
    m_notificationMgr->showSleepNotification(currentTime.toString("HH:mm"));
}

// ==========================================
// 动作 2：用户点击【我醒了】按钮，进行数据结算
// ==========================================
void MainWindow::on_btn_wake_clicked()
{
    // 0. 防呆：如果入睡时间还没记录，提示用户先去点【准备入睡】
    QString s_text = ui->lineEdit_sleep_disp->text();
    if (s_text.isEmpty() || s_text == "未记录") {
        QMessageBox::information(this, "提示",
                                 "您还没有记录入睡时间，请先点击【准备入睡】按钮，\n"
                                 "或手动编辑入睡时间后再点击【我醒了】。");
        return;
    }

    // 0. 按钮动效 + 音效
    pulseButton(ui->btn_wake);
    m_notificationMgr->playButtonClick();

    // 1. 获取当前系统那一瞬间的起床时间
    QTime currentTime = QTime::currentTime();
    //捞取时间填写到lineEdit_wake_disp文本框中
    ui->lineEdit_wake_disp->setText(currentTime.toString("HH:mm"));
    updateDurationDisplay(); // 刷新时长

    // 托盘通知
    m_notificationMgr->showWakeNotification(currentTime.toString("HH:mm"));

    // 现在在on_btn_save_report_clicked函数的函数体，原本就在这个地方
    MainWindow::on_btn_save_report_clicked();
}

//手动修改入睡和起床时间的按钮：btn_edit_sleep和btn_edit_wake，调用了QDialog
// 修改入睡时间的专属 QDialog
void MainWindow::on_btn_edit_sleep_clicked()
{
    // 1. 动态创建一个子窗口
    QDialog dialog(this);
    dialog.setWindowTitle("⏰ 修改入睡时间");
    dialog.setFixedSize(250, 150); // 设置一个优雅的弹窗大小

    // 2. 准备布局和时间编辑器
    QVBoxLayout layout(&dialog);
    QTimeEdit timeEdit(&dialog);
    timeEdit.setDisplayFormat("HH:mm"); // 只显示小时和分钟
    timeEdit.setFont(QFont("Microsoft YaHei", 16, QFont::Bold)); // 字体调大点更好看

    // 3. 数据回填：如果本来有时间，就让拨轮停在那个时间上
    QString currentText = ui->lineEdit_sleep_disp->text();
    if (currentText != "通宵" && currentText != "未记录" && !currentText.isEmpty()) {
        timeEdit.setTime(QTime::fromString(currentText, "HH:mm"));
    } else {
        timeEdit.setTime(QTime::currentTime()); // 否则默认显示当前时间
    }
    layout.addWidget(&timeEdit);

    // 4. 加上确认和取消按钮
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addWidget(&buttonBox);

    // 将按钮与窗口的确认/关闭动作连接起来
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 5. 阻塞程序并显示弹窗，如果用户点击了 Ok
    if (dialog.exec() == QDialog::Accepted) {
        // 把修改好的时间写回主界面的显示框
        ui->lineEdit_sleep_disp->setText(timeEdit.time().toString("HH:mm"));
        // 立刻联动刷新睡眠时长
        updateDurationDisplay();
    }
}

// 🎓 修改起床时间的专属 QDialog（逻辑完全一致）
void MainWindow::on_btn_edit_wake_clicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle("⏰ 修改起床时间");
    dialog.setFixedSize(250, 150);

    QVBoxLayout layout(&dialog);
    QTimeEdit timeEdit(&dialog);
    timeEdit.setDisplayFormat("HH:mm");
    timeEdit.setFont(QFont("Microsoft YaHei", 16, QFont::Bold));

    QString currentText = ui->lineEdit_wake_disp->text();
    if (currentText != "通宵" && currentText != "未记录" && !currentText.isEmpty()) {
        timeEdit.setTime(QTime::fromString(currentText, "HH:mm"));
    } else {
        timeEdit.setTime(QTime::currentTime());
    }
    layout.addWidget(&timeEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addWidget(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        ui->lineEdit_wake_disp->setText(timeEdit.time().toString("HH:mm"));
        updateDurationDisplay(); // 联动刷新
    }
}

// ==========================================
// 辅助函数：封装了算分、本地持久化、日历染色和弹窗生成的全部流程
void MainWindow::save_and_report(QDate recordDay, int s_hour, int s_min, int w_hour, int w_min, int nap, int exe, int sit)
{
    // 4. 纽转到纯 C++ 算法领域：实例化你队友写的 SleepAnalyzer 对象，并将刚刚从界面上搜集来的 7 个变量传给它
    SleepAnalyzer todayData(s_hour, s_min, w_hour, w_min, nap, exe, sit);
    // 5. 调用算法，计算并返回该作息日的睡眠健康评分（似乎对于周报并无必要）
    int score = todayData.getEnoughSleepScore();

    string title = "";
    // 6. 根据该作息日睡眠时间计算具体数值，以及是否熬夜、是否睡懒觉。匹配对应的修仙称号。评判标准同评分的标准
    int nightsleep = todayData.calculateNightSleep();
    bool noNightSleep = todayData.noNightSleep;
    bool overSleep = todayData.oversleep;

    if (noNightSleep) title = "你见过凌晨五点的学一吗";// 熬穿
    else if(overSleep) title = "你醒啦，期末已经考完了"; //睡懒觉，后面再评价：如果睡眠分小于一定数目，睡懒觉有加分（补觉）；否则不加分
    else if(nightsleep<360) title = "这里是第几层梦境？"; //小于6h
    else if(nightsleep<420) title = "醒了吗？没醒的话，吃我一拳"; //6-7h
    else if(nightsleep<480) title = "如此理想"; //7-8h
    else if(nightsleep<=540) title = "获得睡眠硕士学位"; //8-9h
    else if(nightsleep<=600) title = "Ph.S学位"; //9-10h
    else title = "小~懒~猫 ❤ 姐姐喊你学高代啦"; //大于10h且不算睡懒觉；睡太长也不好

    /* 暂时不考虑的做法
    // 7. 从界面左侧的日历控件（calendarWidget）中，捕获用户当前鼠标选中的那个日期
    QDate selectedDate = ui->calendarWidget->selectedDate();
    // 8. 将获取到的日期对象转换成符合中国人阅读习惯的“XXXX年XX月XX日”字符串格式
    // 具体实现比较复杂，toString会启动一个类似状态机或字符串替换器的机制
    // 执行流程：扫描字符串并识别暗号（yyyy，MM，dd），保留普通字符并替换暗号，最后输出
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");
    */

    //在通过起床按钮生成日报时，不需要考虑鼠标选的哪个日期。后期想要补上哪天的数据的话，加上手动重设按钮，手动编辑入睡和起床时间框，
    //考虑加一个生成日报+保存数据的辅助函数。当点击醒来按钮时，会自动调用生成日报；
    //假如是手动选择了其他日期，手动编辑入睡和起床时间的话，需要一个手动生成日报按钮
    //如果一天完全没有动程序，那么也不会存下数据文件，当天就没有数据文件，直到手动编辑当日入睡和起床时间并手动生成日报
    QString dateStr = recordDay.toString("yyyy年MM月dd日");
    // 获取QString格式的睡眠时长（“x小时x分钟”）
    QString durationText = todayData.ShowSleepTime(nightsleep);

    //生成JSON字符串
    std::string dateStdStr = recordDay.toString("yyyy-MM-dd").toStdString();
    std::string jsonPayload = SleepJsonExporter::toJsonString(todayData, dateStdStr);
    //存本地，修正了存储路径
    //新增260530 11：00 改写了路径，防止windows用户名是中文无法访问
    QString fullPath = dataDir() + "/" + recordDay.toString("yyyy-MM-dd") + ".json";
    QFile outFile(fullPath);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outFile.write(QString::fromStdString(jsonPayload).toUtf8());
        outFile.close();
    } else {
        QMessageBox::warning(this, "保存失败", "无法写入文件：\n" + fullPath);  // 以后再失败会有提示，不再静默
    }

    refreshCalendarColors();  // 刚保存的作息日立刻染色
    checkAndShowAchievements();  // 检查是否达成里程碑
    updateAchievementDisplay(); // 实时联动成就的显示

    // 9. 使用 QString 强大的字符串格式化功能（.arg()），动态把日期、分数、称号拼装成一封完整的报告文本
    // 函数中的 %1 会被 dateStr 替换，以此类推
    QString finalReport = QString("【%1 清晨日结报告】\n\n数据录入成功！\n累计睡眠时长：%2\n判定您的睡眠评分为： %3 分\n\n授予称号：\n%4")
                              .arg(dateStr, durationText, QString::number(score), QString::fromStdString(title));

    // 10. 弹出一个最终的日结报告窗口，将拼装好的内容展示给用户看
    QMessageBox::information(this, "早安，打工人", finalReport);
}

// 点击“保存并生成简报”触发该按钮。假如没睡觉，没有按我起床了或者
void MainWindow::on_btn_save_report_clicked()
{
    // 不用玩那么多花的，其功能和我起床了按钮没有任何区别
    // 好吧，还是有点区别，手动编辑后不能把当前时间扒下来放进起床时间里
    // 何不换个思路，在btn_wake_clicked里，起床时间更新之后，调用这个函数呢
    
    // 2. 从显示框里直接拿到要展示在弹窗里的字符串（比如 "08:30" 或 "通宵"）
    QString s_text = ui->lineEdit_sleep_disp->text();
    QString w_text = ui->lineEdit_wake_disp->text();

    //起床时获得的“白天数据”是昨天（也就是确定的作息日）的数据
    int nap = ui->spinBox_nap->value();           // 从界面上的午睡微调框中获取用户输入的午睡分钟数 
    int exe = ui->spinBox_exercise->value();      // 从界面上的运动微调框中获取用户输入的运动分钟数
    int sit = ui->spinBox_sit->value();           // 从界面上的久坐微调框中获取用户输入的久坐分钟数

    // ==========================================
    // 新增：防呆与容错机制
    /*
    // 组装带前导零的时间字符串（比如把 8:5 变成 08:05），看起来更专业
    QString s_time_str = QString("%1:%2").arg(s_hour, 2, 10, QChar('0')).arg(s_min, 2, 10, QChar('0'));
    QString w_time_str = QString("%1:%2").arg(w_hour, 2, 10, QChar('0')).arg(w_min, 2, 10, QChar('0'));
    */

    // 弹出一个带自定义按钮的确认对话框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("作息结算确认");
    msgBox.setText(QString("系统读取到您的作息如下：\n\n🛌 入睡：%1\n🌅 起床：%2\n\n请确认时间是否准确？\n（若昨晚忘记点【准备入睡】，上方入睡时间可能不准）")
                       .arg(s_text, w_text));
    msgBox.setIcon(QMessageBox::Question);

    // 添加三个自定义按钮，供用户选择，有两个需要记录值
    // 不让通宵按钮和睡了觉的按钮同时出现
    QPushButton *btnAllNight = nullptr;
    //考虑了用户通宵的所有情况，不管点没点手动设置通宵，都能弹出这个按钮
    if (s_text == "通宵" || w_text == "通宵" || s_text == "未记录" || w_text == "未记录"){
        btnAllNight = msgBox.addButton("🔥 我通宵了，一分钟没睡", QMessageBox::ActionRole);
    }
    else{
        msgBox.addButton("✅ 睡眠时间准确，直接结算", QMessageBox::AcceptRole);
    }
    QPushButton *btnCancel = msgBox.addButton("❌ 不准，我去手动改改时间或者设定通宵", QMessageBox::RejectRole);

    msgBox.exec(); // 阻塞程序，等待用户在弹窗上做出点击选择

    // 解析阶段：准备传给底层算法的变量
    int s_hour = -1, s_min = -1, w_hour = -1, w_min = -1;

    if (msgBox.clickedButton() == btnCancel) {
        // 用户发现时间不对，点击了取消
        return; // 直接 return 强行终止这个函数，不往下算分了，让用户回主界面手动调时间
    }
    // 如果点了通宵按钮
    else if (msgBox.clickedButton() == btnAllNight) {
        // 用户通宵了，强行把时间全部设为 -1
        // 会触发你们 sleep_core.h 里构造函数的变量设置：noNightSleep = true
        ui->lineEdit_sleep_disp->setText("通宵");
        ui->lineEdit_wake_disp->setText("通宵");
        updateDurationDisplay();
        // 通宵时算法接收全是 -1 的变量，这里无需处理，因为上面声明的就是 -1
    }
    else{
        // 正常结算：把刚才读到的文本转换回具体的时、分数字
        if (s_text != "通宵" && s_text != "未记录" && !s_text.isEmpty()) {
            QTime s_t = QTime::fromString(s_text, "HH:mm");
            s_hour = s_t.hour(); s_min = s_t.minute();
        }
        if (w_text != "通宵" && w_text != "未记录" && !w_text.isEmpty()) {
            QTime w_t = QTime::fromString(w_text, "HH:mm");
            w_hour = w_t.hour(); w_min = w_t.minute();
        }
    }

    //根据起床时间计算作息日，一般来讲是起床时的前一天；
    QDate recordDay = QDate::currentDate().addDays(-1);
    //极特殊情况当晚睡当晚起，回到当天
    if (s_hour > 12 && w_hour > s_hour)
        recordDay = recordDay.addDays(1);
    QDate selectedDate = ui->calendarWidget->selectedDate();
    // 默认情况下日历选中的是当前日期。如果当前日期（也就是作息日的下一天）不等于日历日期，说明没有在记录今天的睡眠，而是选中了日历上别的日期，在记录特定日期的睡眠
    // 这样的情况下作息日就是被选中的日期
    // 还得让非当日时，入睡和起床按钮不能点击
    if (selectedDate != recordDay.addDays(1))
        recordDay = selectedDate;

    // 6. 发射！
    save_and_report(recordDay, s_hour, s_min, w_hour, w_min, nap, exe, sit);
}

// ==========================================
// 清理功能 1：清空当前日历上选中的那一天
// ==========================================
void MainWindow::on_action_clear_today_triggered()
{
    QDate selectedDate = ui->calendarWidget->selectedDate();
    QString filePath = dataDir() + "/" + selectedDate.toString("yyyy-MM-dd") + ".json";

    // 删除前确认
    auto reply = QMessageBox::question(this, "确认清理",
                                       QString("确定要让 %1 的作息记录灰飞烟灭吗？\n(此操作不可逆！)").arg(selectedDate.toString("yyyy-MM-dd")),
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QFile file(filePath);
        if (file.exists()) {
            file.remove();
            refreshCalendarColors();   // 刷新日历，去掉颜色
            updateAchievementDisplay(); // 实时联动成就的显示
            onCalendarDateSelected();  // 触发一次日期点击，让右侧的显示框全变回“未记录”
            QMessageBox::information(this, "成功", "该日数据已清除。");
        } else {
            QMessageBox::information(this, "提示", "这一天本来就没有记录数据哦。");
        }
    }
}

// ==========================================
// 清理功能 2：清空指定区间
// ==========================================
void MainWindow::on_action_clear_range_triggered()
{
    // 动态造一个选择日期的子窗口
    QDialog dialog(this);
    dialog.setWindowTitle("📅 选择要清空的区间");
    dialog.setFixedSize(300, 150);

    QFormLayout layout(&dialog);
    // 默认起始时间设为7天前，结束时间设为今天
    QDateEdit *startDateEdit = new QDateEdit(QDate::currentDate().addDays(-7), &dialog);
    QDateEdit *endDateEdit = new QDateEdit(QDate::currentDate(), &dialog);
    startDateEdit->setCalendarPopup(true); // 允许弹出小日历选日期
    endDateEdit->setCalendarPopup(true);

    layout.addRow("开始日期:", startDateEdit);
    layout.addRow("结束日期:", endDateEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout.addRow(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QDate start = startDateEdit->date();
        QDate end = endDateEdit->date();

        // 如果用户选反了，调换过来
        if (start > end) std::swap(start, end);

        auto reply = QMessageBox::question(this, "确认清理",
                                           QString("确定要清空 %1 到 %2 之间的所有数据吗？").arg(start.toString("yyyy-MM-dd"), end.toString("yyyy-MM-dd")),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            int deletedCount = 0;
            QDate curr = start;
            // 循环遍历这几天，删文件
            while (curr <= end) {
                QString path = dataDir() + "/" + curr.toString("yyyy-MM-dd") + ".json";
                if (QFile::remove(path)) {
                    deletedCount++;
                }
                curr = curr.addDays(1);
            }
            refreshCalendarColors();
            updateAchievementDisplay(); // 实时联动成就的显示
            onCalendarDateSelected();
            QMessageBox::information(this, "成功", QString("清理完成，共删除了 %1 天的数据。").arg(deletedCount));
        }
    }
}

// ==========================================
// 清理功能 3：清空全部数据
// ==========================================
void MainWindow::on_action_clear_all_triggered()
{
    auto reply = QMessageBox::critical(this, "🚨",
                                       "确定要清空【所有】作息数据吗？\n你的所有修仙记录将彻底归零！",
                                       QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QDir dir(dataDir());
        // 扫描目录下所有的 json 文件
        QStringList files = dir.entryList({"*.json"}, QDir::Files);
        for (const QString &fileName : std::as_const(files)) {
            dir.remove(fileName); // 逐个击破
        }

        refreshCalendarColors();
        updateAchievementDisplay(); // 实时联动成就的显示
        onCalendarDateSelected();
        QMessageBox::information(this, "重生", "功德簿已烧毁，请大侠重新来过！");
    }
}

void MainWindow::on_btn_week_report_clicked()
{
    // 1. 获取并校验用户输入的 API Key
    QString apiKey = ui->lineEdit_apiKey->text().trimmed();

    if (apiKey.isEmpty()) {
        ui->textBrowser_ai->setText("未检测到 API Key，将本地生成周报...");
    } else {
        ui->textBrowser_ai->setText("检测到 API Key，将请掌律大长老出关...");
    }

    // 从界面的日历控件中，抓取用户当前选中的那天，赋值给 targetDate
    QDate targetDate = ui->calendarWidget->selectedDate();

    QString allJsonData = "以下是本弟子最近七天的真实作息 JSON 数据：\n";
    int foundFiles = 0;

    // 实例化本地的核心算法跟踪器
    WeeklyTracker localTracker;

    // 1. 尝试读取近 7 天的文件（包含选中的当天，共向前推7天）
    for (int i = 0; i < 7; ++i) {
        QDate d = targetDate.addDays(-i);
        QString fileName = dataDir() + "/" + d.toString("yyyy-MM-dd") + ".json";
        QFile file(fileName);//这两行进行了修改，改了读取路径，和上面一致

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString jsonContent = file.readAll();
            allJsonData += QString("【日期 %1】:\n%2\n").arg(d.toString("yyyy-MM-dd"), jsonContent);

            // 无论走哪条路，先把本地 JSON 拆解，喂给本地的Weektracker类
            QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8());
            if (!doc.isObject()) continue; // 跳过损坏的 JSON 文件
            QJsonObject obj = doc.object();

            // 根据你截图中序列化的键名，精准提取数据
            int s_h = obj["sleep_hour"].toInt();
            int s_m = obj["sleep_min"].toInt();
            int w_h = obj["wake_hour"].toInt();
            int w_m = obj["wake_min"].toInt();
            int nap = obj["day_sleep_min"].toInt();
            int exe = obj["exercise_min"].toInt();
            int sit = obj["sit_min"].toInt();

            //构建SleepAnalyzer对象并传入Weektracker类对象localTracker
            SleepAnalyzer sa(s_h, s_m, w_h, w_m, nap, exe, sit);
            localTracker.addDay(sa);

            file.close();
            foundFiles++;
        }
    }

    // 2. 如果七天连一天的数据都没找到
    if (foundFiles == 0) {
        ui->textBrowser_ai->setText("未能在本地寻得近七天的作息 .json 存档，请先打卡！");
        return;
    }

    // ==========================================
    // 3. 路线分发：本地分析 vs AI 分析
    // ==========================================
    if (apiKey.isEmpty()) {
        // 【路线 A：无 API Key，调用本地 C++ 算法】
        ui->textBrowser_ai->setText("未检测到 API Key，正在调用本地核心引擎分析...\n\n");

        // 直接获取刚刚解析并塞进 tracker 的周报，追加显示在界面上
        QString localResult = localTracker.generateLocalReport();
        ui->textBrowser_ai->append(localResult);

    } else {
        // 校验 API Key 格式：DeepSeek Key 以 "sk-" 开头
        if (!apiKey.startsWith("sk-") || apiKey.length() < 10) {
            ui->textBrowser_ai->setText("API Key 格式似乎不正确，请检查是否以 sk- 开头且长度足够。\n已降级为本地周报。");
            // 降级到本地模式
            QString localResult = localTracker.generateLocalReport();
            ui->textBrowser_ai->append(localResult);
            return;
        }
        // 【路线 B：有 API Key，走大模型网络请求】
        ui->textBrowser_ai->setText("检测到 API Key，正在请大长老出关批阅一周玉简，请稍候...");

        // 按照使用的大模型 API 的要求，组装 JSON 数据包
        QJsonObject requestBody;
        requestBody["model"] = "deepseek-chat";
        // 温度，可以控制大模型回复的质量
        requestBody["temperature"] = 0.2;

        // 下面的代码是在组装 "聊天记录" (messages)。大模型需要知道聊天的上下文。
        // QJsonArray 相当于 JSON 里的中括号 []，用来存多条消息。
        QJsonArray messagesArray;
        // 组装第一条消息：系统指令 (System Prompt)
        QJsonObject systemMessage; // 创建一个 JSON 对象（小盒子），用来装系统指令
        // "role" 代表说话人的身份。"system" 告诉大模型这是最高级别的后台规则
        systemMessage["role"] = "system";
        // 注意：这里的提示词改成了“近七天”
        systemMessage["content"] = "你是一位高冷毒舌的‘修仙宗门掌律大长老’。你需要审视弟子递交上来的近七天作息 JSON 数据。用严厉、阴阳怪气、充满修仙色彩的口吻，给出一份排版完美的 Markdown 格式周报。必须包含：### 📜 掌律周评、### 💀 陨落风险评估、### 💊 下周渡劫仙方。";
        // 把这条系统指令塞进我们的“聊天记录”数组里
        messagesArray.append(systemMessage);

        // 组装第二条消息：用户的真实数据 (User Message)
        QJsonObject userMessage; // 再次创建一个 JSON 对象
        // "role" 设为 "user"，告诉大模型：这是真实用户发给你的内容，请根据上面的系统规则来处理它。
        userMessage["role"] = "user";
        // 把我们之前从本地 .json 文件里捞出来的全部长篇字符串塞进去
        userMessage["content"] = allJsonData;
        // 同样，把这条用户消息也塞进“聊天记录”数组里。
        // 现在，数组里有两条记录了：[ {系统规则}, {用户数据} ]
        messagesArray.append(userMessage);

        // 最后，把整个聊天记录数组，贴上 "messages" 的标签，放进最外层的大纸箱里。
        // 这样整个 JSON 包就严格符合了大模型 API 的官方标准
        requestBody["messages"] = messagesArray;


        // 下面这块是网络通信部分，相当于去邮局寄这个箱子

        // 填好信封（设置 URL 和 请求头）
        QUrl url("https://api.deepseek.com/chat/completions");
        QNetworkRequest request(url);
        // 在快递单上备注 Content-Type：告诉服务器"我发过去的数据格式是 JSON，请用 JSON 解析它"
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        // 在快递单上备注 Authorization：这是你的通行证。
        // QString("Bearer %1").arg(apiKey) 会把你的 Key 拼成 "Bearer sk-xxxx" 的标准格式。
        // .toUtf8() 是因为网络传输底层只认字节流（Byte Array），所以要把字符串转成 UTF-8 的字节流。
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
        // 设置网络超时 15 秒，避免请求卡死
        request.setTransferTimeout(15000);

        // 邮递员出发！收到回复后依然会走之前的 on_api_reply_finished 槽函数
        // QJsonDocument(requestBody).toJson() 的作用是：
        // 拔掉套娃，把我们在内存里用 QJsonObject 拼好的结构，压扁成一长串真实的纯文本 JSON 字符串，然后发送
        networkManager->post(request, QJsonDocument(requestBody).toJson());
    }
    /*
    // 1. 准备你要发给大模型的话（这里你需要把你队友类里的数据转成字符串，这里先用假数据演示）
    QString sleepData = "我最近三天分别睡了 5小时、6小时、4小时，并且连续两天都在凌晨 2 点后入睡。请给我一份刻薄又搞笑的修仙警告报告。";

    // 2. 按照你使用的大模型 API 的要求，组装 JSON 数据包
    QJsonObject requestBody;
    requestBody["model"] = "你使用的大模型名字"; // 例如 "gpt-3.5-turbo" 或 "glm-4"

    QJsonArray messagesArray;
    QJsonObject messageItem;
    messageItem["role"] = "user";
    messageItem["content"] = sleepData;
    messagesArray.append(messageItem);

    requestBody["messages"] = messagesArray;

    QJsonDocument doc(requestBody);
    QByteArray postData = doc.toJson();

    // 3. 填好信封（设置 URL 和 请求头）
    QUrl url("大模型厂商提供的API地址");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // 注意：你需要去大模型官网申请一个 API Key，替换到下面这行
    request.setRawHeader("Authorization", "Bearer 你的_API_KEY");

    // 4. 让邮递员把信发出去（发送 POST 请求）
    networkManager->post(request, postData);
    */
}

void MainWindow::on_api_reply_finished(QNetworkReply *reply)
{
    // 如果网络出错（比如没网）
    if (reply->error() != QNetworkReply::NoError) {
        ui->textBrowser_ai->setText("网络请求失败：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    // 读取所有的返回数据
    QByteArray responseData = reply->readAll();

    // 解析 JSON 找到大模型说的那句话（注意：不同大模型的返回格式略有不同，以下是通用格式）
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    QJsonObject jsonObj = jsonDoc.object();

    QJsonArray choices = jsonObj["choices"].toArray();
    QJsonObject firstChoice = choices[0].toObject();
    QJsonObject message = firstChoice["message"].toObject();
    QString aiText = message["content"].toString();

    // 把大模型的回复显示在界面的文本框里（Markdown 渲染）
    ui->textBrowser_ai->setMarkdown(aiText);

    // 释放内存
    reply->deleteLater();
}

// ==========================================
// 可视化图表按钮：读取近7天数据，弹出图表对话框
// ==========================================
void MainWindow::on_btn_show_chart_clicked()
{
    QDate targetDate = ui->calendarWidget->selectedDate();
    QVector<SleepDayData> weekData;

    // 从最旧到最新依次加载（i=6 最旧 → i=0 今天），
    // 这样 weekData[0] = 上周前, weekData[6] = 今天，图表从左到右即为日期正序
    for (int i = 6; i >= 0; --i) {
        QDate d = targetDate.addDays(-i);
        QString fileName = dataDir() + "/" + d.toString("yyyy-MM-dd") + ".json";
        QFile file(fileName);

        SleepDayData sdd;
        sdd.date = d;

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();

            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                int s_h = obj["sleep_hour"].toInt(-1);
                int w_h = obj["wake_hour"].toInt(-1);

                sdd.nightSleepMin = obj["night_sleep_min"].toInt(0);
                sdd.daySleepMin   = obj["day_sleep_min"].toInt(0);
                sdd.score         = obj["sleep_score"].toInt(0);
                sdd.stayUp        = obj["stay_up_late"].toBool(false);
                sdd.noNightSleep  = (s_h == -1 && w_h == -1);
                sdd.oversleep     = obj["oversleep"].toBool(false);
            }
        }
        weekData.append(sdd);
    }

    // 检查是否有数据
    bool hasData = false;
    for (const auto &d : weekData) {
        if (d.nightSleepMin > 0 || d.daySleepMin > 0 || d.noNightSleep) {
            hasData = true;
            break;
        }
    }

    if (!hasData) {
        QMessageBox::information(this, "提示", "近7天暂无数据，请先打卡！");
        return;
    }

    WeekChartDialog dialog(weekData, this);
    dialog.exec();
}

// ==========================================
// 用户设置按钮：打开自定义设置对话框
// ==========================================
void MainWindow::on_btn_settings_clicked()
{
    // 拼接出配置文件的完整存放路径
    QString cfgPath = dataDir() + "/config.json";
    // 调用全局函数去读文件。如果文件在，currentConfig 就是老配置；如果不在，就是默认设置
    QJsonObject currentCfg = loadUserConfig(cfgPath);

    // 【创建界面】把读出来的配置塞给弹窗，弹窗构造函数在后台把数字填进那些数字框里
    SettingsDialog dialog(currentCfg, this);
    if (dialog.exec() == QDialog::Accepted) {
        QJsonObject newCfg = dialog.getConfig();

        // 保存到磁盘
        if (saveUserConfig(cfgPath, newCfg)) {
            // 立即应用到全局变量
            applyUserConfig(newCfg);

            // 【新增工具】：将 double 转为 "HH:mm" 格式的字符串的lambda表达式
            auto formatTime = [](double val) {
                int h = static_cast<int>(val);
                int m = (val - h) > 0 ? 30 : 0;
                // 用 0 补齐两位数，例如把 "8:0" 变成 "08:00"
                return QString("%1:%2").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0'));
            };

            // 💡 替换弹窗逻辑，去掉写死的 :00，改为调用 toDouble() 和 formatTime 工具
            QMessageBox::information(this, "设置已保存",
                                     QString("偏好设置已更新！\n\n"
                                             "一般入睡时间：%1\n"
                                             "一般起床时间：%2\n"
                                             "熬夜判定区间：%3 ~ %4\n\n"
                                             "之后的打卡将按新标准评估。")
                                         .arg(formatTime(newCfg["general_sleep_hour"].toDouble()))
                                         .arg(formatTime(newCfg["general_wake_hour"].toDouble()))
                                         .arg(formatTime(newCfg["stayup_begin"].toDouble()))
                                         .arg(formatTime(newCfg["stayup_end"].toDouble())));
        } else {
            QMessageBox::warning(this, "保存失败", "无法写入配置文件，请检查磁盘权限。");
        }

        refreshCalendarColors();
    }
}

// ==========================================
// ⌨️ 键盘活跃度分析按钮
// ==========================================
void MainWindow::on_btn_keyboard_clicked()
{
    if (!m_keyMonitor) return;

    KeyMonitorDialog dialog(m_keyMonitor, this);
    dialog.exec();
}

// ==========================================
// 📄 导出 AI 分析结果为 PDF 文件
// ==========================================
void MainWindow::on_btn_export_pdf_clicked()
{
    QString html = ui->textBrowser_ai->toHtml();
    // 如果内容为空或者是默认的占位文字，提示用户
    if (html.isEmpty() || html.contains("暂无数据") ||
        ui->textBrowser_ai->toPlainText().trimmed().isEmpty()) {
        QMessageBox::information(this, "提示", "AI 分析区域为空，请先生成周报后再导出。");
        return;
    }

    // 弹出文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(this, "导出 PDF",
        QDir::homePath() + "/睡眠分析报告.pdf", "PDF 文件 (*.pdf)");
    if (fileName.isEmpty()) return;

    // 用 QTextDocument 承载 HTML，通过 QPrinter 输出为 PDF
    QTextDocument doc;
    doc.setHtml(html);
    // 设置合适的页面边距
    doc.setDocumentMargin(20);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    doc.print(&printer);

    QMessageBox::information(this, "导出成功",
                             QString("PDF 已保存至：\n%1").arg(fileName));
}