// macOS 系统级按键计数读取
// 使用 CGEventSourceCounterForEventType 获取自系统启动以来的按键总数
// 无需输入监控权限，跨应用可用
// API 文档：https://developer.apple.com/documentation/coregraphics/1456105-cgeventsourcecounterforeventtyp

#import <CoreGraphics/CoreGraphics.h>

// =====================================================
// 获取系统级按键次数（自系统启动以来的累计值）
// 通过前后两次调用差值，可精确获取时间窗口内的按键数量
// =====================================================
uint64_t macos_getSystemKeystrokeCount()
{
    // kCGEventSourceStateCombinedSessionState = 当前用户会话（所有应用）
    // kCGEventKeyDown = 按键按下事件
    int64_t count = CGEventSourceCounterForEventType(
        kCGEventSourceStateCombinedSessionState,
        kCGEventKeyDown
    );

    // 函数返回 -1 表示不支持（极低概率，仅出现在非常老的 macOS 版本上）
    return count > 0 ? (uint64_t)count : 0;
}
