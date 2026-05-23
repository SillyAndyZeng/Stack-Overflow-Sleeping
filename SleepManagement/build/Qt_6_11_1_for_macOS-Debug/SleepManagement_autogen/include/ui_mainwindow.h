/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCalendarWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTimeEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QCalendarWidget *calendarWidget;
    QTimeEdit *timeEdit_sleep;
    QTimeEdit *timeEdit_wake;
    QSpinBox *spinBox_nap;
    QSpinBox *spinBox_exercise;
    QSpinBox *spinBox_sit;
    QPushButton *btn_sleep;
    QPushButton *btn_wake;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        calendarWidget = new QCalendarWidget(centralwidget);
        calendarWidget->setObjectName("calendarWidget");
        calendarWidget->setGeometry(QRect(20, 20, 331, 181));
        timeEdit_sleep = new QTimeEdit(centralwidget);
        timeEdit_sleep->setObjectName("timeEdit_sleep");
        timeEdit_sleep->setGeometry(QRect(410, 20, 118, 22));
        timeEdit_wake = new QTimeEdit(centralwidget);
        timeEdit_wake->setObjectName("timeEdit_wake");
        timeEdit_wake->setGeometry(QRect(410, 60, 118, 22));
        spinBox_nap = new QSpinBox(centralwidget);
        spinBox_nap->setObjectName("spinBox_nap");
        spinBox_nap->setGeometry(QRect(430, 110, 42, 22));
        spinBox_exercise = new QSpinBox(centralwidget);
        spinBox_exercise->setObjectName("spinBox_exercise");
        spinBox_exercise->setGeometry(QRect(430, 150, 42, 22));
        spinBox_sit = new QSpinBox(centralwidget);
        spinBox_sit->setObjectName("spinBox_sit");
        spinBox_sit->setGeometry(QRect(430, 180, 42, 22));
        btn_sleep = new QPushButton(centralwidget);
        btn_sleep->setObjectName("btn_sleep");
        btn_sleep->setGeometry(QRect(60, 340, 181, 41));
        btn_wake = new QPushButton(centralwidget);
        btn_wake->setObjectName("btn_wake");
        btn_wake->setGeometry(QRect(60, 410, 181, 41));
        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 33));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        btn_sleep->setText(QCoreApplication::translate("MainWindow", "\360\237\222\244 \345\207\206\345\244\207\345\205\245\347\235\241", nullptr));
        btn_wake->setText(QCoreApplication::translate("MainWindow", "\360\237\214\205 \346\210\221\351\206\222\344\272\206", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
