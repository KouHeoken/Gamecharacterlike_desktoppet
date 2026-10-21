# Gamecharacterlike_desktoppet

一个用 Qt 6 / C++ 写的 Windows 桌面宠物：一只头顶不锈钢盆的 Q 版小女孩，住在你的桌面上，可以拖着走、用键盘遛，还能读取桌面图标、帮你按 F 打开压住的那个。

## 功能

- **透明窗口**：无边框、背景全透明、始终置顶，不在任务栏留图标，屏幕上只有角色本身
- **拖拽移动**：按住鼠标左键拖动
- **滚轮缩放**：以窗口中心为锚点缩放，尺寸限制在 50px ~ 1600px 之间
- **WASD 行走**：按住方向键让角色走动，走动时在「站立 / 迈步」两帧之间循环形成动画；左右会切换朝向，上下移动不改变朝向
- **站立正脸**：松开按键后固定显示正脸图 `front.png`
- **桌面图标气泡**：每次位置变化都会重新读一遍桌面图标，角色压住某个图标时，旁边弹出白色气泡显示它的名字
- **F 键打开图标**：按 F 打开气泡里显示的那个图标，等同于在桌面上双击它；「此电脑」「回收站」这类虚拟图标也能打开
- **右键菜单**：恢复默认大小 / 退出

## 直接运行

仓库里不含编译产物（`.gitignore` 排除了 `build/`、`*.exe`、`*.dll`），所以拉下来不能直接双击运行，得先构建。

构建完在 `build/` 里会得到 `Gamecharacterlike_desktoppet.exe`，但它还依赖 Qt 的 DLL。想得到一个能拷给别人、双击就开的目录，执行：

```bat
%QT_DIR%\bin\windeployqt.exe --release build\Gamecharacterlike_desktoppet.exe
```

`windeployqt` 会把 `Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Widgets.dll`、`platforms\qwindows.dll`、MinGW 运行时等一整套依赖摆到 exe 旁边，之后这个目录就能脱离 Qt 环境运行。

只支持 Windows 10 / 11。

## 从源码构建

需要：

- Windows 10 / 11
- Qt 6.5 以上（开发时用的是 **Qt 6.11.0 MinGW 64-bit**）
- CMake 3.16 以上
- MinGW 工具链（跟 Qt 一起装的那套即可）

**方式一：Qt Creator**

用 Qt Creator 打开 `CMakeLists.txt`，选好 Kit 直接构建运行。

**方式二：命令行**

```bat
set "QT_DIR=D:\Programing\Qt\6.11.0\mingw_64"
set "MINGW_BIN=D:\Programing\Qt\Tools\mingw1310_64\bin"
set "PATH=%MINGW_BIN%;%QT_DIR%\bin;%PATH%"

cmake -S . -B build -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
      -DCMAKE_MAKE_PROGRAM="%MINGW_BIN%\mingw32-make.exe" ^
      -DCMAKE_CXX_COMPILER="%MINGW_BIN%\g++.exe"
cmake --build build -j 8
```

**方式三：仓库里的脚本**

```powershell
.\build.ps1          # 配置 + 构建（Debug）
.\build.ps1 -Run     # 构建完直接启动
.\build.ps1 -Clean   # 清掉 build 目录
```

`build.bat` 用法相同（`build.bat` 直接构建 / `build.bat run` / `build.bat clean`）。

注意：这两个脚本里的 `QT_ROOT` 写死成了 `D:\Programing\Qt`，换机器要改。

**打包成可分发目录**

```bat
%QT_DIR%\bin\windeployqt.exe --release build\Gamecharacterlike_desktoppet.exe
```

## 操作一览

| 操作 | 效果 |
| --- | --- |
| 左键拖动 | 移动角色 |
| 鼠标滚轮 | 缩放角色（50 ~ 1600px） |
| W / A / S / D | 上下左右走，走动时播放动画 |
| F | 打开角色当前压住的桌面图标 |
| 右键 | 菜单：恢复默认大小 / 退出 |

键盘操作需要窗口先拿到焦点，点一下角色即可。

## 目录结构

```
main.cpp              程序入口
mainwindow.h/.cpp     主窗口：透明窗口、拖拽、缩放、行走动画、气泡调度
mainwindow.ui         界面（只有一张铺满窗口的 QLabel）
desktopicons.h/.cpp   读取桌面图标、打开图标（Windows 无障碍接口 + Shell 命名空间）
speechbubble.h/.cpp   说话气泡，独立顶层窗口，手绘白色圆角/椭圆气泡
windowframe.h/.cpp    无边框置顶窗口的公共设置，以及关掉 Windows 系统阴影和圆角
resources.qrc         角色图片资源
resources/images/whale/
    front.png         站立时的正脸
    left.png          朝左站立
    left_stride.png   朝左迈步
    right.png         朝右站立
    right_stride.png  朝右迈步
```

## 实现上的几个说明

- **窗口透明**：`WA_TranslucentBackground` + `FramelessWindowHint`，同时用 DWM 的 `DWMWA_WINDOW_CORNER_PREFERENCE` 和 `DWMWA_BORDER_COLOR` 关掉 Windows 11 自带的圆角与描边，否则角色四周会有一圈阴影或边框。
- **读桌面图标**：桌面的图标其实是 explorer 里一个普通列表控件，别的进程读不到它的数据，所以这里走 Windows 的无障碍接口（MSAA / `IAccessible`）向系统要。拿到的坐标是物理像素，代码里按 `devicePixelRatio` 换算成了逻辑像素。
- **打开图标**：走 Shell 命名空间（`IShellFolder` + `SHELLEXECUTEINFO`），所以「此电脑」「回收站」这类桌面上没有对应文件的虚拟图标也能打开。
- **键盘**：窗口是 `Qt::Tool`，不进任务栏也不抢焦点，需要点一下角色才有键盘输入；失焦时会清空按键状态，避免角色一直走。
- **气泡**：独立的顶层窗口，不是桌宠窗口的子控件。子控件只能画在父窗口矩形内，而气泡要能出现在角色外面；分开之后气泡怎么变宽变高都不会牵动角色在屏幕上的位置。

## 已知限制

- 只有 Windows 实现；别的平台上 `desktopicons.cpp` 编译不过（用到 Win32 API），窗口装饰那段同理。
- 读桌面图标依赖 explorer 的桌面列表控件，如果桌面被特殊软件接管（比如某些桌面整理工具），可能读不到。
- 无 macOS / Linux 版本。

## 许可

未指定。
