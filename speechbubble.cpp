#include "speechbubble.h"

#include "windowframe.h"

#include <QColor>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QVBoxLayout>

namespace {
// 气泡本体离桌宠留出的空隙，贴着图片显得挤。
constexpr int kGapPixels = 8;

// 配色：白底 + 深蓝的边框和文字。
const QColor kInkColor(0x1B, 0x3A, 0x6B);
const QColor kFillColor(Qt::white);
constexpr qreal kBorderWidth = 2.0;

// 文字离气泡本体的留白。本体是个椭圆，同样一行字在椭圆里能占的地方比在矩形里小，
// 所以左右得比一般的气泡留得宽一些，不然字的两头会戳出弧线外面。
constexpr int kPaddingHorizontal = 24;
constexpr int kPaddingVertical = 12;

// 尾巴：两节越来越小的椭圆，从气泡的角上斜着伸出去。
constexpr int kTailSpace = 18;      // 本体之外要留出的高度，不留尾巴会被窗口裁掉
constexpr int kTailInset = 18;      // 尾巴第一节的左边缘离窗口左边多远
constexpr int kTailOverlap = 8;     // 尾巴压在本体上的深度，压一点才连得住
constexpr int kTail1Width = 22;
constexpr int kTail1Height = 16;
constexpr int kTail2Width = 13;
constexpr int kTail2Height = 10;
constexpr int kTail2RightShift = 10;  // 第二节再往左挪一点，整条尾巴就是斜着出去的
constexpr int kTail2Overlap = 0;    // 第二节压在第一节上的深度
} // namespace

SpeechBubble::SpeechBubble(QWidget *parent)
    : QWidget(parent)
{
    // 纯展示用的窗口，所以额外要求两件事：
    //   - 鼠标事件穿过去落到桌面上，不然它盖住的地方点不动，拖桌宠也会被它挡住；
    //   - 不抢焦点。一旦焦点从桌宠手里被拿走，WASD 就不灵了。
    WindowFrame::makeFramelessToolWindow(
        this, Qt::Window | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);

    // 文字颜色走调色板，子控件会继承过去。这里只有字用得上颜色，
    // 背景和边框都是自己画的，没必要为此开样式表。
    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, kInkColor);
    setPalette(palette);

    m_label = new QLabel(this);
    // 图标名字里可能有 & 之类的字符，按纯文本显示，别当富文本解析。
    m_label->setTextFormat(Qt::PlainText);
    // 不折行也不省略：名字多长气泡就多宽。
    m_label->setWordWrap(false);
    m_label->setAlignment(Qt::AlignCenter);

    m_layout = new QVBoxLayout(this);
    m_layout->addWidget(m_label);
    // 先把尾巴那一侧的留白算上，否则第一条文字会被摆到尾巴的地盘上。
    updateLayoutMargins();

    hide();
}

void SpeechBubble::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    const QRectF body = bodyRect();
    const bool tailDown = (m_tailSide == TailSide::Bottom);

    // 两节尾巴：靠近本体的那节大一点，往外那节小一点、再往左挪一点。
    // 竖着画的时候两边镜像，所以这里把"朝下"和"朝上"的 y 分别算出来。
    const QRectF tail1(kTailInset,
                       tailDown ? body.bottom() - kTailOverlap
                                : body.top() + kTailOverlap - kTail1Height,
                       kTail1Width,
                       kTail1Height);
    const QRectF tail2(kTailInset + kTail2RightShift,
                       tailDown ? tail1.bottom() - kTail2Overlap
                                : tail1.top() + kTail2Overlap - kTail2Height,
                       kTail2Width,
                       kTail2Height);

    // 三个椭圆先合成一条闭合轮廓再描边。分开画的话，本体和尾巴相压的地方会留下
    // 一道多余的弧线，看起来像两个图形贴在一起，而不是长出来的一条尾巴。
    QPainterPath outline;
    outline.addEllipse(body);
    outline.addEllipse(tail1);
    outline.addEllipse(tail2);
    // 三个椭圆是互相压着的，得说明"重叠的地方也算在形状里"，
    // 否则按默认的奇偶填充规则，压住的那几块会被当成洞挖掉。
    outline.setFillRule(Qt::WindingFill);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(kFillColor);
    painter.setPen(QPen(kInkColor, kBorderWidth));
    painter.drawPath(outline.simplified());
}

void SpeechBubble::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 原生窗口句柄要到窗口真正显示出来的时候才存在，所以系统装饰只能在这里关。
    WindowFrame::applyNoSystemDecoration(this);
}

void SpeechBubble::setText(const QString &text)
{
    m_label->setText(text);
    // 窗口没边框，标题根本显示不出来；写在这里是为了调试时能从外部读到
    // 气泡此刻在说哪个图标（自动化验证只能拿到窗口标题，读不到画出来的字）。
    setWindowTitle(text);
    // 换了文字尺寸才变，这里把新尺寸落实下去，后面的定位就直接能用 width()/height()。
    adjustSize();
}

void SpeechBubble::setTailSide(TailSide side)
{
    if (m_tailSide == side)
        return;

    m_tailSide = side;
    updateLayoutMargins();
    // 留白变了，气泡的长宽也跟着变，所以要重新量一次。
    adjustSize();
}

void SpeechBubble::updateLayoutMargins()
{
    // 尾巴占掉的是窗口的一条边：尾巴在下就多留底边距，在上就多留顶边距。
    // 这样文字始终落在本体椭圆里，不会跑到尾巴的地盘上。
    const int top = kPaddingVertical + (m_tailSide == TailSide::Top ? kTailSpace : 0);
    const int bottom = kPaddingVertical + (m_tailSide == TailSide::Bottom ? kTailSpace : 0);
    m_layout->setContentsMargins(kPaddingHorizontal, top, kPaddingHorizontal, bottom);
}

QRectF SpeechBubble::bodyRect() const
{
    // 本体就是窗口去掉尾巴那一块之后剩下的矩形。
    const bool tailDown = (m_tailSide == TailSide::Bottom);
    return QRectF(0,
                  tailDown ? 0 : kTailSpace,
                  width(),
                  height() - kTailSpace);
}

void SpeechBubble::placeNear(const QRect &anchor, const QRect &screen)
{
    // 默认摆在桌宠正上方，尾巴朝下指着它。
    setTailSide(TailSide::Bottom);
    int y = anchor.top() - kGapPixels - height();

    if (y < screen.top()) {
        // 上面顶到屏幕了，就翻到桌宠正下方，尾巴跟着换个方向朝上指。
        setTailSide(TailSide::Top);
        y = anchor.bottom() + 1 + kGapPixels;
    }

    int x = anchor.center().x() - width() / 2;

    // 两边都夹回屏幕里。气泡比屏幕还宽或还高时 qBound 的上下界会反
    // （qMax 兜住这种情况，退回贴左上角，至少内容是可见的）。
    x = qBound(screen.left(), x, qMax(screen.left(), screen.right() - width() + 1));
    y = qBound(screen.top(), y, qMax(screen.top(), screen.bottom() - height() + 1));

    move(x, y);
}
