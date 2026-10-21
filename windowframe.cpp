#include "windowframe.h"

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <dwmapi.h>

namespace {

/// 把 Windows 给顶层窗口额外画的那圈装饰全部关掉：投影、Windows 11 的圆角，
/// 以及活动窗口那条细描边。必须先有原生窗口句柄（也就是窗口已经显示出来）才能调用。
void disableSystemWindowFrame(QWidget *window)
{
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd == nullptr)
        return;

    // 注意：这里不要设 DWMWA_NCRENDERING_POLICY = DWMNCRP_DISABLED。
    // 它看着很对症（关掉整个非客户区绘制），但实测会把窗口一起变成"不绘制"：
    // 窗口句柄在、visible 也是 true，屏幕上却什么都不显示。

    // Windows 11 默认把顶层窗口的四个角切成圆角，透明窗口上就会在四角多出缺口。
    // 下面两个属性编号是 Windows 11 才加的，MinGW 自带的 dwmapi.h 里还没有名字，
    // 所以直接写数值，在这里注明含义。
    const DWORD cornerPreference = 1; // DWMWCP_DONOTROUND
    ::DwmSetWindowAttribute(hwnd, 33, &cornerPreference, sizeof(cornerPreference)); // DWMWA_WINDOW_CORNER_PREFERENCE

    // 再顺手去掉 Windows 11 给活动窗口描的那条细边框（默认取系统强调色）。
    const DWORD borderColor = 0xFFFFFFFEu; // DWMWA_COLOR_NONE
    ::DwmSetWindowAttribute(hwnd, 34, &borderColor, sizeof(borderColor)); // DWMWA_BORDER_COLOR
}

} // namespace
#endif // Q_OS_WIN

void WindowFrame::makeFramelessToolWindow(QWidget *window, Qt::WindowFlags extraFlags)
{
    // 无边框 + 始终置顶 + 不进任务栏（Qt::Tool）。
    // Qt::NoDropShadowWindowHint 让 Qt 自己别加投影；Windows 那层由 DWM 画的阴影
    // 它管不到，交给 applyNoSystemDecoration() 处理。
    window->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint | Qt::Tool
                           | Qt::NoDropShadowWindowHint | extraFlags);
    // 关键：让窗口背景真正透明。不设这一条的话，无边框窗口会留下一块灰底板。
    window->setAttribute(Qt::WA_TranslucentBackground);
}

void WindowFrame::applyNoSystemDecoration(QWidget *window)
{
#ifdef Q_OS_WIN
    disableSystemWindowFrame(window);
#else
    Q_UNUSED(window);
#endif
}
