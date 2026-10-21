#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "desktopicons.h"
#include "speechbubble.h"
#include "windowframe.h"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QMoveEvent>
#include <QPainter>
#include <QScreen>
#include <QSize>
#include <QTimer>

namespace {
// 窗口（也就是角色图片）的像素上下限：能缩到多小、放到多大，最终由实际像素数卡住，
// 而不是由缩放比例卡住，所以换一张别的素材也不用重算这些数。
constexpr int kMinSidePixels = 50;    // 最短边至少 50px，再小就没法用鼠标抓住了
constexpr int kMaxSidePixels = 1600;  // 最长边不超过 1600px，免得一放大就铺满整个屏幕
// 滚轮每滚一格的缩放倍率。
constexpr qreal kZoomStep = 1.1;
// 程序启动时的缩放比例（原图 800x1024，缩到一半刚好适合放在桌面上）。
constexpr qreal kInitialScale = 0.5;

// 走路时窗口每格平移的像素数，以及两格之间的间隔（4px / 15ms ≈ 267px 每秒）。
constexpr int kMoveStepPixels = 3;
constexpr int kMoveIntervalMs = 30;
// 走动动画每一帧停留多久。
constexpr int kAnimIntervalMs = 200;

// 是不是 WASD 四个走路键。这里比的是 Qt::Key_W 这类键码（认的是物理键位），
// 所以开着大写锁定、或者切到中文输入法，一样能走。
bool isWalkKey(int key)
{
    return key == Qt::Key_W || key == Qt::Key_A
        || key == Qt::Key_S || key == Qt::Key_D;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 无边框 + 始终置顶 + 不进任务栏 + 背景透明，跟旁边的说话气泡用同一套设置。
    WindowFrame::makeFramelessToolWindow(this);

    // QLabel 只负责显示图片：让它对鼠标事件完全透明，
    // 这样左键拖动、滚轮缩放都会直接落到窗口自己身上。
    ui->petLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->petLabel->setAlignment(Qt::AlignCenter);

    // 键盘 WASD 要生效，窗口自己得先能拿到焦点（点一下角色就行）。
    setFocusPolicy(Qt::StrongFocus);

    loadFrames();

    // 平移和换帧各用一个定时器，走路快慢和动画快慢互不干扰。
    m_moveTimer = new QTimer(this);
    m_moveTimer->setInterval(kMoveIntervalMs);
    connect(m_moveTimer, &QTimer::timeout, this, &MainWindow::stepMove);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(kAnimIntervalMs);
    connect(m_animTimer, &QTimer::timeout, this, &MainWindow::stepFrame);

    m_scale = kInitialScale;
    applyFrame();

    // 说话气泡。它是独立的顶层窗口（parent 只是用来管生命周期），
    // 所以它再怎么变宽变高都不会牵动桌宠窗口，角色在屏幕上的位置不会跟着动。
    m_bubble = new SpeechBubble(this);

    // 启动时把角色摆到屏幕右上角（用 availableGeometry 是为了避开任务栏）。
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        // available.right() 已经是"最右边那一列"的坐标，所以减掉宽度后要再 +1，
        // 否则窗口右边会正好压出屏幕一个像素。
        move(available.right() - width() + 1, available.top());
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateScaledPixmap()
{
    if (m_sourcePixmap.isNull())
        return;

    // 注意这里始终是从 m_sourcePixmap 重新缩放，
    // 而不是在上一次的结果上继续缩放，否则来回滚滚轮会让图片越来越模糊。
    const QSize targetSize = m_sourcePixmap.size() * m_scale;
    const QPixmap scaled = m_sourcePixmap.scaled(targetSize,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);

    ui->petLabel->setPixmap(scaled);

    // 窗口大小跟着图片走：既不会多出一圈透明区域挡住桌面上的点击，
    // 也能保证整张图片都在窗口范围内、能被鼠标点到。
    resize(scaled.size());
    ui->petLabel->setGeometry(rect());
}

void MainWindow::setScale(qreal scale)
{
    if (m_sourcePixmap.isNull())
        return;

    // 把像素上下限换算成缩放比例的上下限：
    // 短边不小于 kMinSidePixels，长边不超过 kMaxSidePixels。
    const QSize source = m_sourcePixmap.size();
    const qreal shortestSide = qMin(source.width(), source.height());
    const qreal longestSide = qMax(source.width(), source.height());
    scale = qBound(qreal(kMinSidePixels) / shortestSide,
                   scale,
                   qreal(kMaxSidePixels) / longestSide);

    if (qFuzzyCompare(scale, m_scale))
        return;

    // 以窗口中心为锚点缩放，否则窗口左上角固定不动，
    // 放大的时候角色会一直往右下角跑。
    const QPoint anchor = frameGeometry().center();

    m_scale = scale;
    updateScaledPixmap();

    move(anchor - QPoint(width() / 2, height() / 2));
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // 原生窗口句柄要到窗口真正显示出来的时候才存在，所以只能在这里关系统装饰。
    // 这个函数是幂等的，窗口反复显示隐藏也没关系。
    WindowFrame::applyNoSystemDecoration(this);

    // 显示的时候也读一次桌面：构造期间那次 move() 未必派发过移动事件。
    refreshBubble();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);

    // 位置一变就重新读一遍桌面图标。这既是"每次改变位置都读一次"，
    // 也是气泡能跟上来的原因 —— 气泡是独立窗口，不会跟着桌宠自动走。
    refreshBubble();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // 兜底：不管窗口因为什么原因变了大小，图片都铺满窗口（超出部分由 QLabel 居中裁剪）。
    ui->petLabel->setGeometry(rect());
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 点一下就把焦点抢过来，之后按 WASD 才有人接。
        activateWindow();
        setFocus(Qt::MouseFocusReason);

        m_dragging = true;
        // 记下光标在窗口内的落点，而不是光标自身的坐标。
        // 这样拖动时窗口不会"跳"一下，把左上角吸到光标上。
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        // 横向滚轮之类的不管，交给上层处理。
        event->ignore();
        return;
    }

    setScale(delta > 0 ? m_scale * kZoomStep : m_scale / kZoomStep);
    event->accept();
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    // 窗口没有边框、也不在任务栏里，所以留一个右键菜单用来退出。
    QMenu menu(this);
    QAction *resetAction = menu.addAction(QStringLiteral("恢复默认大小"));
    menu.addSeparator();
    QAction *quitAction = menu.addAction(QStringLiteral("退出"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == quitAction)
        QApplication::quit();
    else if (chosen == resetAction)
        setScale(kInitialScale);
}

void MainWindow::loadFrames()
{
    // 四个素材的路径，以及它们在 m_frames 里该待的位置：
    // 第一个下标是朝向（Facing），第二个下标是帧（Frame）。
    const struct {
        const char *path;
        int facing;
        int frame;
    } files[] = {
        {":/images/whale/left.png",         int(Facing::Left),  FrameStand},
        {":/images/whale/left_stride.png",  int(Facing::Left),  FrameStride},
        {":/images/whale/right.png",        int(Facing::Right), FrameStand},
        {":/images/whale/right_stride.png", int(Facing::Right), FrameStride},
    };

    QSize referenceSize;
    for (const auto &file : files) {
        const QString path = QString::fromLatin1(file.path);
        const QPixmap pixmap(path);

        if (pixmap.isNull()) {
            qWarning() << QStringLiteral("角色图片加载失败：") << path;
        } else if (referenceSize.isEmpty()) {
            referenceSize = pixmap.size();
        } else if (pixmap.size() != referenceSize) {
            // 尺寸不一致的话，走动切帧时窗口会跟着一跳一跳，先提醒一句。
            qWarning() << QStringLiteral("角色图片尺寸不一致：") << path << pixmap.size()
                       << QStringLiteral("，其他图是") << referenceSize;
        }

        m_frames[file.facing][file.frame] = pixmap;
    }

    // 站住不动时用的正脸图。它不属于左右两套帧（朝哪个方向都一样），所以单独存一份。
    m_frontPixmap = QPixmap(QStringLiteral(":/images/whale/front.png"));
    if (m_frontPixmap.isNull()) {
        qWarning() << QStringLiteral("角色图片加载失败：:/images/whale/front.png");
        return;
    }

    // front.png 跟左右两套帧不是同一批出的图，尺寸对不上（300x360 对 800x1024）。
    // 直接换上去的话，站住和走动之间切换时窗口会猛地跳一下大小，
    // 所以先按比例缩放到参照尺寸，再居中铺到一张同样大小的透明画布上。
    if (!referenceSize.isEmpty() && m_frontPixmap.size() != referenceSize) {
        const QPixmap scaled = m_frontPixmap.scaled(referenceSize,
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation);
        QPixmap canvas(referenceSize);
        canvas.fill(Qt::transparent);

        QPainter painter(&canvas);
        painter.drawPixmap((referenceSize.width() - scaled.width()) / 2,
                           (referenceSize.height() - scaled.height()) / 2,
                           scaled);
        painter.end();

        m_frontPixmap = canvas;
    }
}

void MainWindow::applyFrame()
{
    // 站住不动时统一用正脸图，跟朝哪个方向无关；走起来才回到左右两套帧里，
    // 按朝向在站立/迈步之间来回切。front.png 没读到时退回原来的站立帧。
    if (!m_walking && !m_frontPixmap.isNull())
        m_sourcePixmap = m_frontPixmap;
    else
        m_sourcePixmap = m_frames[int(m_facing)][m_walking ? m_frameIndex : int(FrameStand)];

    if (m_sourcePixmap.isNull())
        return;

    updateScaledPixmap();
}

void MainWindow::setFacing(Facing facing)
{
    if (m_facing == facing)
        return;

    m_facing = facing;
    applyFrame();
}

void MainWindow::updateWalkState()
{
    if (!m_moveTimer || !m_animTimer)
        return;

    const bool walking = !m_pressedKeys.isEmpty();
    if (walking == m_walking)
        return;

    m_walking = walking;
    if (m_walking) {
        m_moveTimer->start();
        m_animTimer->start();
    } else {
        m_moveTimer->stop();
        m_animTimer->stop();
        m_frameIndex = FrameStand; // 停下来就回到站立帧
    }
    applyFrame();
}

void MainWindow::stepMove()
{
    int dx = 0;
    int dy = 0;
    if (m_pressedKeys.contains(Qt::Key_A))
        dx -= kMoveStepPixels;
    if (m_pressedKeys.contains(Qt::Key_D))
        dx += kMoveStepPixels;
    if (m_pressedKeys.contains(Qt::Key_W))
        dy -= kMoveStepPixels;
    if (m_pressedKeys.contains(Qt::Key_S))
        dy += kMoveStepPixels;

    // 朝向只看左右：上下移动时 dx 是 0，所以朝向原样不动。
    if (dx < 0)
        setFacing(Facing::Left);
    else if (dx > 0)
        setFacing(Facing::Right);

    if (dx == 0 && dy == 0)
        return;

    QPoint target = frameGeometry().topLeft() + QPoint(dx, dy);

    // 别让角色走出屏幕：缩小之后一旦跑到屏幕外，就再也点不到了。
    if (const QScreen *scr = screen()) {
        const QRect available = scr->availableGeometry();
        target.setX(qBound(available.left(), target.x(), available.right() - width() + 1));
        target.setY(qBound(available.top(), target.y(), available.bottom() - height() + 1));
    }

    move(target);
}

void MainWindow::stepFrame()
{
    m_frameIndex = (m_frameIndex == FrameStand) ? FrameStride : FrameStand;
    applyFrame();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // F 键：打开跟桌宠重合的那个桌面图标，等同于在桌面上双击它。
    if (event->key() == Qt::Key_F) {
        // 按住不放会一直补发 press，只认第一下，不然图标会被连续打开好几次。
        if (!event->isAutoRepeat())
            openIconUnderPet();
        event->accept();
        return;
    }

    if (!isWalkKey(event->key())) {
        QWidget::keyPressEvent(event);
        return;
    }

    // 按住不放时系统会一直补发 press，这里只认第一次按下的那一下。
    if (!event->isAutoRepeat())
        m_pressedKeys.insert(event->key());

    updateWalkState();
    event->accept();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (!isWalkKey(event->key())) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    // 有些平台在"连按"期间也会补发 release，滤掉它们，否则角色会一走一停。
    if (!event->isAutoRepeat())
        m_pressedKeys.remove(event->key());

    updateWalkState();
    event->accept();
}

void MainWindow::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);

    // 焦点没了就再也收不到 release 了，不清空的话角色会一直走到天荒地老。
    if (!m_pressedKeys.isEmpty()) {
        m_pressedKeys.clear();
        updateWalkState();
    }
}

std::optional<DesktopIcon> MainWindow::iconUnderPet() const
{
    // 重新读一遍桌面：图标可能被别的东西挪过位置，也可能刚好被桌宠压住了。
    const QVector<DesktopIcon> icons = DesktopIcons::enumerate();

    // 窗口是不透明的整张图片，所以直接用窗口矩形跟图标的位置比。
    // 这里挑"重合面积最大"而不是"只要相交就算"：小角色常常同时压住两个图标，
    // 只按相交挑的话选中谁取决于图标的排列顺序，看着就像在随机翻牌子。
    const QRect petRect = frameGeometry();
    std::optional<DesktopIcon> target;
    qint64 bestArea = 0;
    for (const DesktopIcon &icon : icons) {
        if (icon.name.isEmpty())
            continue;

        // 两边都是逻辑像素，可以直接比。
        const QRect overlap = petRect.intersected(icon.rect);
        const qint64 area = qint64(overlap.width()) * overlap.height();
        if (area > bestArea) {
            bestArea = area;
            target = icon;
        }
    }

    return target;
}

void MainWindow::refreshBubble()
{
    if (m_bubble == nullptr)
        return;

    // 跟按 F 用的是同一个函数，所以气泡上写的是谁，按 F 打开的就是谁。
    const std::optional<DesktopIcon> target = iconUnderPet();
    if (!target) {
        // 没压住任何图标就不显示气泡，免得桌上一直挂着个白方块。
        m_bubble->hide();
        return;
    }

    // 一次只显示这一条，不再把压住的图标全列出来。
    m_bubble->setText(target->name);

    const QRect petRect = frameGeometry();
    // 按屏幕可用区域来摆，不然贴着屏幕上边缘时气泡会被切掉一半。
    const QScreen *currentScreen = screen();
    const QRect area = currentScreen != nullptr ? currentScreen->availableGeometry() : petRect;
    m_bubble->placeNear(petRect, area);
    m_bubble->show();
}

void MainWindow::openIconUnderPet()
{
    // 桌宠压在桌面图标上的时候，按 F 就等于双击那个图标。
    // 注意这里挑图标用的是 iconUnderPet()，跟气泡显示的是同一个 ——
    // 否则角色同时压住两个图标时，会出现"气泡写着这个、F 打开那个"的错位。
    const std::optional<DesktopIcon> target = iconUnderPet();
    if (!target) {
        qWarning() << QStringLiteral("桌宠没有压在任何桌面图标上（或者读不到桌面图标），按 F 没有反应。");
        return;
    }

    if (!DesktopIcons::open(*target))
        qWarning() << QStringLiteral("打不开桌面图标：") << target->name;
}
