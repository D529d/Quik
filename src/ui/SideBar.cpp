#include "ui/SideBar.h"

#include "ui/Icons.h"
#include "ui/ThemeManager.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace studentai {

namespace {

// 导航项定义（顺序与 NavPage 枚举一致）
struct NavEntry {
    const char *title;
    QIcon (*icon)(const QColor &, int);
};

const NavEntry kNavEntries[] = {
    {"对话问答", &Icons::chat},
    {"拍照识题", &Icons::camera},
    {"笔记总结", &Icons::note},
    {"离线翻译", &Icons::globe},
    {"错题解析", &Icons::bulb},
    {"设置", &Icons::sliders},
};

constexpr int kWidthExpanded = 200; // 展开宽度
constexpr int kWidthCollapsed = 64; // 折叠宽度

} // namespace

SideBar::SideBar(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("sideBar"));
    setFixedWidth(kWidthExpanded);
    m_animValue = kWidthExpanded;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 10);
    layout->setSpacing(4);

    // ---- 顶部：Logo + 名称 + 副标题 ----
    m_logoArea = new QWidget(this);
    auto *logoLayout = new QHBoxLayout(m_logoArea);
    logoLayout->setContentsMargins(14, 16, 8, 12);
    logoLayout->setSpacing(10);

    auto *logoLabel = new QLabel(m_logoArea);
    logoLabel->setFixedSize(36, 36);
    logoLabel->setObjectName(QStringLiteral("appLogo"));
    logoLabel->setPixmap(Icons::logo(ThemeManager::instance()->accentColor(), 36)
                             .pixmap(36, 36));

    auto *nameBox = new QVBoxLayout;
    nameBox->setSpacing(1);
    m_appName = new QLabel(QStringLiteral("Quik"), m_logoArea);
    m_appName->setObjectName(QStringLiteral("appName"));
    m_appSub = new QLabel(QStringLiteral("轻量离线学习助手"), m_logoArea);
    m_appSub->setObjectName(QStringLiteral("appSub"));
    nameBox->addWidget(m_appName);
    nameBox->addWidget(m_appSub);

    logoLayout->addWidget(logoLabel, 0, Qt::AlignTop);
    logoLayout->addLayout(nameBox, 1);
    layout->addWidget(m_logoArea);

    // ---- 导航项：QToolButton 可复选组 ----
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    const int n = static_cast<int>(sizeof(kNavEntries) / sizeof(kNavEntries[0]));
    for (int i = 0; i < n; ++i) {
        auto *btn = new QToolButton(this);
        btn->setObjectName(QStringLiteral("navBtn"));
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setIconSize(QSize(20, 20));
        btn->setToolTip(QString::fromUtf8(kNavEntries[i].title));
        m_navGroup->addButton(btn, i);
        layout->addWidget(btn);
        m_navBtns.append(btn);
    }

    layout->addStretch(1);

    // ---- 底部：折叠 + 主题切换 ----
    auto *footRow = new QHBoxLayout;
    footRow->setContentsMargins(10, 0, 10, 0);
    footRow->setSpacing(2);

    m_foldBtn = new QToolButton(this);
    m_foldBtn->setObjectName(QStringLiteral("sideFootBtn"));
    m_foldBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_foldBtn->setIconSize(QSize(18, 18));
    m_foldBtn->setCursor(Qt::PointingHandCursor);
    m_foldBtn->setToolTip(QStringLiteral("折叠导航栏"));
    footRow->addWidget(m_foldBtn, 1);

    m_themeBtn = new QToolButton(this);
    m_themeBtn->setObjectName(QStringLiteral("sideFootBtn"));
    m_themeBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_themeBtn->setIconSize(QSize(18, 18));
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    footRow->addWidget(m_themeBtn, 1);

    layout->addLayout(footRow);

    // ---- 信号 ----
    connect(m_navGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit pageChanged(static_cast<NavPage>(id));
    });
    connect(m_foldBtn, &QToolButton::clicked, this, [this]() {
        setCollapsed(!m_collapsed);
        emit collapseToggled(m_collapsed);
    });
    connect(m_themeBtn, &QToolButton::clicked, this, &SideBar::themeToggleRequested);

    // 主题变化 -> 重着图标颜色
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const Theme &) { rebuildIcons(); });

    rebuildIcons();
    setCurrentPage(NavPage::Chat);
}

void SideBar::rebuildIcons() {
    ThemeManager *tm = ThemeManager::instance();
    const QColor normal = tm->iconColor();
    const int n = static_cast<int>(sizeof(kNavEntries) / sizeof(kNavEntries[0]));
    for (int i = 0; i < n; ++i) {
        m_navBtns[i]->setIcon(kNavEntries[i].icon(normal, 20));
    }

    // 折叠/主题按钮
    m_foldBtn->setIcon(Icons::chevronLeft(normal, 18));
    const bool dark = tm->theme().dark;
    m_themeBtn->setIcon(dark ? Icons::sun(normal, 18) : Icons::moon(normal, 18));
    m_themeBtn->setText(dark ? QStringLiteral("浅色") : QStringLiteral("深色"));
    m_foldBtn->setText(m_collapsed ? QStringLiteral("展开") : QStringLiteral("折叠"));
    applyNavMode();
}

void SideBar::applyNavMode() {
    // 折叠：仅图标模式 + tooltip；展开：图标 + 文字
    const int n = static_cast<int>(sizeof(kNavEntries) / sizeof(kNavEntries[0]));
    for (int i = 0; i < n; ++i) {
        QToolButton *btn = m_navBtns[i];
        btn->setToolButtonStyle(m_collapsed ? Qt::ToolButtonIconOnly
                                            : Qt::ToolButtonTextBesideIcon);
        btn->setText(m_collapsed ? QString()
                                 : QString::fromUtf8(kNavEntries[i].title));
        btn->setToolTip(QString::fromUtf8(kNavEntries[i].title));
    }
    m_foldBtn->setToolButtonStyle(m_collapsed ? Qt::ToolButtonIconOnly
                                              : Qt::ToolButtonTextBesideIcon);
    m_foldBtn->setText(m_collapsed ? QString() : QStringLiteral("折叠"));
    m_themeBtn->setToolButtonStyle(m_collapsed ? Qt::ToolButtonIconOnly
                                               : Qt::ToolButtonTextBesideIcon);

    m_appName->setVisible(!m_collapsed);
    m_appSub->setVisible(!m_collapsed);
}

void SideBar::setCollapsed(bool collapsed) {
    if (m_collapsed == collapsed) {
        return;
    }
    m_collapsed = collapsed;

    // 0.2s 缓动：宽度在 200 <-> 64 之间过渡
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(200);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(m_animValue);
    anim->setEndValue(m_collapsed ? kWidthCollapsed : kWidthExpanded);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_animValue = v.toInt();
        setFixedWidth(m_animValue);
    });
    connect(anim, &QVariantAnimation::finished, this, [this]() {
        // 动画结束后切换图标/文字模式（避免动画中途布局跳动）
        m_foldBtn->setText(m_collapsed ? QStringLiteral("展开") : QStringLiteral("折叠"));
        applyNavMode();
        rebuildIcons();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

NavPage SideBar::currentPage() const {
    return static_cast<NavPage>(m_navGroup->checkedId());
}

void SideBar::setCurrentPage(NavPage page) {
    QAbstractButton *btn = m_navGroup->button(static_cast<int>(page));
    if (btn) {
        btn->setChecked(true);
    }
}

bool SideBar::collapsed() const {
    return m_collapsed;
}

} // namespace studentai
