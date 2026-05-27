#include "mainwindow.h" // 引入对应的头文件声明
#include "ui_mainwindow.h"  //引入由 mainwindow.ui 编译生成的底层界面头文件，不引它就无法通过 ui-> 指针访问控件
#include "sleep_core.h"       // 队友的算分引擎
#include <QMessageBox>        // 引入 Qt 官方的弹窗对话框类，用于实现各种警告、信息提示弹窗
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>            // 引入时间编辑器控件的类声明
#include <QSpinBox>         // 引入数字微调框控件的类声明
#include <QCalendarWidget>  // 引入日历控件的类声明
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
