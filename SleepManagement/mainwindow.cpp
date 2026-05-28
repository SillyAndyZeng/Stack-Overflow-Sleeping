#include "mainwindow.h" // 引入对应的头文件声明
#include "ui_mainwindow.h"  //引入由 mainwindow.ui 编译生成的底层界面头文件，不引它就无法通过 ui-> 指针访问控件
#include "achievement_manager.h"
#include "sleep_core.h"       // 算分引擎
#include <QMessageBox>        // 引入 Qt 官方的弹窗对话框类，用于实现各种警告、信息提示弹窗
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>            // 引入时间编辑器控件的类声明
#include <QSpinBox>         // 引入数字微调框控件的类声明
#include <QCalendarWidget>  // 引入日历控件的类声明
#include <QFile>
#include <QDir>
/*
当界面上的按钮被点击时，具体要做什么计算、弹出什么提示，全部写在这里。
什么是 ui->？
 ui是由 .ui 文件编译生成的界面类对象。
 界面上所有的组件（例如 calendarWidget 、timeEdit_sleep 、spinBox_nap  等）都挂在 ui 指针下面。在 .cpp 里输入 ui-> 就能调出它们。
什么是 .value() 和 .time()？
 不同控件取值的方式不同：QSpinBox（数字微调框）用 .value() 拿数字 ；QTimeEdit（时间组件）用 .time().hour() 拿小时数 。
数据的流转闭环：
 用户在界面（ui->...）上输入数据 -> 你的槽函数通过代码把数据捞出来 -> 喂给队友的纯 C++ 类（SleepAnalyzer）去算分 -> 把算出来的分数包装成弹窗（QMessageBox）反馈渲染给用户。

*/
// 构造函数的具体实现：执行窗口的初始化
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)   // 先调用基类 QMainWindow 的构造函数，完成最基础的通用窗口初始化
    , ui(new Ui::MainWindow) // 动态分配内存，实例化负责管理 UI 控件的界面类
{
    ui->setupUi(this); // 超级核心！这个函数把在图形化界面里拖拽的所有按钮、日历、输入框等，真正地绘制、布置到当前的这个MainWindow窗口上
    connect(ui->calendarWidget, &QCalendarWidget::selectionChanged,
        this, &MainWindow::onCalendarDateSelected);//构造函数绑定日历信号
    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // 当邮递员收到回信时，自动把信（reply）交给 on_api_reply_finished 函数去拆解
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::on_api_reply_finished);
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
void MainWindow::onCalendarDateSelected()
{
    QDate date = ui->calendarWidget->selectedDate();
    QString filePath = dataDir() + "/" + date.toString("yyyy-MM-dd") + ".json";
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;   // 该天没有打卡记录，不做任何操作

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) return;
    QJsonObject obj = doc.object();

    // 把存储的数据回填到界面控件
    ui->timeEdit_sleep->setTime(QTime(obj["sleep_hour"].toInt(), obj["sleep_min"].toInt()));
    ui->timeEdit_wake->setTime(QTime(obj["wake_hour"].toInt(),   obj["wake_min"].toInt()));
    ui->spinBox_nap->setValue(obj["day_sleep_min"].toInt());
    ui->spinBox_exercise->setValue(obj["exercise_min"].toInt());
    ui->spinBox_sit->setValue(obj["sit_min"].toInt());
}

// ==========================================
// 动作 1：用户点击【准备入睡】按钮，通过信号-槽机制触发此函数
// ==========================================
void MainWindow::on_btn_sleep_clicked()
{
    // 1. 获取当前系统的精确时间（时、分、秒）
    QTime currentTime = QTime::currentTime();
    // 2. 将捞到的当前系统时间，自动填写到界面上那个叫"timeEdit_sleep"的时间编辑框中显示
    ui->timeEdit_sleep->setTime(currentTime);
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
    // 2. 将获取到的起床时间，自动填写到界面上名为"timeEdit_wake"的时间框中
    ui->timeEdit_wake->setTime(currentTime);

    // 3. 开始通过 ui-> 指针，从界面上的各个控件中捞取用户输入的数据：
    // 增加容错机制：假如没有点“我睡了”，或者没有手动填写入睡时间，此时timeEdit_sleep框为空（或者保留的是昨天的入睡时间），可能报错。
    int s_hour = ui->timeEdit_sleep->time().hour();
    int s_min  = ui->timeEdit_sleep->time().minute();
    int w_hour = ui->timeEdit_wake->time().hour();
    int w_min  = ui->timeEdit_wake->time().minute();

    int nap = ui->spinBox_nap->value();           // 从界面上的午睡微调框中获取用户输入的午睡分钟数 
    int exe = ui->spinBox_exercise->value();      // 从界面上的运动微调框中获取用户输入的运动分钟数 
    int sit = ui->spinBox_sit->value();           // 从界面上的久坐微调框中获取用户输入的久坐分钟数 

    // 4. 纽转到纯 C++ 算法领域：实例化你队友写的 SleepAnalyzer 对象，并将刚刚从界面上搜集来的 7 个变量传给它
    SleepAnalyzer todayData(s_hour, s_min, w_hour, w_min, nap, exe, sit);
    // 5. 调用算法，计算并返回当天的睡眠健康评分（似乎对于周报并无必要）
    int score = todayData.getEnoughSleepScore();

    string title = "";
    // 6. 根据当天睡眠时间具体数值，匹配对应的修仙称号。评判标准同评分的标准
    int nightsleep = todayData.calculateNightSleep();
    bool noNightSleep = todayData.noNightSleep;
    bool overSleep = todayData.oversleep;
    //总之我觉得后面可以把晚上睡眠和白天睡眠分开考虑，晚上不睡白天昏昏欲睡也不好，但总比不睡好x
    if (noNightSleep) title = "你见过凌晨六点的未名湖吗";// 熬穿
    else if(overSleep) title = "小~懒~猫 ❤ 姐姐喊你学高代啦"; //睡懒觉，后面再评价：如果睡眠分小于一定数目，睡懒觉有加分（补觉）；否则不加分
    else if(nightsleep<360) title = "阿巴阿巴阿巴"; //小于6h
    else if(nightsleep<420) title = ""; //6-7h
    else if(nightsleep<480) title = ""; //7-8h
    else if(nightsleep<=540) title = ""; //8-9h
    else if(nightsleep<=600) title = ""; //9-10h
    else title = ""; //大于10h且不算睡懒觉；睡太长也不好
    

    // 7. 从界面左侧的日历控件（calendarWidget）中，捕获用户当前鼠标选中的那个日期
    QDate selectedDate = ui->calendarWidget->selectedDate();
    // 8. 将获取到的日期对象转换成符合中国人阅读习惯的“XXXX年XX月XX日”字符串格式
    // 具体实现比较复杂，toString会启动一个类似状态机或字符串替换器的机制
    // 执行流程：扫描字符串并识别暗号（yyyy，MM，dd），保留普通字符并替换暗号，最后输出
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");

    // 获取QString格式的睡眠时长（“x小时x分钟”）
    QString durationText = todayData.ShowSleepTime(nightsleep);

    //生成JSON字符串
    std::string dateStdStr = selectedDate.toString("yyyy-MM-dd").toStdString();std::string jsonPayload = SleepJsonExporter::toJsonString(todayData, dateStdStr);
    //存本地，修正了存储路径
    std::string fullPath = (dataDir() + "/" + QString::fromStdString(dateStdStr) + ".json").toStdString();
    SleepJsonExporter::saveToFile(jsonPayload, fullPath);

    refreshCalendarColors();  // 刚保存的今天立刻染色
    checkAndShowAchievements();  // 检查是否达成里程碑

    // 9. 使用 QString 强大的字符串格式化功能（.arg()），动态把日期、分数、称号拼装成一封完整的报告文本
    // 函数中的 %1 会被 dateStr 替换，以此类推
    QString finalReport = QString("【%1 清晨日结报告】\n\n数据录入成功！\n累计睡眠时长：%2\n判定您的睡眠评分为： %3 分\n\n授予称号：\n%4")
                              .arg(dateStr, durationText, QString::number(score), title);

    // 10. 弹出一个最终的日结报告窗口，将拼装好的内容展示给用户看                              
    QMessageBox::information(this, "早安，打工人", finalReport);
}

void MainWindow::on_btn_ai_report_clicked()
{
    // 1. 获取并校验用户输入的 API Key
    QString apiKey = ui->lineEdit_apiKey->text().trimmed();
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "天机不可泄露", "如果你有，请先在输入框中填写您的 DeepSeek API Key；如果填写了，会调用API生成周报；如果没有填写，老夫自己给你周报");
        return;
    }

    // 从界面的日历控件中，抓取用户当前选中的那天，赋值给 targetDate
    QDate targetDate = ui->calendarWidget->selectedDate();
    ui->textBrowser_ai->setText("正在翻阅本门过去七天的功德簿，等着...");

    QString allJsonData = "以下是本弟子最近七天的真实作息 JSON 数据：\n";
    int foundFiles = 0;

    // 实例化你们本地的核心算法跟踪器
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

