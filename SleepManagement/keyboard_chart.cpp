#include "keyboard_chart.h"
#include "key_monitor.h"
#include <QFontMetrics>
#include <algorithm>

namespace {
    const int MARGIN_L = 50;
    const int MARGIN_R = 20;
    const int MARGIN_T = 30;
    const int MARGIN_B = 42;
    const QColor COLOR_GRID(0xE0, 0xE0, 0xE0);
    const QColor COLOR_AXIS(0x66, 0x66, 0x66);
    const QColor COLOR_FILL(0xF5, 0xF5, 0xF5);
    const QColor COLOR_LOW(0x6B, 0xCB, 0x77);   // ≤30% of peak
    const QColor COLOR_MED(0xFF, 0xD9, 0x3D);    // 30%~60% of peak
    const QColor COLOR_HIGH(0xFF, 0x6B, 0x6B);   // >60% of peak

    QColor barColor(int val, int peak) {
        if (peak == 0) return COLOR_LOW;
        double ratio = double(val) / peak;
        if (ratio > 0.6) return COLOR_HIGH;
        if (ratio > 0.3) return COLOR_MED;
        return COLOR_LOW;
    }
}

// ============================================================
//  KeyActivityChart
// ============================================================
KeyActivityChart::KeyActivityChart(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(280);
}

void KeyActivityChart::setData(const QVector<int> &data)
{
    m_data = data;
    m_peak = 0;
    for (int v : data) m_peak = std::max(m_peak, v);
    if (m_peak == 0) m_peak = 1;
    update();
}

QSize KeyActivityChart::minimumSizeHint() const { return QSize(400, 220); }
QSize KeyActivityChart::sizeHint() const         { return QSize(500, 250); }

void KeyActivityChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_data.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "暂无键盘活动记录");
        return;
    }

    QRect area = rect().adjusted(MARGIN_L, MARGIN_T, -MARGIN_R, -MARGIN_B);
    if (area.width() <= 10 || area.height() <= 10) return;

    // 背景
    p.fillRect(rect(), COLOR_FILL);
    p.fillRect(area, Qt::white);

    // 网格 + Y 轴标签
    p.save();
    p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));
    int nTicks = 4;
    for (int i = 0; i <= nTicks; ++i) {
        int y = area.bottom() - i * area.height() / nTicks;
        p.drawLine(area.left(), y, area.right(), y);
        p.setPen(COLOR_AXIS);
        QFont sf = font(); sf.setPointSize(8); p.setFont(sf);
        int label = m_peak * i / nTicks;
        p.drawText(QRect(0, y - 10, MARGIN_L - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(label));
        p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));
    }
    p.restore();

    // 柱状图
    int n = qMin(static_cast<int>(m_data.size()), 120);
    double barW = double(area.width()) / n * 0.7;
    double gap  = double(area.width()) / n * 0.3;

    p.save();
    QFont valFont = font(); valFont.setPointSize(7); p.setFont(valFont);

    for (int i = 0; i < n; ++i) {
        int idx = n - 1 - i;
        if (idx >= m_data.size()) continue;
        int val = m_data[idx];
        int barH = val * area.height() / m_peak;
        if (barH > area.height()) barH = area.height();

        double x = area.left() + i * (barW + gap) + gap / 2;
        QRectF barRect(x, area.bottom() - barH, barW, barH);

        QColor c = barColor(val, m_peak);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barRect, 2, 2);

        // 柱顶数值
        if (val > 0 && barH > 15) {
            p.setPen(QColor(0x33, 0x33, 0x33));
            p.drawText(QRectF(x - 4, area.bottom() - barH - 14, barW + 8, 12),
                       Qt::AlignCenter, QString::number(val));
        }
    }
    p.restore();

    // 修改X 轴标签：从右(现在)向左均匀取点，按文字实际宽度给足空间，避免裁剪与拥挤
    p.save();
    QFont lf = font(); lf.setPointSize(8); p.setFont(lf); p.setPen(COLOR_AXIS);
    QFontMetrics fm(lf);
    int desiredLabels = qBound(4, area.width() / 90, 8);   // 大约 4~8 个标签
    int labelStep = std::max(1, n / desiredLabels);
    for (int k = 0; k < n; k += labelStep) {
        int idx = n - 1 - k;                                // 从右往左，保证"现在"一定出现
        double x = area.left() + idx * (barW + gap) + (barW + gap) / 2;
        int minsAgo = k;
        QString lbl;
        if (minsAgo == 0)       lbl = "现在";
        else if (minsAgo < 60)  lbl = QString("%1分钟前").arg(minsAgo);
        else                    lbl = QString("%1小时前").arg(minsAgo / 60);
        int w = fm.horizontalAdvance(lbl) + 10;             // 按文字真实宽度撑开矩形
        p.drawText(QRectF(x - w / 2.0, area.bottom() + 6, w, 18),
                   Qt::AlignCenter, lbl);
    }
    p.restore();

}

// ============================================================
//  KeyMonitorDialog
// ============================================================
KeyMonitorDialog::KeyMonitorDialog(KeyMonitor *monitor, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("⌨️ 键盘活跃度分析");
    setMinimumSize(720, 560);
    resize(760, 580);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(16);
    layout->setContentsMargins(20, 16, 20, 16);

    // 标题
    auto *title = new QLabel("注意力曲线 — 每分钟键盘敲击频率", this);
    QFont tf = font(); tf.setPointSize(13); tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 统计信息（动态更新）
    auto *statsLabel = new QLabel(this);
    statsLabel->setAlignment(Qt::AlignCenter);
    statsLabel->setStyleSheet("color: #666; font-size: 12px; padding: 4px;");
    layout->addWidget(statsLabel);

    // 提示信息（初始时显示）
    auto *hintLabel = new QLabel("⌨️ 请在当前程序窗口内开始打字，数据将实时更新显示", this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #999; font-size: 12px; padding: 8px;");
    hintLabel->setWordWrap(true);
    layout->addWidget(hintLabel);

    // 更新统计信息的 lambda
    auto updateStats = [=]() {
        if (monitor && monitor->hasData()) {
            int total   = monitor->totalKeystrokes();
            int peak    = monitor->peakMinute();
            int active  = monitor->activeMinutes();
            int elapsed = monitor->elapsedMinutes();
            statsLabel->setText(
                QString("总计 %1 次按键  ·  峰值 %2 次/分钟  ·  活跃 %3/%4 分钟  ·  平均 %5 次/分钟")
                    .arg(total).arg(peak).arg(active)
                    .arg(elapsed > 0 ? elapsed : 1)
                    .arg(elapsed > 0 ? total / elapsed : 0));
            hintLabel->hide();
            statsLabel->show();
        } else {
            statsLabel->hide();
            hintLabel->show();
        }
    };

    // 图表
    auto *chart = new KeyActivityChart(this);
    if (monitor) {
        chart->setData(monitor->recentPerMinute(120));
        // 数据更新时刷新图表和统计
        connect(monitor, &KeyMonitor::dataUpdated, chart, [chart, monitor]() {
            chart->setData(monitor->recentPerMinute(120));
        });
        connect(monitor, &KeyMonitor::dataUpdated, this, updateStats);
        // 每次按键也刷新统计（数据总量变化时立即更新）
        connect(monitor, &KeyMonitor::keystrokeDetected, this, [=](int) {
            updateStats();
            // 如果图表暂无数据但用户正在打字，尝试刷新图表
            if (!monitor->hasData()) return;
            auto cur = monitor->recentPerMinute(120);
            if (!cur.isEmpty())
                chart->setData(cur);
        });
    }
    layout->addWidget(chart, 1);

    // 初始化显示状态
    updateStats();

    // 图例
    auto *legendWidget = new QWidget(this);
    auto *legendLayout = new QHBoxLayout(legendWidget);
    legendLayout->setSpacing(20);
    legendLayout->setAlignment(Qt::AlignCenter);

    struct { unsigned int color; QString text; } legends[] = {
        {0x6BCB77, "🟢 低活跃 (≤30%)"},
        {0xFFD93D, "🟡 中活跃 (30%~60%)"},
        {0xFF6B6B, "🔴 高活跃 (>60%)"},
    };
    for (auto &l : legends) {
        auto *lb = new QLabel(l.text, this);
        lb->setStyleSheet("font-size: 11px; color: #888;");
        legendLayout->addWidget(lb);
    }
    layout->addWidget(legendWidget);

    // 关闭按钮
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *closeBtn = new QPushButton("关闭", this);
    closeBtn->setFixedSize(100, 30);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: #4D96FF; color: white; border-radius: 6px; font-size: 13px; }"
        "QPushButton:hover { background-color: #3A7BD5; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
}
