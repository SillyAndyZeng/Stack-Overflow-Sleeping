#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sleep_core.h"       // 队友的算分引擎
#include <QMessageBox>        // 弹窗交互
#include <QTime>              // 抓取系统时间
#include <QDate>              // 处理日历日期
#include <QTimeEdit>
#include <QSpinBox>
#include <QCalendarWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ==========================================
// 动作 1：用户点击【准备入睡】按钮
// ==========================================
void MainWindow::on_btn_sleep_clicked()
{
    QTime currentTime = QTime::currentTime();
    ui->timeEdit_sleep->setTime(currentTime);
    QMessageBox::information(this, "晚安守护", "已记录入睡时间！\n系统已进入静默模式，请放下手机，好好休息哦~");
}

// ==========================================
// 动作 2：用户点击【我醒了】按钮，进行数据结算
// ==========================================
void MainWindow::on_btn_wake_clicked()
{
    QTime currentTime = QTime::currentTime();
    ui->timeEdit_wake->setTime(currentTime);

    int s_hour = ui->timeEdit_sleep->time().hour();
    int s_min  = ui->timeEdit_sleep->time().minute();
    int w_hour = ui->timeEdit_wake->time().hour();
    int w_min  = ui->timeEdit_wake->time().minute();

    int nap = ui->spinBox_nap->value();
    int exe = ui->spinBox_exercise->value();
    int sit = ui->spinBox_sit->value();

    SleepAnalyzer todayData(s_hour, s_min, w_hour, w_min, nap, exe, sit);
    int score = todayData.getEnoughSleepScore();

    QString title = "";
    if(score >= 16) title = "🏆 睡眠大神，请受我一拜！";
    else if(score >= 10 && score <= 15) title = "✨ 健康的清澈大学生";
    else if(score >= 4 && score <= 9) title = "🏃 正在生死时速赶早八";
    else title = "💀 熬夜修仙者！系统警告，请惜命！";

    QDate selectedDate = ui->calendarWidget->selectedDate();
    QString dateStr = selectedDate.toString("yyyy年MM月dd日");

    QString finalReport = QString("【%1 清晨日结报告】\n\n数据录入成功！\n系统判定您的睡眠评分为： %2 分\n\n授予称号：\n%3")
                              .arg(dateStr)
                              .arg(score)
                              .arg(title);

    QMessageBox::information(this, "早安，打工人", finalReport);
}
