#ifndef SPEECHBUBBLE_H
#define SPEECHBUBBLE_H

#include <QRect>
#include <QRectF>
#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

/**
 * 说话气泡：一个白色椭圆，角上带两节小尾巴，用来在桌宠旁边显示一条信息
 * （当前是桌面图标的名字）。边框和文字是深蓝色。
 *
 * 形状、尾巴、底色全部手画，不走样式表 —— 顶层 QWidget 的样式表背景得额外打开
 * WA_StyledBackground 才会画出来，而圆角和尾巴本来也不是样式表能直接表达的。
 *
 * 它是一个独立的顶层窗口，不是桌宠窗口的子控件 —— 子控件只能画在父窗口的矩形里，
 * 而气泡得能跑到桌宠外面去。分开还有个好处：气泡怎么变宽变高都不会牵动桌宠窗口，
 * 角色在屏幕上的位置自然就不动。
 */
class SpeechBubble : public QWidget
{
    Q_OBJECT

public:
    /// 尾巴挂在哪一侧。气泡摆在桌宠上方时尾巴朝下（落在气泡左下角），
    /// 摆在桌宠下方时尾巴朝上（落在气泡左上角）。
    enum class TailSide { Bottom, Top };

    explicit SpeechBubble(QWidget *parent = nullptr);

    /// 换上要显示的文字（就一条）。换完尺寸就更新好了，placeNear() 可以直接用。
    void setText(const QString &text);

    /// 把气泡摆到 anchor 的正上方并水平居中；上面顶到屏幕了就改摆到正下方，
    /// 尾巴会自动跟着换边。横向纵向都会夹在 screen 范围内，免得贴边时被切掉一半。
    void placeNear(const QRect &anchor, const QRect &screen);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// 换尾巴所在的一侧。尺寸会变（留白换了），所以换完要重新量一遍。
    void setTailSide(TailSide side);
    /// 按当前尾巴的位置，把这段留白摊到布局的上下边距上。
    void updateLayoutMargins();
    /// 气泡本体（不含尾巴）在窗口里占的矩形。
    QRectF bodyRect() const;

    QLabel *m_label = nullptr;
    QVBoxLayout *m_layout = nullptr;
    TailSide m_tailSide = TailSide::Bottom;
};

#endif // SPEECHBUBBLE_H
