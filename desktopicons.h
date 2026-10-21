#ifndef DESKTOPICONS_H
#define DESKTOPICONS_H

#include <QRect>
#include <QString>
#include <QVector>

/**
 * 桌面上一个图标的信息。
 */
struct DesktopIcon
{
    /// 图标下面显示的名字。快捷方式不显示 .lnk 后缀。
    QString name;
    /// 图标在屏幕上的位置和大小。已经换算成跟 Qt 窗口一样的逻辑像素，
    /// 可以直接拿去跟 frameGeometry() 比。
    QRect rect;
    /// 这个图标在桌面列表控件里排第几（从 1 开始），打开它的时候要用。
    int childId = 0;
};

/**
 * 读取桌面信息：把桌面上所有图标的名字和位置读出来。
 *
 * 桌面上的图标其实是 explorer.exe 里一个普通的列表控件，别的进程直接读不到它的数据，
 * 所以这里走 Windows 的无障碍接口（MSAA）向它要 —— 这个接口本来就是给外部程序用的，
 * 数据会由系统搬到我们进程里，不用自己去别人进程的内存里翻。
 *
 * 只有 Windows 上有实现；别的平台 enumerate() 返回空列表，open() 返回 false。
 */
namespace DesktopIcons {

/// 读一遍桌面，返回当前所有图标。读不到（比如桌面被隐藏了）就返回空列表。
QVector<DesktopIcon> enumerate();

/// 打开一个图标，效果等同于在桌面上双击它。成功返回 true。
bool open(const DesktopIcon &icon);

} // namespace DesktopIcons

#endif // DESKTOPICONS_H
