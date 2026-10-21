#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QSet>
#include <QTimer>

#include <optional>

#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

#include "desktopicons.h"

class SpeechBubble;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * 桌宠主窗口。
 *
 * 窗口本身无边框、背景透明、始终置顶，里面只放一个 QLabel 用来显示角色图片，
 * 所以屏幕上看起来就只有角色本身，没有窗口边框和底板。
 *
 * 交互：
 *   - 按住鼠标左键拖动：移动窗口；
 *   - 滚动鼠标滚轮：以窗口中心为锚点缩放角色；
 *   - 键盘 WASD：让角色上下左右走，走动时在两帧之间循环形成动画；
 *   - 键盘 F：打开跟角色重合的那个桌面图标（等同于在桌面上双击它）；
 *   - 鼠标右键：弹出菜单（恢复默认大小 / 退出）。
 *
 * 每次位置变化都会重新读一遍桌面图标，角色压在图标上时，用旁边一个白色气泡
 * 把图标名字显示出来。
 *
 * 角色不会自己乱跑，只在你按着 WASD 的时候动。
 * 键盘操作需要窗口先拿到焦点，点一下角色即可。
 */
class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// 角色朝向。只有左右两种，上下移动不会改变它。
    enum class Facing { Left = 0, Right = 1 };
    /// 走动动画的两帧：站住 / 迈步。
    enum Frame { FrameStand = 0, FrameStride = 1 };

    /// 缩放到指定比例，并保持窗口中心不动。
    void setScale(qreal scale);
    /// 按当前的 m_scale 重新生成图片，并把窗口调整成图片大小。
    void updateScaledPixmap();
    /// 把四个方向的两帧图，外加站住时用的正脸图，都从资源里读进来。
    void loadFrames();
    /// 按当前朝向和动画帧，取一张原图放到 m_sourcePixmap 里显示出来。
    void applyFrame();
    /// 切换朝向（朝向变了要立刻换图）。
    void setFacing(Facing facing);
    /// 根据当前按下的键，更新朝向并启停走路的两个定时器。
    void updateWalkState();
    /// 平移定时器：按当前按住的键把窗口挪一格。
    void stepMove();
    /// 动画定时器：在站立帧和迈步帧之间来回切。
    void stepFrame();
    /// 挑出桌宠此刻压住的那个桌面图标：跟窗口重合面积最大的一个。
    /// 气泡显示哪一条、按 F 打开哪一个，都走这里，保证两者永远盯着同一个目标。
    /// 没有压住任何图标时返回 std::nullopt。
    std::optional<DesktopIcon> iconUnderPet() const;
    /// 打开 iconUnderPet() 挑出来的那个图标。
    void openIconUnderPet();
    /// 把 iconUnderPet() 挑出来的那一个名字显示到气泡里；没有压住图标就藏起气泡。
    void refreshBubble();

    Ui::MainWindow *ui;

    /// 四张原图。第一个下标是朝向（Facing），第二个下标是帧（Frame）。
    /// 每次缩放都从它重新采样，避免反复插值导致画质越来越糊。
    QPixmap m_frames[2][2];
    /// 站住不动时用的正脸图（front.png），已经补到跟 m_frames 一样大。
    /// 它不属于左右两套帧，所以单独放。
    QPixmap m_frontPixmap;
    /// 当前正在显示的那张原图，是 m_frontPixmap 或 m_frames 里的某一张。
    QPixmap m_sourcePixmap;
    /// 当前缩放比例（1.0 表示图片原始大小）。
    qreal m_scale = 1.0;
    /// 是否正在拖动窗口。
    bool m_dragging = false;
    /// 按下鼠标那一刻，光标相对窗口左上角的偏移。
    QPoint m_dragOffset;

    /// 当前朝向，默认朝左（素材里的 left.png）。
    Facing m_facing = Facing::Left;
    /// 走动动画当前停在第几帧。
    int m_frameIndex = FrameStand;
    /// 当前是不是在走（有 WASD 键被按住）。
    bool m_walking = false;
    /// 当前按住的 WASD 键。
    QSet<int> m_pressedKeys;
    /// 平移窗口的节拍器，只在走路时跑。
    QTimer *m_moveTimer = nullptr;
    /// 切换走动帧的节拍器，只在走路时跑。
    QTimer *m_animTimer = nullptr;
    /// 桌宠旁边的说话气泡：独立的小窗口，显示当前压住的桌面图标名字。
    SpeechBubble *m_bubble = nullptr;
};

#endif // MAINWINDOW_H
