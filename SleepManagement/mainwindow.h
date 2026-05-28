#ifndef MAINWINDOW_H // 宏保护（Header Guard）：防止这个头文件在编译时被重复包含，避免引发重复定义的编译错误
#define MAINWINDOW_H
#include <QStandardPaths>
#include <QDir>
#include <QMainWindow>
// 新增：引入网络和JSON相关的头文件
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/*
这个头文件用来定义你的主窗口类里有哪些变量、有哪些函数。
*/
QT_BEGIN_NAMESPACE
// 预先声明一个命名空间 Ui 里的 MainWindow 类。这个类是 Qt 编译时根据 mainwindow.ui 文件自动生成的，专门用来管理界面上的控件。
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 定义我们自己的 MainWindow 类，它公开继承自 QMainWindow（从而获得了官方主窗口的一切技能）
class MainWindow : public QMainWindow
{
    // 重要！这是 Qt 的核心宏。
    // 任何想要使用“信号与槽（Signals & Slots）”机制、动态属性系统的类，必须在首行写这个宏，否则编译会报错。
    Q_OBJECT

public:
    // 构造函数：用于初始化窗口。parent 指针用于指定父窗口，默认为 nullptr，表示它自己就是一个最顶层的独立窗口
    MainWindow(QWidget *parent = nullptr);
    // 析构函数：在窗口被关闭、销毁时自动调用，用来做清理工作（比如释放内存）
    ~MainWindow();

    // 这里是我们补上的两个按钮的“插槽声明(菜单)”
    // 当你的槽函数命名符合 "on_控件名_信号名()" 的规则时，Qt 会在运行时把对应的控件和这个函数自动绑定起来。
private slots:
    void on_btn_sleep_clicked();
    void on_btn_wake_clicked();
    // 新增：点击 AI 报告按钮的触发函数
    void on_btn_week_report_clicked();
    // 新增：大模型回信后，负责接收回复的函数
    void on_api_reply_finished(QNetworkReply *reply);
    void onCalendarDateSelected();//日历点击，数据回填
    // 新增：手动作息修改与手动保存生成的三个按钮槽函数
    void on_btn_edit_sleep_clicked();
    void on_btn_edit_wake_clicked();
    void on_btn_save_report_clicked();

private:
    // 核心指针：指向由 .ui 文件编译生成的界面类对象。
    // 在对应的 .cpp 文件中，你所有想操控的界面控件（日历、按钮、输入框等），全部都要通过这个 `ui->` 指针来访问！
    Ui::MainWindow *ui;
    // 新增：这是一个专门负责发 HTTP 请求的“邮递员”
    QNetworkAccessManager *networkManager;
    // 新增：告诉编译器有这几个私有辅助函数
    QString dataDir();
    void refreshCalendarColors();
    void checkAndShowAchievements();
    // 核心封装函数，传入作息日和各项具体数据进行保存和弹窗
    // 不传入一个SleppAnalyzer对象的原因是，在哪里实例化呢
    void save_and_report(QDate recordDay, int s_hour, int s_min, int w_hour, int w_min, int nap, int exe, int sit);
};

#endif // MAINWINDOW_H // 结束宏保护
