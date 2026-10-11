#include "ui/ThinkBlock.h"

#include "ui/Icons.h"
#include "ui/ThemeManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace studentai {

namespace {

// ---- dotm-triangle-19「Pivot Ray」原始参数（与官方组件保持一致）----
constexpr int   kMatrix      = 7;    // 7x7 点阵
constexpr qreal kBaseOpacity = 0.08; // 掩码内最暗点
constexpr qreal kMidOpacity  = 0.38;
constexpr qreal kHighOpacity = 0.96; // 光束最亮点
constexpr int   kCenterRow   = 3;    // 轴心格（组件中的 heart cell）
constexpr int   kCenterCol   = 3;
constexpr qreal kBeamSigma   = 0.58; // 楔形光束宽度（弧度）
constexpr qreal kPi          = 3.14159265358979323846;

// 三角掩码内的 10 个点（行,列）：自上而下 1 + 2 + 3 + 4
constexpr int kTriangleCells[][2] = {
    {1, 3},
    {2, 2}, {2, 4},
    {3, 1}, {3, 3}, {3, 5},
    {4, 0}, {4, 2}, {4, 4}, {4, 6},
};

// 绘制尺寸：点直径 / 点间距 / 画布边长（保持 7 格均匀栅格）
constexpr qreal kDotSize = 3.2;
constexpr qreal kGap     = 1.3;
constexpr qreal kStride  = kDotSize + kGap;
constexpr int   kBox     = 30;

// 标题栏图标位固定为 30x30，动画与对勾切换时不会引起布局跳动
constexpr int kIconSlot = kBox;

// 完成态对勾颜色（深浅主题下都清晰）
const QColor kCheckGreen(0x34, 0xA8, 0x53);

bool isTriangleCell(int row, int col) {
    for (const auto &cell : kTriangleCells) {
        if (cell[0] == row && cell[1] == col) {
            return true;
        }
    }
    return false;
}

// 角度差，归一化到 (-pi, pi]
qreal angleDiff(qreal a, qreal b) {
    qreal d = a - b;
    while (d > kPi) {
        d -= 2.0 * kPi;
    }
    while (d < -kPi) {
        d += 2.0 * kPi;
    }
    return d;
}

qreal smoothstep01(qreal edge0, qreal edge1, qreal x) {
    if (edge1 <= edge0) {
        return x >= edge1 ? 1.0 : 0.0;
    }
    const qreal t = qBound(0.0, (x - edge0) / (edge1 - edge0), 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// 单点不透明度：轴心呼吸 + 绕轴旋转的高斯楔形光束 + 次级边缘提亮
qreal opacityForCell(int row, int col, qreal phase) {
    if (row == kCenterRow && col == kCenterCol) {
        const qreal hub = 0.5 + 0.5 * qSin(phase * 2.0 * kPi);
        return kMidOpacity + smoothstep01(0.12, 0.9, hub) * 0.22;
    }

    const qreal t = phase * 2.0 * kPi;
    const qreal ang = qAtan2(qreal(row - kCenterRow), qreal(col - kCenterCol));
    const qreal d = angleDiff(ang, t);
    const qreal beamRaw = qExp(-(d * d) / (kBeamSigma * kBeamSigma));
    const qreal beam = smoothstep01(0.05, 0.98, beamRaw);
    const qreal rim = 0.5 + 0.5 * qCos(ang * 2.0 - t * 1.15);
    const qreal accent = smoothstep01(0.45, 0.92, rim) * 0.18;

    return qMin(kHighOpacity,
                kBaseOpacity + (beam + accent) * (kHighOpacity - kBaseOpacity));
}

} // namespace

// ============================================================================
// DotMatrixLoader
// ============================================================================
DotMatrixLoader::DotMatrixLoader(QWidget *parent) : QWidget(parent) {
    setFixedSize(kBox, kBox);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_color = ThemeManager::instance()->accentColor();

    m_timer = new QTimer(this);
    m_timer->setInterval(16); // 约 60fps
    connect(m_timer, &QTimer::timeout, this, [this]() { tick(); });
}

void DotMatrixLoader::setDotColor(const QColor &color) {
    m_color = color;
    update();
}

void DotMatrixLoader::setCycleMs(int ms) {
    m_cycleMs = qMax(120, ms);
}

void DotMatrixLoader::start() {
    m_phase = 0.0;
    m_timer->start();
    update();
}

void DotMatrixLoader::stop() {
    m_timer->stop();
}

void DotMatrixLoader::tick() {
    m_phase += qreal(m_timer->interval()) / qreal(m_cycleMs);
    while (m_phase >= 1.0) {
        m_phase -= 1.0;
    }
    update();
}

void DotMatrixLoader::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    for (int row = 0; row < kMatrix; ++row) {
        for (int col = 0; col < kMatrix; ++col) {
            if (!isTriangleCell(row, col)) {
                continue; // 掩码外的格不绘制
            }
            const qreal opacity = opacityForCell(row, col, m_phase);
            if (opacity <= 0.001) {
                continue;
            }
            QColor c = m_color;
            c.setAlphaF(qBound(0.0, opacity, 1.0));
            p.setBrush(c);
            const qreal cx = col * kStride + kDotSize / 2.0;
            const qreal cy = row * kStride + kDotSize / 2.0;
            p.drawEllipse(QPointF(cx, cy), kDotSize / 2.0, kDotSize / 2.0);
        }
    }
}

// ============================================================================
// ThinkBlock
// ============================================================================
ThinkBlock::ThinkBlock(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("thinkBlock"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    // ---- 标题栏（整条可点击）----
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("thinkHeader"));
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->installEventFilter(this);
    auto *hdr = new QHBoxLayout(m_header);
    hdr->setContentsMargins(0, 2, 0, 2);
    hdr->setSpacing(8);

    // 图标位：思考中显示点阵动画，完成后换成绿色对勾
    m_loader = new DotMatrixLoader(m_header);
    hdr->addWidget(m_loader, 0, Qt::AlignVCenter);

    m_check = new QLabel(m_header);
    m_check->setFixedSize(kIconSlot, kIconSlot);
    m_check->setAlignment(Qt::AlignCenter);
    m_check->hide();
    hdr->addWidget(m_check, 0, Qt::AlignVCenter);

    m_title = new QLabel(QStringLiteral("Thinking..."), m_header);
    m_title->setObjectName(QStringLiteral("thinkTitle"));
    hdr->addWidget(m_title, 0, Qt::AlignVCenter);
    hdr->addStretch(1);

    m_toggle = new QToolButton(m_header);
    m_toggle->setObjectName(QStringLiteral("thinkToggle"));
    m_toggle->setAutoRaise(true);
    m_toggle->setFocusPolicy(Qt::NoFocus);
    m_toggle->setCursor(Qt::PointingHandCursor);
    m_toggle->setIconSize(QSize(16, 16));
    m_toggle->setToolTip(QStringLiteral("折叠 / 展开思考过程"));
    hdr->addWidget(m_toggle, 0, Qt::AlignVCenter);

    root->addWidget(m_header);

    // ---- 内容区（折叠动画作用在它的 maximumHeight 上）----
    m_contentWrap = new QWidget(this);
    m_contentWrap->setObjectName(QStringLiteral("thinkContentWrap"));
    auto *wrapLay = new QVBoxLayout(m_contentWrap);
    wrapLay->setContentsMargins(0, 0, 0, 0);
    wrapLay->setSpacing(0);

    m_content = new QLabel(m_contentWrap);
    m_content->setObjectName(QStringLiteral("thinkText"));
    m_content->setWordWrap(true);
    m_content->setTextFormat(Qt::RichText);
    m_content->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    wrapLay->addWidget(m_content);

    root->addWidget(m_contentWrap);

    // ---- 计时（0.1s 精度）----
    m_tick = new QTimer(this);
    m_tick->setInterval(100);
    connect(m_tick, &QTimer::timeout, this, [this]() { refreshElapsed(); });

    connect(m_toggle, &QToolButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
    });

    // 折叠动画复用同一个对象：避免快速连点时多个动画互相抢占 maximumHeight
    m_anim = new QPropertyAnimation(m_contentWrap, "maximumHeight", this);
    m_anim->setDuration(300);
    m_anim->setEasingCurve(QEasingCurve::OutBack); // iOS 式回弹
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_collapsed) {
            m_contentWrap->hide();
        }
        // 展开后交还布局自行计算高度，避免后续流式追加被旧高度卡住
        m_contentWrap->setMaximumHeight(QWIDGETSIZE_MAX);
    });

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const Theme &) { applyTheme(); });
    applyTheme();

    hide();
}

void ThinkBlock::applyTheme() {
    ThemeManager *tm = ThemeManager::instance();
    m_loader->setDotColor(tm->accentColor());
    const QColor iconC = tm->iconColor();
    m_toggle->setIcon(m_collapsed ? Icons::chevronRight(iconC, 16)
                                  : Icons::chevronDown(iconC, 16));
    m_check->setPixmap(Icons::check(kCheckGreen, 18).pixmap(18, 18));
}

void ThinkBlock::updateContentText() {
    // 推理正文是纯文本：转义后按换行拆行，并用块级 line-height 撑开行距
    QString body = m_thinkText.toHtmlEscaped();
    body.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    m_content->setText(QStringLiteral("<p style='margin:0; line-height:160%;'>")
                       + body + QStringLiteral("</p>"));
}

void ThinkBlock::refreshElapsed() {
    const double sec = double(m_elapsed.elapsed()) / 1000.0;
    m_title->setText(QStringLiteral("Thinking... %1s").arg(sec, 0, 'f', 1));
}

void ThinkBlock::begin() {
    m_everStarted = true;
    m_finished = false;
    m_thinkText.clear();
    updateContentText();

    m_elapsed.start();
    m_loader->show();
    m_loader->start();
    m_check->hide();
    m_title->setText(QStringLiteral("Thinking... 0.0s"));
    m_tick->start();

    // 默认展开（规范：思考区块默认保持展开，由用户手动折叠）
    m_collapsed = false;
    m_toggle->setIcon(Icons::chevronDown(ThemeManager::instance()->iconColor(), 16));
    m_contentWrap->show();
    m_contentWrap->setMaximumHeight(QWIDGETSIZE_MAX);

    show();
}

void ThinkBlock::appendThink(const QString &piece) {
    if (piece.isEmpty()) {
        return;
    }
    m_thinkText += piece;
    updateContentText();
}

void ThinkBlock::markAnswerStarted() {
    if (m_finished) {
        return;
    }
    m_finished = true;
    m_tick->stop();
    m_loader->stop();
    m_loader->hide();
    m_check->show();

    const double sec = double(m_elapsed.elapsed()) / 1000.0;
    m_title->setText(QStringLiteral("思考完成 用时 %1s").arg(sec, 0, 'f', 1));
}

void ThinkBlock::finish() {
    // 被用户中断等情况下没有走完 markAnswerStarted，这里兜底收尾
    if (m_everStarted && !m_finished) {
        markAnswerStarted();
    }
    m_loader->stop();
    m_tick->stop();
}

void ThinkBlock::showNoThink() {
    m_loader->stop();
    m_tick->stop();
    m_everStarted = false;
    hide();
}

void ThinkBlock::setCollapsed(bool collapsed, bool animated) {
    if (m_collapsed == collapsed) {
        return;
    }
    m_collapsed = collapsed;
    m_toggle->setIcon(collapsed
                          ? Icons::chevronRight(ThemeManager::instance()->iconColor(), 16)
                          : Icons::chevronDown(ThemeManager::instance()->iconColor(), 16));
    if (collapsed) {
        collapseAnimated(animated);
    } else {
        expandAnimated(animated);
    }
}

void ThinkBlock::expandAnimated(bool animated) {
    m_anim->stop();
    m_contentWrap->show();

    const int w = m_contentWrap->width();
    int target = (w > 0) ? m_content->heightForWidth(w) : -1;
    if (target <= 0) {
        target = m_content->sizeHint().height();
    }

    if (!animated) {
        m_contentWrap->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    m_anim->setStartValue(0);
    m_anim->setEndValue(target);
    m_anim->start();
}

void ThinkBlock::collapseAnimated(bool animated) {
    const int start = m_contentWrap->height();
    if (!animated) {
        m_anim->stop();
        m_contentWrap->hide();
        m_contentWrap->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    m_anim->stop();
    m_anim->setStartValue(start);
    m_anim->setEndValue(0);
    m_anim->start();
}

bool ThinkBlock::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton && m_header->rect().contains(me->pos())) {
                setCollapsed(!m_collapsed);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace studentai