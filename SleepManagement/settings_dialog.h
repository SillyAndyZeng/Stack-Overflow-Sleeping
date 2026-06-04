#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#pragma once
#include <QDialog>
#include <QComboBox> // 引入下拉单选框
#include <QSpinBox>
#include <QLabel>
#include <QJsonObject>

// ============================================================
// 用户配置读写接口（管理 sleep_core.h 中的全局变量）
// ============================================================

// 从磁盘加载用户配置；若文件不存在则返回默认值
QJsonObject loadUserConfig(const QString &configFilePath);

// 将用户配置保存到磁盘
bool saveUserConfig(const QString &configFilePath, const QJsonObject &config);

// 将配置应用到 sleep_core.h 的全局变量
void applyUserConfig(const QJsonObject &config);

// 生成带默认值的配置对象
QJsonObject defaultUserConfig();

// ============================================================
// SettingsDialog — 用户设置界面
// ============================================================
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const QJsonObject &currentConfig, QWidget *parent = nullptr);

    // 返回用户修改后的配置
    QJsonObject getConfig() const;

private slots:
    // 声明两个专门用于监听主控框变化的槽函数
    void onGeneralSleepChanged(); // 当一般入睡时间改变时触发
    void onStayupBeginChanged();  // 当熬夜区间起始点改变时触发

private:
    // 辅助函数
    void initTimeComboBox(QComboBox *box, int start=0, int end=24, bool _clean=true);
    // 2. 将原本的 QSpinBox* 替换为 QComboBox*
    QComboBox *m_spGeneralSleep;  // 一般入睡时间（浮点数）
    QComboBox *m_spGeneralWake;   // 一般起床时间（浮点数）
    QComboBox *m_spStayupBegin;   // 熬夜判定起始（浮点数）
    QComboBox *m_spStayupEnd;     // 熬夜判定结束（浮点数）
};

#endif // SETTINGS_DIALOG_H
