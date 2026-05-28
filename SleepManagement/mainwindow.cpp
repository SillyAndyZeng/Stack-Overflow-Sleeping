#include "mainwindow.h" // 引入对应的头文件声明
#include "ui_mainwindow.h"  //引入由 mainwindow.ui 编译生成的底层界面头文件，不引它就无法通过 ui-> 指针访问控件
#include "achievement_manager.h"
#include "sleep_core.h"       // 算分引擎
#include <QMessageBox>        // 引入 Qt 官方的弹窗对话框类，用于实现各种警告、信息提示弹窗
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>  // 引入时间编辑器控件的类声明
#include <QSpinBox>         // 引入数字微调框控件的类声明
#include <QCalendarWidget>  // 引入日历控件的类声明
#include <QFile>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
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
    connect(ui->calendarWidget, &QCalendarWidget::selectionChanged,
        this, &MainWindow::onCalendarDateSelected);//构造函数绑定日历信号
    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // connect函数：当收到大模型回信时，自动把信（reply）交给 on_api_reply_finished 函数去拆解
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::on_api_reply_finished);

    // 【初始化时】把显示框设为只读，不允许键盘修改
    ui->lineEdit_sleep_disp->setReadOnly(true);
    ui->lineEdit_wake_disp->setReadOnly(true);

    // 默认隐藏历史修改按钮（只有点选过去的日子才展示）
    ui->btn_edit_sleep->setVisible(false);
    ui->btn_edit_wake->setVisible(false);
    // 默认隐藏手动保存按钮
    ui->btn_save_report->setVisible(false);
}
MainWindow::~MainWindow()
{
    delete ui; //析构函数的具体实现：回收在构造函数中 new 出来的 ui 指针占用的内存，防止内存泄漏
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
        // no_night_sleep / oversleep 是你即将补充的布尔字段，
        // 若文件里尚未存在，toBool(false) 会安全返回 false，不影响现有记录
        bool noNightSleep = obj["no_night_sleep"].toBool(false);
        bool stayUp       = obj["stay_up_late"].toBool(false);
        bool oversleep    = obj["oversleep"].toBool(false);
        int  sit          = obj["sit_min"].toInt(0);
        int  score        = obj["sleep_score"].toInt(0);

        QTextCharFormat fmt;
        fmt.setFontWeight(QFont::Bold);

        // 使用十六进制数表示RGB颜色
        if (noNightSleep)  fmt.setBackground(QColor("0xFF6B6B")); // 红：熬穿
        else if (stayUp)        fmt.setBackground(QColor("0xFFD93D")); // 黄：熬夜
        else if (oversleep)     fmt.setBackground(QColor("0xC8A2C8")); // 紫：睡懒觉
        else if (sit > 360)     fmt.setBackground(QColor("0xFFB347")); // 橙：久坐超标
        else if (score >= 3)    fmt.setBackground(QColor("0x6BCB77")); // 绿：睡眠良好
        else                    fmt.setBackground(QColor("0x4D96FF")); // 蓝：正常

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

    QMessageBox::information(this, "🎖 成就解锁！",
                             QString("恭喜解锁新成就：\n\n%1\n\n连续打卡 %2 天 · 连续早睡 %3 天\n\n继续保持！")
                                 .arg(badge).arg(ci).arg(es));
}

// 🎓【新增辅助函数】动态读取界面上的时间，计算时长并显示
void MainWindow::updateDurationDisplay()
{
    QString s_text = ui->lineEdit_sleep_disp->text();
    QString w_text = ui->lineEdit_wake_disp->text();

    // 1. 如果还没记录，或者通宵了
    if (s_text == "未记录" || w_text == "未记录" || s_text.isEmpty() || w_text.isEmpty()) {
        ui->lineEdit_duration_disp->setText("--:--");
        return;
    }
    if (s_text == "修仙" || w_text == "修仙") {
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
        ui->lineEdit_sleep_disp->setText("修仙");
        ui->lineEdit_wake_disp->setText("修仙");
    } else {
        ui->lineEdit_sleep_disp->setText(QString("%1:%2").arg(s_h, 2, 10, QChar('0')).arg(s_m, 2, 10, QChar('0')));
        ui->lineEdit_wake_disp->setText(QString("%1:%2").arg(w_h, 2, 10, QChar('0')).arg(w_m, 2, 10, QChar('0')));
    }
    ui->spinBox_nap->setValue(obj["day_sleep_min"].toInt()); //[cite: 43]
    ui->spinBox_exercise->setValue(obj["exercise_min"].toInt()); //[cite: 43]
    ui->spinBox_sit->setValue(obj["sit_min"].toInt()); //[cite: 43]

    // 刷新晚间睡眠时长显示！
    updateDurationDisplay();
}

// ==========================================
// 动作 1：用户点击【准备入睡】按钮，通过信号-槽机制触发此函数
// ==========================================
void MainWindow::on_btn_sleep_clicked()
{
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

    // 3. 弹出一个温馨的提示框。参数含义：（父窗口为当前窗口, 弹窗标题, 弹窗正文文本）
    QMessageBox::information(this, "晚安守护", "已记录入睡时间！\n系统已进入静默模式，请放下手机，好好休息哦~");
}

// ==========================================
// 动作 2：用户点击【我醒了】按钮，进行数据结算
// ==========================================
void MainWindow::on_btn_wake_clicked()
{
    // 1. 获取当前系统那一瞬间的起床时间
    QTime currentTime = QTime::currentTime();
    //捞取时间填写到lineEdit_wake_disp文本框中
    ui->lineEdit_wake_disp->setText(currentTime.toString("HH:mm"));
    updateDurationDisplay(); // 刷新时长

    /*
    // 3. 开始通过 ui-> 指针，从界面上的各个控件中捞取用户输入的数据：
    // 增加容错机制：假如没有点“我睡了”，或者没有手动填写入睡时间，此时timeEdit_sleep框为空（或者保留的是昨天的入睡时间），可能报错。
    int s_hour = ui->timeEdit_sleep->time().hour();
    int s_min  = ui->timeEdit_sleep->time().minute();
    int w_hour = ui->timeEdit_wake->time().hour();
    int w_min  = ui->timeEdit_wake->time().minute();
    */
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

    // 添加三个自定义按钮，供用户选择，后两个需要记录值
    msgBox.addButton("✅ 睡眠时间准确，直接结算", QMessageBox::AcceptRole);
    QPushButton *btnAllNight = msgBox.addButton("🔥 我通宵了，一分钟没睡", QMessageBox::ActionRole);
    QPushButton *btnCancel = msgBox.addButton("❌ 不准，我去手动改改时间", QMessageBox::RejectRole);

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

    //根据入睡时间计算作息日，一般来讲是起床时的前一天；
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
    // 6. 根据该作息日睡眠时间计算具体数值，以及是否睡懒觉。匹配对应的修仙称号。评判标准同评分的标准
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
    std::string dateStdStr = recordDay.toString("yyyy-MM-dd").toStdString();std::string jsonPayload = SleepJsonExporter::toJsonString(todayData, dateStdStr);
    //存本地，修正了存储路径
    std::string fullPath = (dataDir() + "/" + QString::fromStdString(dateStdStr) + ".json").toStdString();
    SleepJsonExporter::saveToFile(jsonPayload, fullPath);

    refreshCalendarColors();  // 刚保存的作息日立刻染色
    checkAndShowAchievements();  // 检查是否达成里程碑

    // 9. 使用 QString 强大的字符串格式化功能（.arg()），动态把日期、分数、称号拼装成一封完整的报告文本
    // 函数中的 %1 会被 dateStr 替换，以此类推
    QString finalReport = QString("【%1 清晨日结报告】\n\n数据录入成功！\n累计睡眠时长：%2\n判定您的睡眠评分为： %3 分\n\n授予称号：\n%4")
                              .arg(dateStr, durationText, QString::number(score), title);

    // 10. 弹出一个最终的日结报告窗口，将拼装好的内容展示给用户看
    QMessageBox::information(this, "早安，打工人", finalReport);
}

// 非今日，该按钮才会出现；点击“保存并生成简报”触发该按钮
void MainWindow::on_btn_save_report_clicked()
{
    // 1. 获取当前日历上选中的那一天作为归属作息日
    QDate recordDay = ui->calendarWidget->selectedDate();

    // 2. 准备底层需要的变量，默认全为 -1（应对通宵情况）
    int s_hour = -1, s_min = -1, w_hour = -1, w_min = -1;

    QString s_text = ui->lineEdit_sleep_disp->text();
    QString w_text = ui->lineEdit_wake_disp->text();

    // 增加容错拦截：如果没写完整时间，不让生成报告
    if ((s_text == "未记录" || s_text.isEmpty()) || (w_text == "未记录" || w_text.isEmpty())) {
        QMessageBox::warning(this, "提示", "数据未填写完整，无法生成报告！\n请先点击上方的修改按钮补齐入睡和起床时间。");
        return;
    }

    // 3. 将界面上的纯文本解析回时、分数字
    if (s_text != "通宵") {
        QTime s_t = QTime::fromString(s_text, "HH:mm");
        s_hour = s_t.hour();
        s_min = s_t.minute();
    }
    if (w_text != "通宵") {
        QTime w_t = QTime::fromString(w_text, "HH:mm");
        w_hour = w_t.hour();
        w_min = w_t.minute();
    }

    // 4. 读取白天的微调框数据
    int nap = ui->spinBox_nap->value();
    int exe = ui->spinBox_exercise->value();
    int sit = ui->spinBox_sit->value();

    // 5. 呼叫我们之前封装好的辅助函数，带走所有保存、刷新、弹窗逻辑
    save_and_report(recordDay, s_hour, s_min, w_hour, w_min, nap, exe, sit);
}

void MainWindow::on_btn_week_report_clicked()
{
    // 1. 获取并校验用户输入的 API Key
    QString apiKey = ui->lineEdit_apiKey->text().trimmed();

    QMessageBox::warning(this, "天机不可泄露", "如果你有，请先在输入框中填写您的 DeepSeek API Key；如果填写了，会调用API生成周报；如果没有填写，公堂自己给你周报");

    if (apiKey.isEmpty()) {
        ui->textBrowser_ai->setText("未检测到 API Key，将本地生成周报...");
    } else {
        ui->textBrowser_ai->setText("检测到 API Key，将请掌律大长老出关...");
    }

    // 从界面的日历控件中，抓取用户当前选中的那天，赋值给 targetDate
    QDate targetDate = ui->calendarWidget->selectedDate();
    ui->textBrowser_ai->setText("正在翻阅本门过去七天的功德簿，等着...");

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
        // 在快递单上备注 Content-Type：告诉服务器“我发过去的数据格式是 JSON，请用 JSON 解析它”
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        // 在快递单上备注 Authorization：这是你的通行证。
        // QString("Bearer %1").arg(apiKey) 会把你的 Key 拼成 "Bearer sk-xxxx" 的标准格式。
        // .toUtf8() 是因为网络传输底层只认字节流（Byte Array），所以要把字符串转成 UTF-8 的字节流。
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

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

    // 把大模型的回复显示在界面的文本框里
    ui->textBrowser_ai->setText(aiText);

    // 释放内存
    reply->deleteLater();
}

