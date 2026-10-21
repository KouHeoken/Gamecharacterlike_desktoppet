#include "desktopicons.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <objbase.h>
#  include <oleacc.h>
#  include <shellapi.h>
#  include <shlobj.h>
#endif

#ifdef Q_OS_WIN
namespace {

// oleacc.h 里的 IID_IAccessible 是 DEFINE_GUID 声明出来的外部符号，
// 能不能链上取决于 MinGW 的 uuid 库里有没有这一条。这里自己定义一份，
// 省掉链接期的不确定性。
const GUID kIID_IAccessible = {
    0x618736e0, 0x3c3d, 0x11cf, {0x81, 0x0c, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

/// 保证当前线程上 COM 可用。Qt 自己会初始化一次，这里只是兜底：
/// 已经初始化过时 CoInitializeEx 返回 S_FALSE，但那也算一次成功的调用，同样要配对一次。
class ComGuard
{
public:
    ComGuard() { m_mine = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)); }
    ~ComGuard()
    {
        if (m_mine)
            ::CoUninitialize();
    }

    ComGuard(const ComGuard &) = delete;
    ComGuard &operator=(const ComGuard &) = delete;

private:
    bool m_mine = false;
};

/// 找到桌面图标所在的那个列表控件（SysListView32）。
/// 桌面图标就是 explorer 里一个普通的 ListView，先定位到它的窗口句柄，
/// 后面才能通过无障碍接口向它要数据。
HWND desktopListView()
{
    HWND defView = nullptr;

    // 最常见的情况：Progman -> SHELLDLL_DefView -> SysListView32。
    const HWND progman = ::FindWindowW(L"Progman", nullptr);
    if (progman != nullptr)
        defView = ::FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);

    // 换过壁纸（幻灯片）之后，DefView 会被挪到某个 WorkerW 底下，得挨个找。
    for (HWND worker = nullptr; defView == nullptr; ) {
        worker = ::FindWindowExW(nullptr, worker, L"WorkerW", nullptr);
        if (worker == nullptr)
            break;
        defView = ::FindWindowExW(worker, nullptr, L"SHELLDLL_DefView", nullptr);
    }

    if (defView == nullptr)
        return nullptr;
    return ::FindWindowExW(defView, nullptr, L"SysListView32", nullptr);
}

/// 桌面列表控件的无障碍对象。调用方负责 Release()。失败返回 nullptr。
IAccessible *desktopAccessible()
{
    const HWND listView = desktopListView();
    if (listView == nullptr)
        return nullptr;

    IAccessible *list = nullptr;
    if (FAILED(::AccessibleObjectFromWindow(listView, OBJID_CLIENT, kIID_IAccessible,
                                            reinterpret_cast<void **>(&list))))
        return nullptr;
    return list;
}

/// 桌面图标都长在主屏上，所以按主屏的系统缩放比来换算。
/// 多显示器且各屏缩放比不同的时候这里不严谨，但桌宠一般就待在主屏上。
qreal desktopPixelRatio()
{
    if (const QScreen *screen = QGuiApplication::primaryScreen())
        return screen->devicePixelRatio();
    return 1.0;
}

/// shell 给你字符串的时候可能用三种写法之一（直接给宽字符、给个偏移量、
/// 或者给个本地编码的窄字符串），统一取出来变成 QString。
/// STRRET_WSTR 那种情况下这块内存是 shell 分配给我们的，取完要自己释放。
QString strRetToString(const STRRET &strret, const ITEMIDLIST *id)
{
    switch (strret.uType) {
    case STRRET_WSTR: {
        const QString text = QString::fromWCharArray(strret.pOleStr);
        ::CoTaskMemFree(strret.pOleStr);
        return text;
    }
    case STRRET_OFFSET:
        return QString::fromWCharArray(
            reinterpret_cast<const wchar_t *>(reinterpret_cast<const BYTE *>(id) + strret.uOffset));
    case STRRET_CSTR:
        return QString::fromLocal8Bit(strret.cStr);
    default:
        break;
    }
    return QString();
}

/// 让 shell 打开一条 ID 列表指向的东西，等同于在资源管理器里双击它。
/// 用 SEE_MASK_INVOKEIDLIST 是让 shell 走它自己的右键菜单那套逻辑，
/// 而不是去猜一个命令行 —— 桌面上的虚拟图标也只能这样打开。
bool executeItem(LPITEMIDLIST id)
{
    SHELLEXECUTEINFOW info;
    ::ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpIDList = id;
    info.nShow = SW_SHOWNORMAL;
    return ::ShellExecuteExW(&info);
}

/// 在桌面命名空间里按显示名字找到对应的一项，并打开它。
///
/// 桌面上的东西（快捷方式、文件、文件夹，还有此电脑、回收站这类虚拟图标）
/// 在 shell 眼里都是一条条 ID 列表，名字就跟图标下面写的一模一样。
/// 拿到 ID 列表交给 shell，虚拟图标也能打开 —— 它们磁盘上根本没有对应的文件，
/// 靠拼路径是拼不出来的。
bool openViaShellNamespace(const QString &name)
{
    IShellFolder *desktop = nullptr;
    if (FAILED(::SHGetDesktopFolder(&desktop)))
        return false;

    IEnumIDList *items = nullptr;
    const HRESULT enumResult = desktop->EnumObjects(
        nullptr, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &items);
    if (FAILED(enumResult)) {
        desktop->Release();
        return false;
    }

    bool opened = false;
    LPITEMIDLIST id = nullptr;
    while (items->Next(1, &id, nullptr) == S_OK) {
        if (!opened) {
            STRRET strret;
            ::ZeroMemory(&strret, sizeof(strret));
            if (SUCCEEDED(desktop->GetDisplayNameOf(id, SHGDN_NORMAL, &strret))
                && strRetToString(strret, id) == name) {
                opened = executeItem(id);
            }
        }
        ::CoTaskMemFree(id);
    }

    items->Release();
    desktop->Release();
    return opened;
}

} // namespace
#endif // Q_OS_WIN

QVector<DesktopIcon> DesktopIcons::enumerate()
{
    QVector<DesktopIcon> icons;

#ifdef Q_OS_WIN
    const ComGuard com;

    IAccessible *list = desktopAccessible();
    if (list == nullptr)
        return icons;

    long childCount = 0;
    if (SUCCEEDED(list->get_accChildCount(&childCount))) {
        for (long i = 1; i <= childCount; ++i) {
            // 无障碍接口里，一个"第 i 个子元素"是用 VT_I4 的 VARIANT 表示的。
            VARIANT child;
            ::VariantInit(&child);
            child.vt = VT_I4;
            child.lVal = i;

            DesktopIcon icon;
            icon.childId = int(i);

            BSTR rawName = nullptr;
            if (SUCCEEDED(list->get_accName(child, &rawName)) && rawName != nullptr) {
                icon.name = QString::fromWCharArray(rawName, int(::SysStringLen(rawName)));
                ::SysFreeString(rawName);
            }

            long left = 0, top = 0, width = 0, height = 0;
            if (SUCCEEDED(list->accLocation(&left, &top, &width, &height, child))) {
                // 无障碍接口给的是屏幕上的物理像素，而 Qt 里窗口的位置和大小都是
                // 逻辑像素（高分屏上两者差一个系统缩放比）。不换算的话整体会偏，
                // 而且越往右下偏得越多 —— 就是乘性的缩放差。
                const qreal ratio = desktopPixelRatio();
                icon.rect = QRect(qRound(left / ratio), qRound(top / ratio),
                                  qRound(width / ratio), qRound(height / ratio));
            }

            if (icon.rect.isValid())
                icons.append(icon);
        }
    }

    list->Release();
#endif

    return icons;
}

bool DesktopIcons::open(const DesktopIcon &icon)
{
#ifdef Q_OS_WIN
    const ComGuard com;

    if (openViaShellNamespace(icon.name))
        return true;

    // 兜底：让无障碍接口自己执行这个图标的"默认动作"，效果也是双击。
    IAccessible *list = desktopAccessible();
    if (list == nullptr)
        return false;

    VARIANT child;
    ::VariantInit(&child);
    child.vt = VT_I4;
    child.lVal = icon.childId;

    const HRESULT result = list->accDoDefaultAction(child);
    list->Release();
    return SUCCEEDED(result);
#else
    Q_UNUSED(icon);
    return false;
#endif
}
