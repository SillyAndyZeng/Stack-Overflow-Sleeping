# Stack-Overflow小组大作业报告：睡眠管理系统



# 一、程序功能介绍

我们制作了用户说明书，详情请见说明书



# 二、项目各模块与类设计细节

## sleep_core.h

这个文件只负责“纯数据和算法”，主要部分不依赖具体 UI。负责基本的C++可实现的全局变量定义、作息日数据分析、周报分析与JSON文件生成。

#### 基类：`SleepData`

作为每日数据的基本存储单位，是数据分析类的基类

#### 单日数据分析派生类：`SleepAnalyzer`

基于基类`SleepData`提供的数据所构建，负责自动统计每日睡眠时长、一周规律及熬夜天数，计算当日睡眠评分，包含多个与计算与输出显示相关的功能性函数，例如：

- `calculateNightSleep()`：计算晚上睡眠时间，并判断是否有晚起床（晚于平时两小时）现象
- `isStayUpLate`：判断是否熬夜，如果在设定的熬夜区间内入睡就算作熬夜
- `getEnoughSleepScore`：根据夜晚睡眠时长计算睡眠分数；区分了熬穿、睡懒觉（起太晚）、正常睡眠、睡太长这些情况

#### 周数据分析类：`WeeklyTracker`

将一整周每天的SleepAnalyzer对象作为核心变量，进行分析并综合评分，最后根据分数提供本地算法周报。

#### JSON文件生成

`SleepJsonExporter`这个类负责创建JSON格式的数据文件的所有操作。

通过`ostringstream`把所有数据拼成 JSON 格式的字符串，并进行转义。



## mainwindow.h  & mainwindow.cpp

负责定义主窗口类的变量及函数，以及具体的实现。包含所有的信号-槽函数（private slots）以及与信号无关的辅助函数（private）。public部分只包含构造函数、析构函数与退出窗口需要实现的函数。

具体函数的定义可查看代码注释。

## achievement_manager.h

成就管理器。负责成就的判定和显示逻辑，具体函数在mainwindow.cpp内调用。

#### 行为状态计算

通过`consecutiveCheckIn()`、`consecutiveEarlySleep()`、`perfectSleepDays()`、`goodSleepDays()`四个函数计算四种成就，并在调用时满足只在完全等于指定日期的那一天才会触发。

####  JSON 数据解析与时间回滚算法

在私有辅助函数 `readRecord()` 中，利用 `QFile` 打开对应日期的文本，通过 `QJsonDocument::fromJson` 将其反序列化为 `QJsonObject`，提取出`"sleep_hour"`等数据。

数据读取逻辑从今天（`QDate::currentDate()`）开始作为基准点，利用循环配合 `.addDays(-i)` 逐天向过去回溯。考虑到了调用函数的当天可能破坏判断逻辑，因此从昨天开始读取数据，只读取三个月内满足条件的最近的时间段。

#### 非侵入式纯逻辑设计

不继承任何 Qt 的 UI 类，内部也没有任何界面弹窗代码。它只接收数据目录路径 `m_dir`，通过暴露轻量级的公共接口（如 `consecutiveCheckIn()`、`currentBadge()`）向外（`mainwindow.cpp`）输出分析结果。

## settings_dialog.h & settings_dialog.cpp

管理用户自定义设置的配置文件，涉及了磁盘文件的写入读出，并负责提供用户自定义参数的界面控件操作逻辑。

#### 本地配置的持久化存储引擎（I/O 模块）

提供了 4 个面向底层磁盘 I/O 的结构化 C 风格全局函数，负责将配置信息以加密/结构化的 JSON 文本持久化保存在本地：

- **`defaultUserConfig()`**：生成一套默认配置（默认入睡23点、起床8点、0-8点算熬夜），作为系统初次运行或配置丢失时的底线。

- **`loadUserConfig(path)`**：使用 `QFile` 和 `QJsonDocument` 读取本地文件，并在文件不存在或损坏时具有**防崩溃的自动兜底机制**。

- **`saveUserConfig(path, config)`**：将内存中的 JSON 对象序列化转换为 UTF-8 文本流，安全写入磁盘。

- **`applyUserConfig(config)`**：将读取出来的用户参数动态赋值到 `sleep_core.h` 的全局变量中，实现配置对全局算分引擎的即时生效。

#### 数据驱动的架构

`SettingsDialog`所控制的窗口，并不直接操作零散的 UI 控件数据，而是采用 `QJsonObject`（JSON对象）作为统一的数据载体。

- **构造函数**：接收一个包含当前配置的 `QJsonObject` 并自动初始化界面控件。
- **输出接口**：提供 `getConfig() const` 公开函数，将用户在界面上所有修改后的结果，再次打包成一个 `QJsonObject` 返回给主窗口。

这种设计使得界面层设计与底层逻辑解耦，主窗口和配置窗口之间只通过一个标准的 JSON 对象进行数据交互。

#### 数据挂载机制

为了用最简单的方式将原本只有整点可选择的时间，修改为可选择半点的时间，使用了 `QComboBox`（下拉单选框）让用户选择时间。数据挂载机制指的是前端显示的是格式化的字符串如 `"23:00"`，但后台实际记录与操作的是浮点数`23.0`，简化了“时间字符串”与“数学逻辑分析”之间的转换。

#### 多级信号和槽的联动

在防止用户设置出不符合逻辑的作息时间（例如：设置“23:00入睡，22:00起床”这种逆向时间），设计了信号槽间（控件间）的**交互约束**，也算一种防呆设计吧。

用户在“入睡时间”下拉框中选择了某个时间（如 23:00），`onGeneralSleepChanged()` 会被触发，它会**动态重新计算并刷新**“起床时间”下拉框的可选范围（比如限制只能选择入睡后 1h 至 16h 之间的时间），并处理越界纠错逻辑。



## sleep_charts.h & sleep_charts.cpp

负责绘制近七天睡眠数据的柱状图和折线图，使用了高级自定义绘制（Custom Paint）技术，根据睡眠时长给予不同的颜色，提供睡眠时长趋势的可视化分析。

#### 基于QPainter 的2D 绘图引擎

通过重写 `paintEvent(QPaintEvent*)` 虚函数，利用 Qt的 `QPainter` 机制进行硬编码绘制。

- **`drawBackground`**：绘制底色与图表画布区域。
- **`drawGrid`**：计算最大值和步长，自适应绘制灰色参考网格线（`COLOR_GRID`）。
- **`drawLabels`**：利用 `QFontMetrics` 动态计算文本的像素宽度，渲染 X 轴日期和 Y 轴刻度文本。
- **`drawBars` / `drawLineChart`**：绘制图形（柱状图或折线图）。

#### 低耦合的组件组合模式

我们没有将所有图表混在一起写，而是拆分成了两个独立的、继承自 `QWidget` 的图形专用控件：`SleepBarChart`（柱状图组件）和 `SleepScoreChart`（折线趋势图组件）。 而最后，通过一个 `WeekChartDialog`（对话框组件），利用 `QVBoxLayout` 将这两个控件组合在一起。

对于柱状图，图表内嵌了颜色状态常量，并采用了双层堆叠柱状图，浅紫色的午睡时长堆叠在夜间睡眠之上。柱子的主颜色由当天的作息状态动态决定

对于折线图，利用 `QPainterPath` 和 `QLinearGradient`（线性渐变），不仅绘制了连贯的睡眠评分折线，还在折线下方填充了一层淡蓝到透明的**渐变阴影区域面积图**。

#### 自包含的数据载体

设计了一个专门的轻量级结构体 `SleepDayData`，类似于`sleep_core`的基类。它不仅封装了一天的核心数据（夜间睡眠、午睡分钟数、得分及熬夜状态标志），还利用内联函数提供了快捷数学转换接口，便于绘图函数调用：

```c++
double totalHours() const { return (nightSleepMin + daySleepMin) / 60.0; }
double nightHours() const { return nightSleepMin / 60.0; }
```



## key_monitor.h & key_monitor.cpp

键盘敲击注意力检测功能的上层界面。不关心当前程序运行在 Windows 还是 macOS，只负责通用的业务逻辑（如每 200ms 轮询一次计数、按分钟切分数据槽 `m_slots`、发送 Qt 信号等）。

重要工具：**条件编译（Conditional Compilation）**，用于和后面两个调用操作系统API的文件连接。

在 `key_monitor.cpp` 中，有以下代码：

```c++
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
extern "C" uint64_t macos_getSystemKeystrokeCount(); // 声明来自 macOS 文件的函数
#elif defined(Q_OS_WIN)
extern "C" uint64_t windows_getSystemKeystrokeCount(); // 声明来自 Windows 文件的函数
#endif

static uint64_t getSystemKeystrokeCount() {
#if defined(Q_OS_MACOS) ...
    return macos_getSystemKeystrokeCount();
#elif defined(Q_OS_WIN)
    return windows_getSystemKeystrokeCount();
#endif
}
```

- 编译期决定：当在 Windows 下编译程序时，CMake/Qt 编译器只会把 `key_monitor_win.cpp` 编译进去，并通过 `extern "C"` 让 `key_monitor.cpp` 成功调用到 `windows_getSystemKeystrokeCount()`；在 Mac 下编译时则同理调用 `macos_getSystemKeystrokeCount()`。
- 输入源统一：无论底层如何实现，最终都向跨平台的 `KeyMonitor` 提供一个**自启动以来（或安装钩子以来）的全局按键累计总数**（`uint64_t`类型）。



##  key_monitor_win.cpp & key_monitor_mac.mm

键盘敲击注意力检测功能的底层操作系统适配层。调用了各自操作系统独有的系统级 API（Windows 的全局钩子 Hook，macOS 的 CoreGraphics 事件计数器），获取系统全局的按键总数。

key_monitor.h & key_monitor.cpp 这两个文件和 key_monitor_win.cpp & key_monitor_mac.mm这两个文件，前者构成了软件的逻辑，定义了数据处理的模型；后者在底层实现上针对不同操作系统实现了适配数据处理模型，是典型的的‘抽象与实现分离’的架构设计。



## notification_manager.h & notification_manager.cpp

系统托盘与桌面气泡提醒管理器。起到了**“程序后台常驻生命周期管理中心”** 与 **“非侵入式用户状态反馈引擎”**的作用。

#### 最小化到托盘管理

利用 Qt 的 `QSystemTrayIcon` 将程序常驻在操作系统的右下角（Windows）或右上角导航栏（MacOS）。

- 托盘图标的内部构建了一个 `QMenu`，绑定了“显示主界面”、“自定义设置”以及“彻底退出程序”的 `QAction`
- 在 `onTrayActivated` 槽函数中，通过捕获用户的点击行为（`Trigger` 或 `DoubleClick`），安全地恢复主窗口的状态

#### 深夜熬夜行为的拦截

`checkLateNightCondition()` 函数是一个定时触发的“状态监测器”。

它通过一个每隔一定时间触发的 `QTimer`，在后台读取当前的系统时间（`QTime::currentTime()`）

当时间进入设定的熬夜区间1h以后时，主动通过 `QFile` 检索用户的本地作息 JSON 数据库，判断用户今天是否已经保存了入睡记录。

如果发现用户深夜未眠且未打卡，它会立刻警告气泡提示（`QSystemTrayIcon::Warning`）并播放警告音效，起到“健康守护”的干预效果。

#### 格式化良好的接口

提供了一组格式化良好的公共接口，供软件的其他模块（如算分引擎、成就系统、主界面）调用，以桌面气泡的形式向用户传达信息

- `showSleepNotification(...)`：入睡成功记录反馈，并提示进入静默状态。
- `showWakeNotification(...)`：早安打卡反馈。
- `showAchievementNotification(...)`：当触发连续打卡 7 天或 30 天等成就时，由 `AchievementManager` 触发此接口向桌面推送勋章解锁通知。

#### 弱耦合机制

`*NotificationManager`类的构造函数里，接收一个 `QMainWindow*` 指针，但并没有将其硬编码强绑定，而是作为父对象（`QObject` 的单根继承体系）管理。它利用  信号与槽机制来异步通知主窗口。

```c++
signals:
    void requestShowWindow(); // 当用户双击托盘时激活该信号，由 MainWindow 监听并决定如何恢复窗口
```

这种方式可以实现主界面（UI层）与后台管理（逻辑层）的解耦，降低主窗口的负担。

#### 系统音效调用

`NotificationManager`类中设计了专门的系统音效触发函数（如 `playWarning()`、`playAchievement()`）。

在底层实现中，还可以通过 `QProcess::startDetached("afplay", ...)` 异步调用 macOS 系统的原生音频播放命令，实现了跨平台兼容和多进程机制。

#### 自包含设计

另外一个很具有稳健性的设计是，使用了`generateSleepIcon()` 静态辅助函数。利用 `QPainter` 在内存的 `QPixmap` 上通过纯代码直接绘制出了在托盘中显示的图标。不依赖外部图片的简单设计，让程序不会因为外部图片路径丢失而导致托盘图标不可见。


## floating_widget.h & floating_widget.cpp

桌面悬浮球快捷打卡组件。独立于主窗口，始终停留在桌面最上层。基于 Qt 的常驻置顶与异形视窗机制。

#### 鼠标事件驱动的拖曳

通过计算全局鼠标坐标与窗口左上角的相对偏移量（`m_dragOffset`），实现了在全屏范围内丝滑的拖拽移动体验。

#### 无边框置顶窗口

在构造函数中，通过设置 Qt 的窗口标志（Window Flags），实现了悬浮球的窗口物理属性：

```c++
setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
setAttribute(Qt::WA_TranslucentBackground);  // 背景透明
setAttribute(Qt::WA_ShowWithoutActivating);  // 启动时不抢占系统焦点
```

#### 基于QPainter的矢量美术

悬浮球的视觉效果由 `paintEvent` 里的代码手工绘制。其内部利用了 `QLinearGradient`（双色环形/线性渐变阴影）渲染球体，并计算了像素点，绘制了数颗白色小星星和底部的“光泽反光层”。



# 三、小组成员分工情况

王晨瑜：主要负责界面美化；完成了以下工作：

- 编写键盘检测功能
- 编写睡眠图表功能
- 参与api功能优化
- 项目录制
- 前端搭建与初步美化
- 任务栏设计
- 说明书初步撰写

董弈齐：主要负责问题探索与程序调试；完成了以下工作：

- 初始功能与ui设计
- 编写成就功能
- 外观和图标优化
- 编写睡眠日记功能

曾梓航：主要负责Qt逻辑设计、搭建与调试；完成了以下工作：

- 时间逻辑设计与构建
- 日历构建
- api功能初步构建
- 自定义功能设计与构建
- 参与按钮与图表设计
- 参与多项功能优化、细节优化、注释撰写
- 参与项目录制
- 实验报告撰写
- 说明书后期撰写


# 四、项目总结与反思
