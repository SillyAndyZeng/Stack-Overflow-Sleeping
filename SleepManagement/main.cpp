#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
/*
它的唯一任务就是把整个应用程序启动起来，并把主窗口显示在屏幕上。
通常你不需要修改这个文件。
*/
//argc 是命令行参数个数，argv 是具体的参数数组
int main(int argc, char *argv[])
{

    ///实例化 Qt 应用程序对象。它负责管理整个图形界面的控制流、初始化各种底层资源，
    // 以及处理最核心的”事件循环”（比如分发鼠标点击、键盘输入等系统消息）
    QApplication a(argc, argv);

    // 设置应用图标（从 Qt 资源加载，确保任务栏和标题栏显示月亮图标）
    QIcon moonIcon(":/app_icon.png");
    if (!moonIcon.isNull()) {
        a.setWindowIcon(moonIcon);
    }

    // 2. 实例化自己设计的主窗口对象（此时窗口还在内存中创建，默认是隐藏的，屏幕上看不见）
    MainWindow w;

    // 3. 如果用户在欢迎弹窗中点击了”退出程序”，则直接返回，不显示窗口也不启动事件循环
    if (w.shouldQuitOnStart())
        return 0;

    // 4. 调用 show() 方法，让主窗口真正显示在屏幕上
    w.show();

    // 5. 核心！调用 a.exec() 让程序进入 Qt 的”主事件循环”并保持运行（进入一个无限循环，等待用户去点击或操作）。
    // 当用户彻底关闭主窗口后，这个循环才会结束，并返回一个状态码给系统，程序随之安全退出。
    return QApplication::exec();
}
