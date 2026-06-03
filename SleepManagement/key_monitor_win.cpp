// Windows 系统级按键计数读取
// 使用 WH_KEYBOARD_LL 低级键盘钩子，在后台线程中统计全局按键次数。
// 与 macOS 的 CGEventSourceCounterForEventType 思路一致：
//   - 只统计“按下”的次数，不记录任何具体按键内容（不是键盘记录器）；
//   - 跨应用全局生效，无需管理员权限；
//   - 对外暴露一个累计计数，KeyMonitor 通过前后两次差值得到时间窗口内的按键数。
#include <windows.h>
#include <atomic>
#include <thread>
#include <cstdint>

namespace {
    // 全局按键累计计数（自钩子安装以来）
    std::atomic<uint64_t> g_keyCount{0};
    // 钩子线程是否已启动 / 是否应继续运行
    std::atomic<bool>     g_running{false};
    // 钩子句柄
    HHOOK                 g_hook = nullptr;
    // 钩子线程的线程 ID（退出时用来唤醒消息循环）
    std::atomic<DWORD>    g_threadId{0};

    // 低级键盘钩子回调：每检测到一次按键按下就累加计数
    LRESULT CALLBACK lowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION) {
            // 只统计按下（WM_KEYDOWN / WM_SYSKEYDOWN，后者用于 Alt 组合键等），
            // 不统计抬起，避免一次敲击被计成两次。
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                g_keyCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // 必须把事件继续传递给系统，否则会“吞掉”用户的按键输入。
        return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }

    // 后台线程函数：安装低级键盘钩子，并运行自己的消息循环。
    // 低级钩子要求安装它的线程持续抽取消息，否则系统会让钩子失效，
    // 因此放在独立线程中，避免受主界面（GUI 线程）临时阻塞的影响。
    void hookThreadFunc()
    {
        g_threadId.store(GetCurrentThreadId(), std::memory_order_release);

        g_hook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            lowLevelKeyboardProc,
            GetModuleHandleW(nullptr),
            0);

        if (!g_hook) {
            g_running.store(false, std::memory_order_release);
            return;
        }

        MSG msg;
        while (g_running.load(std::memory_order_acquire) &&
               GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
        g_threadId.store(0, std::memory_order_release);
    }

    // 第一次调用时惰性启动后台钩子线程（线程安全，只会启动一次）
    void ensureHookStarted()
    {
        bool expected = false;
        if (g_running.compare_exchange_strong(expected, true)) {
            std::thread(hookThreadFunc).detach();
        }
    }
}

// 与 macOS 版本 macos_getSystemKeystrokeCount() 对应：
// 返回自钩子安装以来累计的全局按键次数。
extern "C" uint64_t windows_getSystemKeystrokeCount()
{
    ensureHookStarted();
    return g_keyCount.load(std::memory_order_relaxed);
}

// 可选：在程序退出时停止钩子线程并卸载钩子。
extern "C" void windows_stopKeystrokeMonitor()
{
    if (g_running.exchange(false)) {
        DWORD tid = g_threadId.load(std::memory_order_acquire);
        if (tid != 0) {
            // 向钩子线程发送 WM_QUIT，使其 GetMessage 返回 0 并退出循环
            PostThreadMessageW(tid, WM_QUIT, 0, 0);
        }
    }
}

// 几个值得知道的细节：钩子是在独立后台线程里跑自己的消息循环,这样即使主界面弹出模态对话框或一时繁忙,计数也不会丢——这是低级钩子的标准正确写法。它和 macOS 一样只累加按下次数,不保存按了哪个键,所以本质是个计数器,不是键盘记录器。按住不放产生的系统自动重复(auto-repeat)会被计入,这点和 macOS 的 kCGEventKeyDown 行为一致,属于刻意保持对等。另外启动后第一次按键可能因为基线尚未建立而漏记一次,可忽略不计。
// 唯一需要提醒的是:全局键盘钩子有时会被某些杀毒软件标记为可疑行为。如果测试时遇到计数不增长,优先排查是否被安全软件拦截了钩子安装。
// 改完直接在 Windows 上重新 CMake configure + build 即可,Mac 端不受影响(走的是 Q_OS_MACOS 分支)。