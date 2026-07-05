#ifndef FLOATING_WIDGET_H
#define FLOATING_WIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QPoint>

/*
 * floating_widget.h
 * ------------------------------------------------------------
 * 桌面悬浮球（类似360悬浮球）
 *
 * 始终停留在屏幕最上层，点击展开快捷打卡按钮：
 *   - "💤 我入睡了"：一键记录入睡时间
 *   - "🌅 我起床了"：一键记录起床时间
 *
 * 支持拖拽移动位置，点击球体切换展开/折叠状态。
 */
class FloatingWidget : public QWidget {
    Q_OBJECT
public:
    explicit FloatingWidget();
    ~FloatingWidget() override;

signals:
    /** 用户点击了"💤 我入睡了" */
    void sleepRequested();
    /** 用户点击了"🌅 我起床了" */
    void wakeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void toggleExpand();
    void setExpanded(bool expanded);
    void setupButtons();
    void updateButtonPositions();

    QPushButton *m_sleepBtn;   // "我入睡了"按钮
    QPushButton *m_wakeBtn;    // "我起床了"按钮
    bool m_expanded;           // 是否展开
    bool m_dragging;           // 是否正在拖拽
    QPoint m_dragOffset;       // 拖拽偏移量

    // 尺寸常量
    static constexpr int BALL_SIZE   = 56;   // 月亮球直径
    static constexpr int BTN_W       = 110;  // 按钮宽度
    static constexpr int BTN_H       = 42;   // 按钮高度
    static constexpr int PADDING     = 8;    // 间距
    // 展开态总宽度 = 球体 + 2个按钮 + 3个间距
    static constexpr int EXPANDED_W  = BALL_SIZE + BTN_W * 2 + PADDING * 3;
    static constexpr int WIDGET_H    = 56;   // 组件高度（与球直径一致）
};

#endif // FLOATING_WIDGET_H
