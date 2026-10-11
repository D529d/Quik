#pragma once
// ============================================================================
// SideBar：左侧功能导航栏（可折叠，宽 200px / 收起 64px）
// - 顶部 Logo + 名称 + 副标题
// - 6 个导航项：线性图标 + 文字，选中态品牌色高亮（左侧 2px 竖条）
// - 底部：折叠按钮 + 浅/深色主题切换按钮
// 折叠时仅保留图标（ToolButtonIconOnly），宽度带 0.2s 缓动动画
// ============================================================================

#include <QWidget>
#include <QVector>

class QButtonGroup;
class QLabel;
class QToolButton;

namespace studentai {

struct Theme;

// 导航页标识（与导航项顺序一致）
enum class NavPage {
    Chat = 0,    // 对话问答
    PhotoQuiz,   // 拍照识题（P1）
    NoteSummary, // 笔记总结（P1）
    Translate,   // 离线翻译（P1）
    WrongQuiz,   // 错题解析（P1）
    Settings,    // 设置
};

class SideBar : public QWidget {
    Q_OBJECT
public:
    explicit SideBar(QWidget *parent = nullptr);

    NavPage currentPage() const;
    void setCurrentPage(NavPage page);
    bool collapsed() const;

signals:
    void pageChanged(NavPage page);       // 点击导航项
    void collapseToggled(bool collapsed); // 折叠 / 展开
    void themeToggleRequested();          // 点击主题切换按钮

private:
    void rebuildIcons();       // 按当前主题重新着色图标
    void setCollapsed(bool collapsed); // 折叠/展开（带动画）
    void applyNavMode();       // 同步折叠状态到按钮样式与文案

    QWidget *m_logoArea = nullptr;
    QLabel *m_appName = nullptr;
    QLabel *m_appSub = nullptr;

    QVector<QToolButton *> m_navBtns; // 与 NavPage 顺序一致
    QButtonGroup *m_navGroup = nullptr;

    QToolButton *m_foldBtn = nullptr;
    QToolButton *m_themeBtn = nullptr;

    bool m_collapsed = false;
    int m_animValue = 0; // 宽度动画当前值
};

} // namespace studentai
