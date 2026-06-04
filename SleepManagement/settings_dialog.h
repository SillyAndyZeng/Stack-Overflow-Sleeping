#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QLabel>
#include <QJsonObject>
#include <QTimeEdit>   // 新增

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

private:
    QSpinBox *m_spGeneralSleep;  // 一般入睡时间（小时）
    QSpinBox *m_spGeneralWake;   // 一般起床时间（小时）
    QSpinBox *m_spStayupBegin;   // 熬夜判定起始（小时）
    QSpinBox *m_spStayupEnd;     // 熬夜判定结束（小时）
};

#endif // SETTINGS_DIALOG_H
