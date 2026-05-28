#include "mainwindow.h" // 引入对应的头文件声明
#include "ui_mainwindow.h"  //引入由 mainwindow.ui 编译生成的底层界面头文件，不引它就无法通过 ui-> 指针访问控件
#include "sleep_core.h"       // 队友的算分引擎
#include <QMessageBox>        // 引入 Qt 官方的弹窗对话框类，用于实现各种警告、信息提示弹窗
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>            // 引入时间编辑器控件的类声明
#include <QSpinBox>         // 引入数字微调框控件的类声明
#include <QCalendarWidget>  // 引入日历控件的类声明
#include <QFile>
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

    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // 当邮递员收到回信时，自动把信（reply）交给 on_api_reply_finished 函数去拆解
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::on_api_reply_finished);
}

MainWindow::~MainWindow()
{
    delete ui; //析构函数的具体实现：回收在构造函数中 new 出来的 ui 指针占用的内存，防止内存泄漏
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
    int s_hour = ui->timeEdit_sleep->time().hour();
    int s_min  = ui->timeEdit_sleep->time().minute();
    int w_hour = ui->timeEdit_wake->time().hour();
    int w_min  = ui->timeEdit_wake->time().minute();

    int nap = ui->spinBox_nap->value();           // 从界面上的午睡微调框中获取用户输入的午睡分钟数 
    int exe = ui->spinBox_exercise->value();      // 从界面上的运动微调框中获取用户输入的运动分钟数 
    int sit = ui->spinBox_sit->value();           // 从界面上的久坐微调框中获取用户输入的久坐分钟数 

    // 4. 纽转到纯 C++ 算法领域：实例化你队友写的 SleepAnalyzer 对象，并将刚刚从界面上搜集来的 7 个变量传给它
    SleepAnalyzer todayData(s_hour, s_min, w_hour, w_min, nap, exe, sit);
    // 5. 调用算法，计算并返回当天的睡眠健康评分
    int score = todayData.getEnoughSleepScore();

    // 6. 根据算出来的分数，匹配对应的修仙称号
    QString title = "";
    if(score >= 16) title = "🏆 睡眠大神，请受我一拜！";
    else if(score >= 10 && score <= 15) title = "✨ 健康的清澈大学生";
    else if(score >= 4 && score <= 9) title = "🏃 正在生死时速赶早八";
    else title = "💀 熬夜修仙者！系统警告，请惜命！";

    // 7. 从界面左侧的日历控件（calendarWidget）中，捕获用户当前鼠标选中的那个日期
    QDate selectedDate = ui->calendarWidget->selectedDate();
    // 8. 将获取到的日期对象转换成符合中国人阅读习惯的“XXXX年XX月XX日”字符串格式
    // 具体实现比较复杂，toString会启动一个类似状态机或字符串替换器的机制
    // 执行流程：扫描字符串并识别暗号（yyyy，MM，dd），保留普通字符并替换暗号，最后输出
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");

    // 9. 使用 QString 强大的字符串格式化功能（.arg()），动态把日期、分数、称号拼装成一封完整的报告文本
    // 函数中的 %1 会被 dateStr 替换，%2 会被 score 替换，%3 会被 title 替换
    QString finalReport = QString("【%1 清晨日结报告】\n\n数据录入成功！\n系统判定您的睡眠评分为： %2 分\n\n授予称号：\n%3")
                              .arg(dateStr)
                              .arg(score)
                              .arg(title);

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
        QString fileName = d.toString("yyyy-MM-dd") + ".json";
        QFile file(fileName);

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString jsonContent = file.readAll();
            allJsonData += QString("【日期 %1】:\n%2\n").arg(d.toString("yyyy-MM-dd")).arg(jsonContent);

            // 无论走哪条路，先顺手把本地 JSON 拆解，喂给你们的本地算分引擎
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

        QJsonObject requestBody;
        requestBody["model"] = "deepseek-chat";
        requestBody["temperature"] = 0.2;

        QJsonArray messagesArray;
        QJsonObject systemMessage;
        systemMessage["role"] = "system";
        // 注意：这里的提示词改成了“近七天”
        systemMessage["content"] = "你是一位高冷毒舌的‘修仙宗门掌律大长老’。你需要审视弟子递交上来的近七天作息 JSON 数据。用严厉、阴阳怪气、充满修仙色彩的口吻，给出一份排版完美的 Markdown 格式周报。必须包含：### 📜 掌律周评、### 💀 陨落风险评估、### 💊 下周渡劫仙方。";
        messagesArray.append(systemMessage);

        QJsonObject userMessage;
        userMessage["role"] = "user";
        userMessage["content"] = allJsonData;
        messagesArray.append(userMessage);

        requestBody["messages"] = messagesArray;

        QUrl url("https://api.deepseek.com/chat/completions");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());

        // 邮递员出发！收到回复后依然会走之前的 on_api_reply_finished 槽函数
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
