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

//初始化默认参数
QJsonObject defaultUserConfig()
{
    QJsonObject cfg;
    cfg["general_sleep_hour"] = 23;   // 一般入睡 23:00
    cfg["general_wake_hour"]  = 8;    // 一般起床 08:00
    cfg["stayup_begin"]       = 0;    // 熬夜从 00:00 开始
    cfg["stayup_end"]         = 8;    // 熬夜到 08:00 结束
    return cfg;
}

//从硬盘加载参数
QJsonObject loadUserConfig(const QString &configFilePath)
{
    QFile file(configFilePath); // 1. 创建一个文件操作对象，绑定传入的本地文件路径
    // 2. 尝试以“只读”模式打开它。
    // 如果打不开，返回默认设置，确保程序能继续
    if (!file.open(QIODevice::ReadOnly))
        return defaultUserConfig();

    // 3. 如果打开成功，file.readAll() 会一次性把文件里所有的文本全部读到内存里
    //QJsonDocument::fromJson() 会负责将这些文本解析、翻译成一个标准的 Qt JSON 文档结构
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) // 6. 检查翻译出来的 JSON 格式对不对（是不是包含 {} 的完整对象）
        return defaultUserConfig(); //如果格式错误，依旧返回默认设置

    QJsonObject cfg = doc.object(); // 8. 格式完美无误，把文档转换成 QJsonObject 对象（也就是键值对账本）返回给调用者

    // 补全缺失的字段为默认值
    QJsonObject def = defaultUserConfig();
    for (auto it = def.begin(); it != def.end(); ++it) {
        if (!cfg.contains(it.key())) //如果字段有缺失
            cfg[it.key()] = it.value();
    }

    return cfg;
}

// 当用户在界面点了保存，负责把新的参数刷新并固化到本地硬盘文件里。
bool saveUserConfig(const QString &configFilePath, const QJsonObject &config)
{
    QFile file(configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // QJsonDocument doc(config)把装满新设置的 QJsonObject 账本包装进一个 QJsonDocument 转换工具里
    // doc.toJson() 把内存里的 JSON 结构揉成一段普通的文本字符串，file.write真正写进硬盘文件
    file.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void applyUserConfig(const QJsonObject &config)
{
    // 更新 sleep_core.h 中的全局变量
    // config["stayup_begin"] 是从账本里根据名字找出对应的值
    // .toInt(0) 代表把它翻译成整数数字。如果因为某种意外找不到这个键，就用数字 0 代替
    stayupBegin       = config["stayup_begin"].toInt(0);
    stayupEnd         = config["stayup_end"].toInt(8);
    generalSleep_hour = config["general_sleep_hour"].toInt(23);
    generalWake_hour  = config["general_wake_hour"].toInt(8);
}

// ============================================================
//  SettingsDialog 自定义设置弹窗内部逻辑
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

    // 创建一个微调输入框控件（就是带上下箭头的数字框）
    m_spGeneralSleep = new QSpinBox(this);
    // 限制输入范围是 0 到 24，防止用户在小时栏里乱填个 99 或者负数
    m_spGeneralSleep->setRange(0, 24);
    m_spGeneralSleep->setSuffix(" :00");
    m_spGeneralSleep->setFixedSize(100, 30);
    // 利用 ->setValue() 把从currentconfig里读到的值填到刚才创建的输入框里。
    m_spGeneralSleep->setValue(currentConfig["general_sleep_hour"].toInt(23));
    generalForm->addRow("😴 一般入睡时间：", m_spGeneralSleep);

    // 一般起床时间
    m_spGeneralWake = new QSpinBox(this);
    m_spGeneralWake->setRange(4, 12);
    m_spGeneralWake->setSuffix(" :00");
    m_spGeneralWake->setFixedSize(100, 30);
    m_spGeneralWake->setValue(currentConfig["general_wake_hour"].toInt(8));
    generalForm->addRow("🌅 一般起床时间：", m_spGeneralWake);

    // 把分组放入布局管理器（Layout）集中展示在界面上
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
    // 绑定：保存按钮发出的保存被点击的信号，绑定到QDialog的accept函数，这会让窗口关闭
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto *cancelBtn = new QPushButton("取消", this);
    cancelBtn->setFixedSize(80, 34);
    cancelBtn->setStyleSheet(
        "QPushButton { background-color: #E0E0E0; color: #333; border-radius: 6px; "
        "font-size: 13px; }"
        "QPushButton:hover { background-color: #D0D0D0; }");
    // 绑定：取消按钮发出的取消被点击的信号，绑定到QDialog的reject函数，这也会让窗口关闭
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
