#include "floating_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>
#include <QMouseEvent>

// ============================================================
// 构造函数：初始化悬浮窗属性、位置和按钮
// ============================================================
FloatingWidget::FloatingWidget()
    : QWidget(nullptr)
    , m_sleepBtn(nullptr)
    , m_wakeBtn(nullptr)
    , m_expanded(false)
    , m_dragging(false)
{
    // 窗口标志：无边框、置顶、不在任务栏显示
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);  // 透明背景
    setAttribute(Qt::WA_ShowWithoutActivating);  // 显示时不激活（不抢焦点）
    setFixedSize(BALL_SIZE, WIDGET_H);           // 初始为折叠态尺寸

    // 默认位置：屏幕左侧垂直居中，距左边缘 20px
    if (auto *screen = QGuiApplication::primaryScreen()) {
        QRect geo = screen->availableGeometry();
        int x = geo.left() + 20;
        int y = geo.center().y() - WIDGET_H / 2;
        move(x, y);
    }

    setCursor(Qt::PointingHandCursor);
    setToolTip("点击打开睡眠快捷打卡面板");

    setupButtons();
}

FloatingWidget::~FloatingWidget() = default;

// ============================================================
// 创建两个快捷打卡按钮（初始隐藏）
// ============================================================
void FloatingWidget::setupButtons()
{
    // ---------- "💤 我入睡了" 按钮 ----------
    m_sleepBtn = new QPushButton("💤 我入睡了", this);
    m_sleepBtn->setFixedSize(BTN_W, BTN_H);
    m_sleepBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #5B8DEF, stop:1 #3D7AE0);"
        "  color: white; border: none; border-radius: 10px;"
        "  font-size: 14px; font-weight: bold; padding: 0 12px;"
        "}"
        "QPushButton:hover { background-color: #4A7DE0; }"
        "QPushButton:pressed { background-color: #2D6ED0; }"
    );
    connect(m_sleepBtn, &QPushButton::clicked, this, [this]() {
        setExpanded(false);        // 点击后折叠
        emit sleepRequested();     // 发射入睡信号
    });

    // ---------- "🌅 我起床了" 按钮 ----------
    m_wakeBtn = new QPushButton("🌅 我起床了", this);
    m_wakeBtn->setFixedSize(BTN_W, BTN_H);
    m_wakeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #FF9A56, stop:1 #F0803A);"
        "  color: white; border: none; border-radius: 10px;"
        "  font-size: 14px; font-weight: bold; padding: 0 12px;"
        "}"
        "QPushButton:hover { background-color: #F08A46; }"
        "QPushButton:pressed { background-color: #E07030; }"
    );
    connect(m_wakeBtn, &QPushButton::clicked, this, [this]() {
        setExpanded(false);        // 点击后折叠
        emit wakeRequested();      // 发射起床信号
    });

    // 两个按钮初始隐藏
    m_sleepBtn->hide();
    m_wakeBtn->hide();
}

// ============================================================
// 更新按钮在控件内的位置
// ============================================================
void FloatingWidget::updateButtonPositions()
{
    if (m_sleepBtn) {
        m_sleepBtn->move(BALL_SIZE + PADDING, (WIDGET_H - BTN_H) / 2);
    }
    if (m_wakeBtn) {
        m_wakeBtn->move(BALL_SIZE + BTN_W + PADDING * 2, (WIDGET_H - BTN_H) / 2);
    }
}

// ============================================================
// 展开/折叠切换
// ============================================================
void FloatingWidget::toggleExpand()
{
    setExpanded(!m_expanded);
}

void FloatingWidget::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;
    m_expanded = expanded;

    if (expanded) {
        // 展开：显示按钮，组件变宽
        setFixedSize(EXPANDED_W, WIDGET_H);
        m_sleepBtn->show();
        m_wakeBtn->show();
    } else {
        // 折叠：隐藏按钮，组件恢复球体大小
        m_sleepBtn->hide();
        m_wakeBtn->hide();
        setFixedSize(BALL_SIZE, WIDGET_H);
    }
    update();
}

// ============================================================
// 绘制事件：绘制月亮球及展开背景
// ============================================================
void FloatingWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // ---- 展开态：绘制圆角白色背景面板 ----
    if (m_expanded) {
        QPainterPath bgPath;
        bgPath.addRoundedRect(rect(), 26, 26);
        p.fillPath(bgPath, QColor(255, 255, 255, 240));
        p.setPen(QPen(QColor(200, 200, 200, 120), 1));
        p.drawPath(bgPath);
    }

    // ---- 球体阴影 ----
    QPainterPath shadowPath;
    shadowPath.addEllipse(4, 4, BALL_SIZE - 8, BALL_SIZE - 8);
    p.fillPath(shadowPath, QColor(0, 0, 0, 40));

    // ---- 蓝色渐变圆形球体 ----
    QRadialGradient grad(BALL_SIZE / 2, BALL_SIZE / 2, BALL_SIZE / 2 - 2);
    grad.setColorAt(0,   QColor(0x7B, 0xC0, 0xFF));  // 浅蓝高光
    grad.setColorAt(0.6, QColor(0x4D, 0x96, 0xFF));  // 主蓝色
    grad.setColorAt(1,   QColor(0x3D, 0x7E, 0xE0));  // 深蓝边缘

    p.setBrush(grad);
    p.setPen(QPen(QColor(0x2D, 0x6E, 0xD0), 1.5));
    p.drawEllipse(3, 3, BALL_SIZE - 6, BALL_SIZE - 6);

    // ---- 月亮弯弯（金色月牙） ----
    // 底层金色圆形
    p.setBrush(QColor(0xFF, 0xE0, 0x60));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(BALL_SIZE / 2 - 1, BALL_SIZE / 2 - 1), 14, 14);

    // 用蓝色遮罩切出月牙形状（颜色与球体主色一致，造成镂空效果）
    p.setBrush(QColor(0x4D, 0x96, 0xFF));
    p.drawEllipse(QPointF(BALL_SIZE / 2 + 4, BALL_SIZE / 2 - 4), 12, 12);

    // ---- 白色小星星 ----
    p.setBrush(Qt::white);
    p.drawEllipse(QPointF(18.0, 16.0), 2.5, 2.5);
    p.drawEllipse(QPointF(34.0, 14.0), 1.5, 1.5);
    p.drawEllipse(QPointF(30.0, 34.0), 2.0, 2.0);
    p.drawEllipse(QPointF(16.0, 30.0), 1.5, 1.5);

    // ---- 底部光泽反光 ----
    p.setBrush(QColor(255, 255, 255, 25));
    p.drawEllipse(14, BALL_SIZE - 16, BALL_SIZE - 28, 6);
}

// ============================================================
// 鼠标事件：实现拖拽移动
// ============================================================
void FloatingWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - pos();
    }
}

void FloatingWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void FloatingWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        // 如果鼠标几乎没移动，视为点击 → 切换展开/折叠
        QPoint delta = event->globalPosition().toPoint() - (pos() + m_dragOffset);
        if (delta.manhattanLength() < 5) {
            toggleExpand();
        }
    }
}

// ============================================================
// 尺寸变化时重新定位按钮
// ============================================================
void FloatingWidget::resizeEvent(QResizeEvent *event)
{
    updateButtonPositions();
    QWidget::resizeEvent(event);
}
