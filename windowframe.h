#ifndef WINDOWFRAME_H
#define WINDOWFRAME_H

#include <QWidget>

/**
 * 桌宠里几个窗口共用的外观设置。
 *
 * 桌宠本身和它旁边的说话气泡都得"看起来不像窗口"：没有边框、没有底板、不进任务栏。
 * 这些设置有平台相关的坑，散落在各处很容易漏，所以集中放在这里。
 */
namespace WindowFrame {

/// 无边框 + 始终置顶 + 不进任务栏（Qt::Tool）+ 背景透明。
/// extraFlags 用来附加别的窗口标志，比如气泡要的"不接收鼠标"。
void makeFramelessToolWindow(QWidget *window, Qt::WindowFlags extraFlags = Qt::WindowFlags());

/// 关掉 Windows 自己给窗口加的那层装饰：投影、Windows 11 的圆角，以及活动窗口的描边。
/// 这三样都不归 Qt 管，只能拿到原生窗口句柄直接跟 DWM（桌面窗口管理器）说。
/// 句柄要等窗口显示出来才存在，所以放在 showEvent 里调用最合适；这个函数是幂等的。
void applyNoSystemDecoration(QWidget *window);

} // namespace WindowFrame

#endif // WINDOWFRAME_H
