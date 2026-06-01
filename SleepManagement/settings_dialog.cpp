#include "settings_dialog.h"
#include "sleep_core.h"   // 需要知道里面的全局变量
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QGroupBox>

// ============================================================
// 配置管理函数
// ============================================================

// 全局变量声明（声明在 sleep_core.h 里）
// 默认值也在其中有设置

QJsonObject defaultUserConfig()
{
    QJsonObject cfg;
    cfg["general_sleep_hour"] = 23;   // 一般入睡 23:00
    cfg["general_wake_hour"]  = 8;    // 一般起床 08:00
    cfg["stayup_begin"]       = 0;    // 熬夜从 00:00 开始
    cfg["stayup_end"]         = 8;    // 熬夜到 08:00 结束
    return cfg;
}

QJsonObject loadUserConfig(const QString &configFilePath)
{
    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return defaultUserConfig();

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
        return defaultUserConfig();

    QJsonObject cfg = doc.object();

    // 补全缺失的字段为默认值
    QJsonObject def = defaultUserConfig();
    for (auto it = def.begin(); it != def.end(); ++it) {
        if (!cfg.contains(it.key()))
            cfg[it.key()] = it.value();
    }

    return cfg;
}

bool saveUserConfig(const QString &configFilePath, const QJsonObject &config)
{
    QFile file(configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void applyUserConfig(const QJsonObject &config)
{
    // 更新 sleep_core.h 中的全局变量
    stayupBegin       = config["stayup_begin"].toInt(0);
    stayupEnd         = config["stayup_end"].toInt(8);
    generalSleep_hour = config["general_sleep_hour"].toInt(23);
    generalWake_hour  = config["general_wake_hour"].toInt(8);
}

// ============================================================
//  SettingsDialog
// ============================================================
SettingsDialog::SettingsDialog(const QJsonObject &currentConfig, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("⚙️ 用户偏好设置");
    setFixedSize(400, 400);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    // ---- 标题 ----
    auto *titleLabel = new QLabel("自定义作息参数", this);
    QFont tf = font();
    tf.setPointSize(14);
    tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #333; padding-bottom: 6px;");
    mainLayout->addWidget(titleLabel);

    // 提示文字
    auto *hintLabel = new QLabel("调整下方的参数，算法将据此判断熬夜、睡懒觉等行为。", this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #888; font-size: 11px; padding-bottom: 8px;");
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);

    // ---- "一般作息时间" 分组 ----
    auto *generalGroup = new QGroupBox("一般作息时间", this);
    generalGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #D0D0D0; border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }");

    auto *generalForm = new QFormLayout(generalGroup);
    generalForm->setSpacing(8);
    generalForm->setContentsMargins(10, 16, 10, 10);

    m_spGeneralSleep = new QSpinBox(this);
    m_spGeneralSleep->setRange(18, 23);
    m_spGeneralSleep->setSuffix(" :00");
    m_spGeneralSleep->setFixedSize(100, 30);
    m_spGeneralSleep->setValue(currentConfig["general_sleep_hour"].toInt(23));
    generalForm->addRow("😴 一般入睡时间：", m_spGeneralSleep);

    m_spGeneralWake = new QSpinBox(this);
    m_spGeneralWake->setRange(5, 12);
    m_spGeneralWake->setSuffix(" :00");
    m_spGeneralWake->setFixedSize(100, 30);
    m_spGeneralWake->setValue(currentConfig["general_wake_hour"].toInt(8));
    generalForm->addRow("🌅 一般起床时间：", m_spGeneralWake);

    mainLayout->addWidget(generalGroup);

    // ---- "熬夜判定区间" 分组 ----
    auto *stayupGroup = new QGroupBox("熬夜判定区间（在此区间入睡视为熬夜）", this);
    stayupGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #D0D0D0; border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }");

    auto *stayupForm = new QFormLayout(stayupGroup);
    stayupForm->setSpacing(8);
    stayupForm->setContentsMargins(10, 16, 10, 10);

    m_spStayupBegin = new QSpinBox(this);
    m_spStayupBegin->setRange(0, 23);
    m_spStayupBegin->setSuffix(" :00");
    m_spStayupBegin->setFixedSize(100, 30);
    m_spStayupBegin->setValue(currentConfig["stayup_begin"].toInt(0));
    stayupForm->addRow("🌙 熬夜起始时间：", m_spStayupBegin);

    m_spStayupEnd = new QSpinBox(this);
    m_spStayupEnd->setRange(0, 12);
    m_spStayupEnd->setSuffix(" :00");
    m_spStayupEnd->setFixedSize(100, 30);
    m_spStayupEnd->setValue(currentConfig["stayup_end"].toInt(8));
    stayupForm->addRow("🌤  熬夜结束时间：", m_spStayupEnd);

    mainLayout->addWidget(stayupGroup);

    // ---- 按钮 ----
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *saveBtn = new QPushButton("💾 保存设置", this);
    saveBtn->setFixedSize(120, 34);
    saveBtn->setStyleSheet(
        "QPushButton { background-color: #4D96FF; color: white; border-radius: 6px; "
        "font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #3A7BD5; }");
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto *cancelBtn = new QPushButton("取消", this);
    cancelBtn->setFixedSize(80, 34);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #E0E0E0; color: #333; border-radius: 6px; "
        "font-size: 13px; }"
        "QPushButton:hover { background-color: #D0D0D0; }");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnLayout->addWidget(saveBtn);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);
}

QJsonObject SettingsDialog::getConfig() const
{
    QJsonObject cfg;
    cfg["general_sleep_hour"] = m_spGeneralSleep->value();
    cfg["general_wake_hour"]  = m_spGeneralWake->value();
    cfg["stayup_begin"]       = m_spStayupBegin->value();
    cfg["stayup_end"]         = m_spStayupEnd->value();
    return cfg;
}
