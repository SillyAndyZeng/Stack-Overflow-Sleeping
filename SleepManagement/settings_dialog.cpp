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
    cfg["general_sleep_hour"] = 23.0;   // 一般入睡 23:00
    cfg["general_wake_hour"]  = 8.0;    // 一般起床 08:00
    cfg["stayup_begin"]       = 0.0;    // 熬夜从 00:00 开始
    cfg["stayup_end"]         = 8.0;    // 熬夜到 08:00 结束
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
    // .toDouble 代表把它翻译成浮点数数字。如果因为某种意外找不到这个键，就用数字 0 代替
    stayupBegin       = config["stayup_begin"].toDouble(0.0);
    stayupEnd         = config["stayup_end"].toDouble(8.0);
    generalSleep_hour = config["general_sleep_hour"].toDouble(23.0);
    generalWake_hour  = config["general_wake_hour"].toDouble(8.0);
}

// =========================================
// 辅助函数：用来快速给下拉框填充 00:00 到 23:30
// ==========================================
// 中间两个参数是为了限制下拉单选框生成的时间的范围（只能为整数，方便for循环），用户不用修改，第三个参数是步长，后期可以开发者修改
// 最后一个参数是控制生成不连续的时间段的，第二个时间段生成时就不要清空box了
void SettingsDialog::initTimeComboBox(QComboBox *box, int start, int end, bool _clean) {
    // 【核心安全防护】暂时阻塞该组合框的信号，防止清除/添加项时误触发 currentIndexChanged 导致死循环
    bool oldState = box->blockSignals(true);

    if (_clean) box->clear(); //如果指定不清空就不清空
    // i指的是第几个"半小时"
    for (int i = start * 2; i < end * 2; ++i) {
        double actualHour = i * 0.5;
        while (actualHour >= 24.0) {
            actualHour -= 24.0; // 超过 24 点的时间（如 25.5）自动循环回第二天（1.5）
        }

        // 格式化显示文本，例如 23.5 -> "23:30"
        int hour = static_cast<int>(actualHour);
        int minute = (actualHour - hour) > 0 ? 30 : 0;
        QString text = QString("%1:%2").arg(hour, 2, 10, QChar('0')).arg(minute, 2, 10, QChar('0'));

        // 绑定的数据依然是 0.0 ~ 23.5 之间的标准浮点数
        box->addItem(text, actualHour);
    }

    box->blockSignals(oldState); // 恢复原有信号状态
}

// ============================================================
//  定义SettingsDialog类对象SettingsDialog 自定义设置弹窗内部逻辑
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

    // ========重要！给控件分配内存==========
    m_spGeneralSleep = new QComboBox(this);
    m_spGeneralWake = new QComboBox(this);
    m_spStayupBegin = new QComboBox(this);
    m_spStayupEnd = new QComboBox(this);

    // ---- "一般作息时间" 分组 ----
    auto *generalGroup = new QGroupBox("一般作息时间", this);
    generalGroup->setStyleSheet(
        "QGroupBox { font-weight: bold; border: 1px solid #D0D0D0; border-radius: 6px; "
        "margin-top: 10px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }");

    auto *generalForm = new QFormLayout(generalGroup);
    generalForm->setSpacing(8);
    generalForm->setContentsMargins(10, 16, 10, 10);

    //原本使用spinBox，但是为了只用小时就能实现整点半点，换成了现在的下拉单选框
    //一般入睡时间
    //为这个下拉单选框填写整点/半点选项；入睡时间限制20：00-4:30
    // 在这里实现的都是构造函数里默认的范围，后面起床时间和熬夜终止点会动态变化
    initTimeComboBox(m_spGeneralSleep, 20, 29);
    //initTimeComboBox(m_spGeneralSleep, 0, 5, 0.5, false);
    // 读取当前配置
    double sleepVal = currentConfig["general_sleep_hour"].toDouble(23.0);
    // ！让 Qt 自动查找哪个选项背后藏着 sleepVal 这个数字，并选择到这个选项上
    m_spGeneralSleep->setCurrentIndex(m_spGeneralSleep->findData(sleepVal));
    generalForm->addRow("😴 一般入睡时间：", m_spGeneralSleep);

    // 一般起床时间
    // 通过信号与槽实现：填写/读取了入睡时间后，起床时间只能晚于入睡时间
    initTimeComboBox(m_spGeneralWake, 4, 12); //起床时间限制4：00-11：30
    //读取配置并转换成索引
    double wakeVal = currentConfig["general_wake_hour"].toDouble(8.0);
    // 自动寻找 wakeVal 对应的选项
    m_spGeneralWake->setCurrentIndex(m_spGeneralWake->findData(wakeVal));
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

    //熬夜判定时间起始点

    initTimeComboBox(m_spStayupBegin, 21, 29); //熬夜判定区间可设置为21:00-4:30
    //initTimeComboBox(m_spStayupBegin, 0, 5, false);
    double stayupBVal = currentConfig["stayup_begin"].toDouble(0.0);
    // 自动寻找 stayupBVal 对应的选项
    m_spStayupBegin->setCurrentIndex(m_spStayupBegin->findData(stayupBVal));
    stayupForm->addRow("🌙 熬夜起始时间：", m_spStayupBegin);

    //熬夜判定时间终止点

    initTimeComboBox(m_spStayupEnd, 22, 36); //最多可设置为22：00-11:30
    //initTimeComboBox(m_spStayupEnd, 0, 12, 0.5, false);
    double stayupEVal = currentConfig["stayup_end"].toDouble(8.0);
    // 自动寻找 stayupupEVal 对应的选项
    m_spStayupEnd->setCurrentIndex(m_spStayupEnd->findData(stayupEVal));
    stayupForm->addRow("🌤  熬夜结束时间：", m_spStayupEnd);

    mainLayout->addWidget(stayupGroup);

    // -------【新增】绑定信号与槽----------
    connect(m_spGeneralSleep, &QComboBox::currentIndexChanged, this, &SettingsDialog::onGeneralSleepChanged);
    connect(m_spStayupBegin, &QComboBox::currentIndexChanged, this, &SettingsDialog::onStayupBeginChanged);

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
    // currentData().toDouble() 可以直接把刚才藏在选中项里的真实小时数字完整取出来
    cfg["general_sleep_hour"] = m_spGeneralSleep->currentData().toDouble();
    cfg["general_wake_hour"]  = m_spGeneralWake->currentData().toDouble();
    cfg["stayup_begin"]       = m_spStayupBegin->currentData().toDouble();
    cfg["stayup_end"]         = m_spStayupEnd->currentData().toDouble();
    return cfg;
}

// 槽函数定义：当一般入睡时间被修改时，一般起床时间的范围变化
void SettingsDialog::onGeneralSleepChanged() {
    // 1. 从下拉框里获取当前选中的入睡时间
    double sleepTime = m_spGeneralSleep->currentData().toDouble();

    // 2. 记住起床时间框当前的选中值
    double oldWakeTime = m_spGeneralWake->currentData().toDouble();

    // 3. 动态限制起床时间：最早不能早过入睡时间之后1h。
    // 允许往后推 16 个小时（比如 23:00 入睡，则起床选项自动生成 23:00 一直到次日 15:00）
    initTimeComboBox(m_spGeneralWake, sleepTime + 1, sleepTime + 16);

    // 4. 尝试恢复原来的选择
    int index = m_spGeneralWake->findData(oldWakeTime);
    if (index != -1) {
        m_spGeneralWake->setCurrentIndex(index);
    } else {
        // 如果原本选的时间不合法了（被裁掉了），默认设为入睡后 8 小时
        double defaultWake = sleepTime + 8.0;
        if (defaultWake >= 24.0) defaultWake -= 24.0;

        int defIdx = m_spGeneralWake->findData(defaultWake);
        if (defIdx != -1) m_spGeneralWake->setCurrentIndex(defIdx);
    }
}

void SettingsDialog::onStayupBeginChanged() {
    // 1. 获取当前选中的熬夜起始时间
    double beginTime = m_spStayupBegin->currentData().toDouble();

    // 2. 记住熬夜结束时间框当前的选中值
    double oldEndTime = m_spStayupEnd->currentData().toDouble();

    // 3. 动态限制熬夜结束时间：最早不能早过起始时间之后1h。
    // 熬夜区间通常最长不超过 12 小时（如 0:00 开始，最晚选到次日中午 12:00）
    initTimeComboBox(m_spStayupEnd, beginTime + 1, beginTime + 12);

    // 4. 尝试恢复原来的选择
    int index = m_spStayupEnd->findData(oldEndTime);
    if (index != -1) {
        m_spStayupEnd->setCurrentIndex(index);
    } else {
        // 如果不合法了，默认设为起始时间后 4 小时
        double defaultEnd = beginTime + 4.0;
        if (defaultEnd >= 24.0) defaultEnd -= 24.0;

        int defIdx = m_spStayupEnd->findData(defaultEnd);
        if (defIdx != -1) m_spStayupEnd->setCurrentIndex(defIdx);
    }
}