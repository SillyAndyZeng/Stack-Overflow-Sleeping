#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 👇 这里就是我们补上的两个按钮的“插槽声明(菜单)”
private slots:
    void on_btn_sleep_clicked();
    void on_btn_wake_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
