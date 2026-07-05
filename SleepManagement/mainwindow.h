#ifndef MAINWINDOW_H // 宏保护（Header Guard）：防止这个头文件在编译时被重复包含，避免引发重复定义的编译错误
#define MAINWINDOW_H
#include <QStandardPaths>
#include <QDir>
#include <QMainWindow>
#include <QCloseEvent>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
//新增：窗口缩放
#include <QHash>
#include <QRect>
#include <QSize>
// 新增：引入网络和JSON相关的头文件
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
// 为了实现工具菜单（清理工具）
#include <QMenu>
#include <QAction>

// 前置声明
class NotificationManager;
class KeyMonitor;
class FloatingWidget;
class QTextEdit;

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

    // 【新增】启动时是否选择了"退出程序"
    bool shouldQuitOnStart() const { return m_shouldQuitOnStart; }
    // 💡 【新增】：暴露出一个让外部通知程序即将彻底退出的接口
    void prepareToQuit() { m_isQuitting = true; }

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
    void on_btn_save_report_clicked(); //这个函数，和btn_wake_clicked应当等同
    // 新增：手动设定通宵按钮，通宵不再仅仅依赖于点我起床时了的选项
    void on_btn_set_nosleep_clicked();
    // 【新增：打开可视化图表窗口】
    void on_btn_show_chart_clicked();
    // 【新增：打开用户设置窗口】
    void on_btn_settings_clicked();
    // 【新增：打开键盘活跃度分析】
    void on_btn_keyboard_clicked();
    // 【新增：清理数据的三个动作槽函数】不能用clicked
    void on_action_clear_today_triggered();
    void on_action_clear_range_triggered();
    void on_action_clear_all_triggered();
    // 【新增：导出 AI 分析结果为 PDF】
    void on_btn_export_pdf_clicked();

protected:
    // 【新增：拦截关闭事件，最小化到系统托盘】
    void closeEvent(QCloseEvent *event) override;
    //【新增：为了调节尺寸】
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;     // 新增

private:
    // 核心指针：指向由 .ui 文件编译生成的界面类对象。
    // 在对应的 .cpp 文件中，你所有想操控的界面控件（日历、按钮、输入框等），全部都要通过这个 `ui->` 指针来访问！
    Ui::MainWindow *ui;
    // 新增：这是一个专门负责发 HTTP 请求的“邮递员”
    QNetworkAccessManager *networkManager;
    // 【新增：系统托盘与气泡提醒管理器】
    NotificationManager *m_notificationMgr;
    // 【新增：键盘敲击频率监测】
    KeyMonitor *m_keyMonitor;
    // 【新增】显示区域透明度动效用
    QList<QGraphicsOpacityEffect*> m_displayEffects;
    // 【新增】启动时用户选择"退出程序"的标记
    bool m_shouldQuitOnStart = false;
    // 【新增】程序主动退出标记，防止 closeEvent 误弹托盘通知
    bool m_isQuitting = false;
    // 【新增】桌面悬浮球（用于快捷打卡）
    FloatingWidget *m_floatingWidget = nullptr;
    // 睡眠日记是运行时动态创建的，不属于 Ui::MainWindow 生成类
    QTextEdit *m_diaryEdit = nullptr;
    // 新增：告诉编译器有这几个私有辅助函数
    QString dataDir();
    void refreshCalendarColors();
    void checkAndShowAchievements();
    // 专门用于动态计算并刷新晚间睡眠时长显示框的函数
    void updateDurationDisplay();
    // 核心封装函数，传入作息日和各项具体数据进行保存和弹窗
    // 不传入一个SleppAnalyzer对象的原因是，在哪里实例化呢
    void save_and_report(QDate recordDay, int s_hour, int s_min, int w_hour, int w_min, int nap, int exe, int sit);
    // 【新增】统一刷新主界面成就显示的函数
    void updateAchievementDisplay();
    // 【新增】按钮缩放动效
    void pulseButton(QPushButton *btn);
    // 【新增】欢迎说明书弹窗（force = true 忽略"不再显示"设置）
    // 返回 true 表示用户选择了"退出程序"
    bool showWelcomeDialog(bool force = false);
    // 【新增】生成说明书 HTML 内容
    static QString buildManualHtml();
    // 【窗口缩放】记录原始控件位置，用于 resize 时按比例重排
    QHash<QWidget*, QRect> m_baseGeometry;
    QSize m_baseSize = QSize(900, 680);

    void rememberBaseGeometry();
    void applyResponsiveGeometry();                     // 新增：统一的适配函数
};

#endif // MAINWINDOW_H // 结束宏保护
