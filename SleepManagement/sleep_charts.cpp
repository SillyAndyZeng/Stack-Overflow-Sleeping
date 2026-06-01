#include "sleep_charts.h"
#include <QFontMetrics>
#include <cmath>

// ============================================================
// 通用颜色常量（与日历染色一致）
// ============================================================
namespace {
    const QColor COLOR_GOOD(0x6B, 0xCB, 0x77);  // 绿：良好
    const QColor COLOR_NORMAL(0x4D, 0x96, 0xFF); // 蓝：正常
    const QColor COLOR_WARN(0xFF, 0xD9, 0x3D);   // 黄：熬夜
    const QColor COLOR_DANGER(0xFF, 0x6B, 0x6B); // 红：熬穿
    const QColor COLOR_NAP(0xC8, 0xA2, 0xC8);    // 紫：午睡区
    const QColor COLOR_GRID(0xE0, 0xE0, 0xE0);   // 网格线
    const QColor COLOR_AXIS(0x66, 0x66, 0x66);   // 坐标轴
    const QColor COLOR_FILL(0xF5, 0xF5, 0xF5);   // 背景填充
    const int MARGIN_L = 50;   // 左边距（Y轴标签）
    const int MARGIN_R = 20;   // 右边距
    const int MARGIN_T = 25;   // 上边距（标题）
    const int MARGIN_B = 35;   // 下边距（X轴标签）

    // 根据评分返回柱状颜色
    QColor barColor(int score, bool noNightSleep, bool stayUp) {
        if (noNightSleep) return COLOR_DANGER;
        if (score >= 3)   return COLOR_GOOD;
        if (score >= 1)   return COLOR_WARN;
        return COLOR_NORMAL;
    }
}

// ============================================================
//  SleepBarChart
// ============================================================
SleepBarChart::SleepBarChart(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(m_minHeightHint);
}

void SleepBarChart::setData(const QVector<SleepDayData> &data) {
    m_data = data;
    update();
}

QSize SleepBarChart::minimumSizeHint() const {
    return QSize(400, m_minHeightHint);
}
QSize SleepBarChart::sizeHint() const {
    return QSize(500, 300);
}

void SleepBarChart::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_data.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "暂无数据");
        return;
    }

    QRect area = rect().adjusted(MARGIN_L, MARGIN_T, -MARGIN_R, -MARGIN_B);
    if (area.width() <= 10 || area.height() <= 10) return;

    double maxVal = 12.0;  // 最大显示 12 小时

    drawBackground(p, area);
    drawGrid(p, area, maxVal);
    drawRefLines(p, area);
    drawBars(p, area, maxVal);
    drawLabels(p, area);
}

void SleepBarChart::drawBackground(QPainter &p, const QRect &area) {
    p.fillRect(rect(), COLOR_FILL);
    p.fillRect(area, Qt::white);
}

void SleepBarChart::drawGrid(QPainter &p, const QRect &area, double maxVal) {
    p.save();
    p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));

    int nTicks = static_cast<int>(maxVal);
    for (int i = 0; i <= nTicks; ++i) {
        int y = area.bottom() - static_cast<int>(i * area.height() / maxVal);
        p.drawLine(area.left(), y, area.right(), y);

        // Y 轴标签
        p.setPen(COLOR_AXIS);
        QFont smallFont = font();
        smallFont.setPointSize(8);
        p.setFont(smallFont);
        p.drawText(QRect(0, y - 10, MARGIN_L - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(i) + "h");
        p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));
    }
    p.restore();
}

void SleepBarChart::drawRefLines(QPainter &p, const QRect &area) {
    // 推荐睡眠范围 7h ~ 8h 的背景高亮
    p.save();
    double maxVal = 12.0;
    int y7 = area.bottom() - static_cast<int>(7 * area.height() / maxVal);
    int y8 = area.bottom() - static_cast<int>(8 * area.height() / maxVal);

    QRect band(area.left(), y8, area.width(), y7 - y8);
    p.fillRect(band, QColor(0x6B, 0xCB, 0x77, 30));

    // 参考线
    QPen refPen(QColor(0x6B, 0xCB, 0x77), 2, Qt::DashLine);
    p.setPen(refPen);
    p.drawLine(area.left(), y7, area.right(), y7);
    p.drawLine(area.left(), y8, area.right(), y8);

    // 标签
    QFont smallFont = font();
    smallFont.setPointSize(7);
    p.setFont(smallFont);
    p.setPen(QColor(0x6B, 0xCB, 0x77));
    p.drawText(area.right() - 65, y7 - 3, "推荐下限 7h");
    p.drawText(area.right() - 65, y8 - 3, "推荐上限 8h");

    p.restore();
}

void SleepBarChart::drawBars(QPainter &p, const QRect &area, double maxVal) {
    if (m_data.isEmpty()) return;
    int n = m_data.size();
    double barWidth = static_cast<double>(area.width()) / n * 0.6;
    double gap = static_cast<double>(area.width()) / n * 0.4;

    p.save();

    for (int i = 0; i < n; ++i) {
        double x = area.left() + i * (barWidth + gap) + gap / 2;
        const auto &d = m_data[i];

        // 夜间睡眠柱
        double nightH = d.nightHours();
        int hNight = static_cast<int>(nightH / maxVal * area.height());
        if (hNight > area.height()) hNight = area.height();

        QColor c = barColor(d.score, d.noNightSleep, d.stayUp);
        QRect barRect(static_cast<int>(x), area.bottom() - hNight,
                      static_cast<int>(barWidth), hNight);

        // 圆角柱
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(barRect, 3, 3);
        // 柱体上半部分加高光
        QLinearGradient grad(barRect.topLeft(), barRect.bottomLeft());
        grad.setColorAt(0, c.lighter(140));
        grad.setColorAt(1, c);
        p.setBrush(grad);
        p.drawRoundedRect(barRect, 3, 3);

        // 午睡部分（叠加在夜间之上）
        if (d.daySleepMin > 0) {
            double dayH = d.dayHours();
            int hDay = static_cast<int>(dayH / maxVal * area.height());
            QRect napRect(static_cast<int>(x), barRect.top() - hDay,
                          static_cast<int>(barWidth), hDay);
            p.setBrush(COLOR_NAP);
            p.drawRoundedRect(napRect, 3, 3);
        }

        // 数值标签
        QFont valFont = font();
        valFont.setPointSize(8);
        valFont.setBold(true);
        p.setFont(valFont);
        p.setPen(QColor(0x33, 0x33, 0x33));

        double totalH = d.totalHours();
        int labelY = area.bottom() - static_cast<int>(totalH / maxVal * area.height()) - 6;
        p.drawText(static_cast<int>(x) - 5, labelY,
                   static_cast<int>(barWidth) + 10, 15,
                   Qt::AlignCenter,
                   QString("%1h").arg(totalH, 0, 'f', 1));
    }

    p.restore();
}

void SleepBarChart::drawLabels(QPainter &p, const QRect &area) {
    if (m_data.isEmpty()) return;
    int n = m_data.size();
    double totalW = area.width();
    double step = totalW / n;

    p.save();
    QFont lblFont = font();
    lblFont.setPointSize(8);
    p.setFont(lblFont);
    p.setPen(COLOR_AXIS);

    for (int i = 0; i < n; ++i) {
        double cx = area.left() + i * step + step / 2;
        QString label = m_data[i].date.toString("MM/dd");
        p.drawText(QRectF(cx - step / 2, area.bottom() + 5, step, 20),
                   Qt::AlignCenter, label);
    }

    p.restore();
}

// ============================================================
//  SleepScoreChart
// ============================================================
SleepScoreChart::SleepScoreChart(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(m_minHeightHint);
}

void SleepScoreChart::setData(const QVector<SleepDayData> &data) {
    m_data = data;
    update();
}

QSize SleepScoreChart::minimumSizeHint() const {
    return QSize(400, m_minHeightHint);
}
QSize SleepScoreChart::sizeHint() const {
    return QSize(500, 220);
}

void SleepScoreChart::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_data.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, "暂无数据");
        return;
    }

    QRect area = rect().adjusted(MARGIN_L, MARGIN_T, -MARGIN_R, -MARGIN_B);
    if (area.width() <= 10 || area.height() <= 10) return;

    drawBackground(p, area);
    drawGrid(p, area);
    drawLineChart(p, area);
    drawLabels(p, area);
}

void SleepScoreChart::drawBackground(QPainter &p, const QRect &area) {
    p.fillRect(rect(), COLOR_FILL);
    p.fillRect(area, Qt::white);
}

void SleepScoreChart::drawGrid(QPainter &p, const QRect &area) {
    p.save();
    p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));

    // Score 范围 -5 ~ 3
    const double minS = -5.0, maxS = 3.0, range = maxS - minS;
    for (int v = -5; v <= 3; ++v) {
        double ratio = (v - minS) / range;
        int y = area.bottom() - static_cast<int>(ratio * area.height());

        p.drawLine(area.left(), y, area.right(), y);

        p.setPen(COLOR_AXIS);
        QFont sf = font();
        sf.setPointSize(8);
        p.setFont(sf);
        p.drawText(QRect(0, y - 10, MARGIN_L - 8, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v));
        p.setPen(QPen(COLOR_GRID, 1, Qt::DashLine));
    }
    p.restore();
}

void SleepScoreChart::drawLineChart(QPainter &p, const QRect &area) {
    int n = m_data.size();
    const double minS = -5.0, maxS = 3.0, range = maxS - minS;
    double step = static_cast<double>(area.width()) / (n - 1 > 0 ? n - 1 : 1);

    // 收集所有点坐标
    QVector<QPointF> points;
    for (int i = 0; i < n; ++i) {
        double ratio = (m_data[i].score - minS) / range;
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        double x = area.left() + i * step;
        double y = area.bottom() - ratio * area.height();
        points.append(QPointF(x, y));
    }

    // 填充区域渐变
    if (points.size() >= 2) {
        QPainterPath fillPath;
        fillPath.moveTo(points[0]);
        for (int i = 1; i < points.size(); ++i)
            fillPath.lineTo(points[i]);
        fillPath.lineTo(points.last().x(), area.bottom());
        fillPath.lineTo(points.first().x(), area.bottom());
        fillPath.closeSubpath();

        QLinearGradient grad(0, area.top(), 0, area.bottom());
        grad.setColorAt(0, QColor(0x4D, 0x96, 0xFF, 80));
        grad.setColorAt(1, QColor(0x4D, 0x96, 0xFF, 10));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawPath(fillPath);
    }

    // 画连线
    if (points.size() >= 2) {
        p.save();
        QPen linePen(QColor(0x4D, 0x96, 0xFF), 3);
        p.setPen(linePen);
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < points.size() - 1; ++i)
            p.drawLine(points[i], points[i + 1]);
        p.restore();
    }

    // 画数据点
    p.save();
    for (int i = 0; i < points.size(); ++i) {
        QColor dotColor = barColor(m_data[i].score, m_data[i].noNightSleep, m_data[i].stayUp);
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(dotColor);
        p.drawEllipse(points[i], 7, 7);

        // 评分数值
        QFont vf = font();
        vf.setPointSize(8);
        vf.setBold(true);
        p.setFont(vf);
        p.setPen(QColor(0x33, 0x33, 0x33));
        p.drawText(points[i].x() - 10, points[i].y() - 16, 20, 14,
                   Qt::AlignCenter, QString::number(m_data[i].score));
    }
    p.restore();
}

void SleepScoreChart::drawLabels(QPainter &p, const QRect &area) {
    if (m_data.isEmpty()) return;
    int n = m_data.size();
    double step = static_cast<double>(area.width()) / (n - 1 > 0 ? n - 1 : 1);

    p.save();
    QFont lf = font();
    lf.setPointSize(8);
    p.setFont(lf);
    p.setPen(COLOR_AXIS);

    for (int i = 0; i < n; ++i) {
        double x = area.left() + i * step;
        QString label = m_data[i].date.toString("MM/dd");
        p.drawText(QRectF(x - step / 2, area.bottom() + 5, step, 20),
                   Qt::AlignCenter, label);
    }
    p.restore();
}

// ============================================================
//  WeekChartDialog
// ============================================================
WeekChartDialog::WeekChartDialog(const QVector<SleepDayData> &data, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("📊 本周作息可视化分析");
    setMinimumSize(600, 580);
    resize(640, 600);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 标题
    auto *titleLabel = new QLabel("本周作息可视化报告", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont tf = font();
    tf.setPointSize(14);
    tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setStyleSheet("color: #333; padding: 6px;");
    mainLayout->addWidget(titleLabel);

    // 提示标签
    auto *hintLabel = new QLabel("绿色柱状越高 = 睡眠越充足  |  评分折线向上 = 作息越规律", this);
    hintLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setStyleSheet("color: #888; font-size: 11px; padding-bottom: 4px;");
    mainLayout->addWidget(hintLabel);

    // 每日睡眠时长柱状图
    auto *barTitle = new QLabel("每日睡眠时长", this);
    barTitle->setStyleSheet("font-weight: bold; font-size: 12px; color: #555; padding-left: 4px;");
    mainLayout->addWidget(barTitle);

    auto *barChart = new SleepBarChart(this);
    barChart->setData(data);
    mainLayout->addWidget(barChart, 3);  // stretch 3

    // 睡眠评分趋势图
    auto *scoreTitle = new QLabel("每日睡眠评分趋势", this);
    scoreTitle->setStyleSheet("font-weight: bold; font-size: 12px; color: #555; padding-left: 4px;");
    mainLayout->addWidget(scoreTitle);

    auto *scoreChart = new SleepScoreChart(this);
    scoreChart->setData(data);
    mainLayout->addWidget(scoreChart, 2);  // stretch 2

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
    mainLayout->addLayout(btnLayout);
}
