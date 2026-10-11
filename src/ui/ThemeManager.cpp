#include "ui/ThemeManager.h"

#include <QApplication>
#include <QPalette>

namespace studentai {

namespace {

// 生成 rgba() 字符串
QString rgba(const QColor &c, double alpha) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(alpha, 0, 'f', 2);
}

// 令牌替换：@key -> value
void fill(QString &s, const char *key, const QString &value) {
    s.replace(QLatin1String(key), value);
}

} // namespace

// ============================================================================
// buildStyleSheet：按主题参数生成全局 QSS
// ============================================================================
QString buildStyleSheet(const Theme &t) {
    const bool dark = t.dark;

    // ---- 基础色板 ----
    const QColor brandC  = t.brand;
    const QColor brandH  = dark ? brandC.lighter(118) : brandC.lighter(106);
    const QColor brandA  = dark ? brandC.darker(116)  : brandC.darker(112);
    const QString brand  = brandC.name(QColor::HexRgb);
    const QString brandHover  = brandH.name(QColor::HexRgb);
    const QString brandActive = brandA.name(QColor::HexRgb);
    const QString brandSoft   = rgba(brandC, dark ? 0.18 : 0.10); // 选中底
    const QString brandSofter = rgba(brandC, dark ? 0.10 : 0.06); // 悬浮底

    // 主题底色
    const QString bg      = dark ? QStringLiteral("#17171A") : QStringLiteral("#FFFFFF");
    const QString surface = dark ? QStringLiteral("#1E1E20") : QStringLiteral("#FFFFFF");
    const QString panelSolid = dark ? QStringLiteral("#1E1E20") : QStringLiteral("#F7F8FA");
    // 毛玻璃：半透明面板；关闭玻璃：实心面板
    const QString panel = t.glass ? rgba(QColor(dark ? "#1E1E20" : "#F7F8FA"), 0.82)
                                  : panelSolid;
    const QString statusBg = t.glass ? rgba(QColor(dark ? "#17171A" : "#F7F8FA"), 0.65)
                                     : panelSolid;

    const QString border   = dark ? QStringLiteral("rgba(255,255,255,0.08)")
                                  : QStringLiteral("rgba(29,33,41,0.08)");
    const QString text     = dark ? QStringLiteral("#F2F3F5") : QStringLiteral("#1D2129");
    const QString textSec  = dark ? QStringLiteral("#A9AEB8") : QStringLiteral("#86909C");
    const QString textTer  = dark ? QStringLiteral("#4E5969") : QStringLiteral("#C9CDD4");
    const QString rowHover = dark ? QStringLiteral("rgba(255,255,255,0.05)")
                                  : QStringLiteral("rgba(29,33,41,0.05)");
    const QString cardHover = rgba(brandC, dark ? 0.08 : 0.04);

    const QString aiBubble   = dark ? QStringLiteral("#26262A") : QStringLiteral("#F2F3F5");
    const QString aiBorder   = dark ? QStringLiteral("rgba(255,255,255,0.14)")
                                    : QStringLiteral("#E5E6EB");
    const QString scroll     = dark ? QStringLiteral("rgba(255,255,255,0.22)")
                                    : QStringLiteral("rgba(0,0,0,0.18)");
    const QString scrollHov  = dark ? QStringLiteral("rgba(255,255,255,0.34)")
                                    : QStringLiteral("rgba(0,0,0,0.30)");
    const QString shadowOnCard = rgba(QColor(0, 0, 0), dark ? 0.35 : 0.08);

    // ---- 密度（0紧凑 / 1标准 / 2宽松）----
    const int dFont  = (t.density == 0) ? 12 : (t.density == 2 ? 14 : 13);
    const int dPad   = (t.density == 0) ? 6  : (t.density == 2 ? 10 : 8);
    const int dItem  = (t.density == 0) ? 34 : (t.density == 2 ? 48 : 40);
    const int dSp    = (t.density == 0) ? 4  : (t.density == 2 ? 8  : 6);
    const int dBub   = (t.density == 0) ? 6  : (t.density == 2 ? 11 : 9);

    // AI 气泡样式二选一（填色 / 描边），运行时通过 [outline="true"] 动态属性切换
    const QString aiBubbleQss =
        t.aiBubbleFilled
            ? QStringLiteral("background-color: %1;").arg(aiBubble)
            : QStringLiteral("background-color: transparent; border: 1px solid %1;").arg(aiBorder);

    QString s = QStringLiteral(R"(
* {
    /* 中文字体置首，避免 Qt 仅选中首个字体族(如 Segoe UI)导致中文缺字形显示方框 */
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Segoe UI", sans-serif;
    font-size: @font px;
    color: @text;
    outline: none;
}
QMainWindow { background-color: @bg; color: @text; }
QToolTip {
    background-color: @surface; color: @text;
    border: 1px solid @border; border-radius: 6px; padding: 4px 8px;
}

/* ---- 滚动条：细、弱化存在感 ---- */
QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
QScrollBar::handle:vertical { background: @scroll; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: @scrollHov; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }
QScrollBar::handle:horizontal { background: @scroll; border-radius: 4px; min-width: 32px; }
QScrollBar::handle:horizontal:hover { background: @scrollHov; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* ---- 左侧导航栏 ---- */
QWidget#sideBar {
    background-color: @panel;
    border-right: 1px solid @border;
}
QLabel#appName { color: @text; font-size: 15px; font-weight: 600; }
QLabel#appSub  { color: @textSec; font-size: 11px; }
QToolButton#navBtn {
    background: transparent; color: @textSec;
    border: none; border-left: 2px solid transparent;
    border-radius: 6px;
    margin: 1px 10px;
    padding: @pad px 10px;
    text-align: left;
    font-size: @font px;
}
QToolButton#navBtn:hover { background-color: @rowHover; color: @text; }
QToolButton#navBtn:checked {
    background-color: @brandSoft; color: @brand;
    border-left: 2px solid @brand;
    font-weight: 600;
}
QToolButton#sideFootBtn {
    background: transparent; color: @textSec; border: none; border-radius: 6px;
    margin: 2px 10px; padding: @pad px 8px; text-align: left;
}
QToolButton#sideFootBtn:hover { background-color: @rowHover; color: @text; }

/* ---- 右侧历史面板 ---- */
QWidget#historyPanel  { background-color: @panel; border-left: 1px solid @border; }
QWidget#historyReopen { background-color: @panel; border-left: 1px solid @border; }
QLabel#panelTitle { color: @text; font-size: 15px; font-weight: 600; }
QPushButton#primaryBtn {
    background-color: @brand; color: #ffffff; border: none;
    border-radius: 6px; padding: 4px 12px; font-size: 12px; font-weight: 500;
}
QPushButton#primaryBtn:hover   { background-color: @brandHover; }
QPushButton#primaryBtn:pressed { background-color: @brandActive; }
QListWidget#historyList { background: transparent; border: none; }
QListWidget#historyList::item { border: none; margin: 0; }
QListWidget#historyList::item:selected { background: transparent; }
QFrame#sessionItem { background: transparent; border: none; border-left: 2px solid transparent; border-radius: 8px; }
QFrame#sessionItem:hover { background-color: @rowHover; }
QFrame#sessionItem[active="true"] { background-color: @brandSoft; border-left: 2px solid @brand; }
QLabel#sessionTitle { color: @text; font-size: 13px; }
QLabel#sessionTitle[active="true"] { color: @brand; font-weight: 600; }
QLabel#sessionTime { color: @textTer; font-size: 11px; }
QLabel#groupHeader { color: @textSec; font-size: 12px; font-weight: 600; padding: 8px 4px 2px 6px; }
QLabel#panelHint { color: @textSec; font-size: 12px; }
QToolButton#iconBtn {
    background: transparent; border: none; border-radius: 6px; color: @textSec;
}
QToolButton#iconBtn:hover { background-color: @rowHover; color: @text; }

/* ---- 聊天区 ---- */
QScrollArea { border: none; background: transparent; }
/* 聊天滚动区：显式指定自身与视口底色（视口名为 qt_scrollarea_viewport）。
   滚动区视口用的是 QPalette::Base，若不给它 QSS 规则，主题色不生效时会退回
   Qt/系统默认色，出现「深色模式聊天区仍是浅色」的问题。 */
QScrollArea#chatScroll,
QScrollArea#chatScroll > QWidget#qt_scrollarea_viewport { background-color: @bg; }
/* 主内容区：中央根容器 + 堆叠容器 + 各功能页背景统一跟随主题（深色下同步变深灰） */
QWidget#appRoot { background-color: @bg; }
QWidget#chatContainer, QStackedWidget#chatContainer { background-color: @bg; }
QWidget#mainContent { background-color: @bg; }
QWidget#emptyState, QWidget#emptyGrid { background: transparent; }
/* 聊天滚动区的内容容器：必须显式声明透明，否则主题切换后它会带着旧调色板
   被绘制成上一个主题的颜色，在聊天区中间显示为一块颜色不符的方块。 */
QWidget#chatContent { background: transparent; }
QLabel#welcomeTitle { color: @text; font-size: 22px; font-weight: 600; }
QLabel#welcomeSub   { color: @textSec; font-size: 13px; }
QFrame#quickCard {
    background-color: @surface; border: 1px solid @border; border-radius: 16px;
}
QFrame#quickCard:hover   { border: 1px solid @brand; background-color: @cardHover; }
QFrame#quickCard[pressed="true"] { background-color: @cardHover; }
QLabel#quickCardTitle { color: @text; font-size: 14px; font-weight: 600; }
QLabel#quickCardDesc  { color: @textSec; font-size: 12px; }
QLabel#quickCardIconBox { background-color: @brandSoft; border-radius: 10px; }
QWidget#msgRow { background: transparent; }

/* 用户气泡：品牌色填充，右下小切角 */
QFrame#bubbleUser {
    background-color: @brand; border-radius: 16px;
    border-bottom-right-radius: 4px;
}
QLabel#bubbleLabelUser { color: #ffffff; font-size: 14px; }
/* AI 气泡：填色 / 描边由 [outline] 属性切换，左下小切角 */
QFrame#bubbleAI {
    border-radius: 16px;
    border-bottom-left-radius: 4px;
    @aiBubble
}
QLabel#bubbleLabelAI { color: @text; font-size: 14px; }

/* ---- 深度思考区块（AI 气泡内：思考过程 + 正式答案 双区块）----
   排版规范：思考标题 15px/600、思考正文 14px 灰度斜体（行高 1.6 由富文本块给出）、
   答案正文 17px 标准正文字色（Markdown 渲染，行距取 Qt 富文本默认值）。 */
QWidget#thinkBlock  { background: transparent; }
QWidget#thinkContentWrap { background: transparent; }
QWidget#thinkHeader { background: transparent; }
QWidget#thinkHeader:hover { background-color: @rowHover; }
QLabel#thinkTitle { color: @textSec; font-size: 15px; font-weight: 600; }
QLabel#thinkText  { color: @textSec; font-size: 14px; font-style: italic; }
QToolButton#thinkToggle { background: transparent; border: none; }
QToolButton#thinkToggle:hover { background-color: @rowHover; }
QLabel#answerText { color: @text; font-size: 17px; }
/* 答案区占位提示：思考阶段显示，弱对比度、不可交互 */
QLabel#answerHint { color: @textTer; font-size: 13px; }
QLabel#aiAvatar {
    background-color: @brandSoft; color: @brand; border-radius: 12px;
    font-size: 10px; font-weight: 700;
}
QLabel#msgTime { color: @textTer; font-size: 11px; }

/* ---- 悬浮输入区 ---- */
QWidget#inputBarContainer { background: transparent; }
QFrame#inputCard {
    background-color: @surface;
    border: 1px solid @border; border-radius: 20px;
}
QFrame#inputCard[focused="true"] { border: 1px solid @brand; }
QTextEdit#chatInput {
    background: transparent; border: none; color: @text;
    font-size: 14px; selection-background-color: @brandSoft;
}
QPushButton#sendBtn {
    background-color: @brand; border: none; border-radius: 20px; color: #ffffff;
}
QPushButton#sendBtn:hover   { background-color: @brandHover; }
QPushButton#sendBtn:pressed { background-color: @brandActive; }
QToolButton#imageBtn { background: transparent; border: none; border-radius: 8px; }
QToolButton#imageBtn:hover { background-color: @rowHover; }

/* ---- 输入区图片附件预览（Step5）---- */
QWidget#attachRow { background: transparent; }
QLabel#attachThumb {
    background-color: @rowHover; border: 1px solid @border; border-radius: 8px;
}
QLabel#attachName { color: @textSec; font-size: 12px; }
QToolButton#attachClose { background: transparent; border: none; border-radius: 6px; }
QToolButton#attachClose:hover { background-color: @rowHover; }

/* ---- 聊天气泡内图片（Step5）---- */
QLabel#bubbleImage { border-radius: 6px; }

/* ---- 底部状态栏：极窄、半透明 ---- */
QStatusBar { background-color: @statusBg; border-top: 1px solid @border; }
QStatusBar QLabel { color: @textSec; font-size: 11px; padding: 0 12px; }

/* ---- 设置页 ---- */
QFrame#settingCard { background-color: @surface; border: 1px solid @border; border-radius: 16px; }
QLabel#settingTitle { color: @text; font-size: 15px; font-weight: 600; }
QLabel#settingLabel { color: @text; font-size: 13px; }
QLabel#settingHint  { color: @textSec; font-size: 12px; }
QComboBox {
    background-color: @surface; color: @text; border: 1px solid @border;
    border-radius: 8px; padding: 4px 10px; min-width: 110px;
}
QComboBox:hover { border-color: @brand; }
QComboBox::drop-down { border: none; width: 22px; }
/* 数值输入框（设置页的生成参数）与下拉框保持一致的观感 */
QDoubleSpinBox, QSpinBox {
    background-color: @surface; color: @text; border: 1px solid @border;
    border-radius: 8px; padding: 4px 8px;
}
QDoubleSpinBox:hover, QSpinBox:hover { border-color: @brand; }
QDoubleSpinBox:focus, QSpinBox:focus { border-color: @brand; }
QComboBox QAbstractItemView {
    background-color: @surface; color: @text; border: 1px solid @border;
    border-radius: 8px; selection-background-color: @brandSoft; selection-color: @brand;
    outline: none;
}
QCheckBox { color: @text; font-size: 13px; spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border: 1px solid @border; border-radius: 4px;
    background: transparent;
}
QCheckBox::indicator:checked { background-color: @brand; border-color: @brand; }
QCheckBox::indicator:hover { border-color: @brand; }
QPushButton#ghostBtn {
    background-color: transparent; color: @brand; border: 1px solid @brand;
    border-radius: 8px; padding: @pad px 14px; font-size: 13px;
}
QPushButton#ghostBtn:hover   { background-color: @brandSoft; }
QPushButton#ghostBtn:pressed { background-color: @brandSofter; }

/* 占位功能页 */
QLabel#pageTitle { color: @text; font-size: 20px; font-weight: 600; }
QLabel#pageDesc  { color: @textSec; font-size: 13px; }

/* ---- 工具功能页（笔记总结 / 离线翻译 / 错题解析）---- */
QLabel#funcTitle { color: @text; font-size: 20px; font-weight: 600; }
QLabel#funcSub   { color: @textSec; font-size: 13px; }
QLabel#funcLabel { color: @text; font-size: 13px; font-weight: 600; }
QTextEdit#funcInput {
    background-color: @surface; color: @text;
    border: 1px solid @border; border-radius: 10px; padding: 8px;
    font-size: 13px;
}
QTextEdit#funcInput:focus { border: 1px solid @brand; }
QTextEdit#funcResult {
    background-color: @surface; color: @text;
    border: 1px solid @border; border-radius: 10px; padding: 10px;
    font-size: 13px;
}
QPushButton#primaryBtn {
    background-color: @brand; color: #ffffff; border: none;
    border-radius: 8px; padding: 8px 22px; font-size: 13px; font-weight: 600;
}
QPushButton#primaryBtn:hover   { background-color: @brandHover; }
QPushButton#primaryBtn:pressed { background-color: @brandActive; }

/* 系统对话框：跟随主题，避免深色模式下浅色背景+浅色文字 */
QMessageBox { background-color: @surface; }
QMessageBox QLabel { color: @text; }
QMessageBox QPushButton {
    background-color: @surface; color: @text;
    border: 1px solid @border; border-radius: 6px;
    padding: 5px 16px; min-width: 60px;
}
QMessageBox QPushButton:hover { background-color: @rowHover; }
QFileDialog { background-color: @surface; }
QFileDialog QLabel, QFileDialog QComboBox, QFileDialog QLineEdit, QFileDialog QListView { color: @text; }
)");

    fill(s, "@font",        QString::number(dFont));
    fill(s, "@pad",         QString::number(dPad));
    fill(s, "@sp",          QString::number(dSp));
    fill(s, "@bub",         QString::number(dBub));
    fill(s, "@bg",          bg);
    fill(s, "@surface",     surface);
    fill(s, "@panel",       panel);
    fill(s, "@statusBg",    statusBg);
    fill(s, "@border",      border);
    // 注意：以下令牌存在前缀关系，必须先替换更长的，否则短令牌会先把长令牌
    // 破坏成非法值（例如 @text 会命中 @textSec 的前缀），导致该声明被 Qt 丢弃。
    fill(s, "@textSec",     textSec);
    fill(s, "@textTer",     textTer);
    fill(s, "@text",        text);
    fill(s, "@brandHover",  brandHover);
    fill(s, "@brandActive", brandActive);
    fill(s, "@brandSofter", brandSofter); // 必须在 @brandSoft 之前
    fill(s, "@brandSoft",   brandSoft);
    fill(s, "@brand",       brand);
    fill(s, "@rowHover",    rowHover);
    fill(s, "@cardHover",   cardHover);
    fill(s, "@aiBubble",    aiBubbleQss);
    fill(s, "@scrollHov",   scrollHov);
    fill(s, "@scroll",      scroll);
    fill(s, "@shadowCard",  shadowOnCard);
    return s;
}

// ============================================================================
// ThemeManager 单例
// ============================================================================
ThemeManager *ThemeManager::instance() {
    static ThemeManager s_instance;
    return &s_instance;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    m_theme = Theme();
}

Theme ThemeManager::theme() const {
    return m_theme;
}

void ThemeManager::setTheme(const Theme &theme) {
    const bool changed = (theme.dark != m_theme.dark
        || theme.brand != m_theme.brand
        || theme.density != m_theme.density
        || theme.glass != m_theme.glass
        || theme.aiBubbleFilled != m_theme.aiBubbleFilled);
    m_theme = theme;
    // 首次调用（样式未应用）也必须强制执行，保证启动即生效
    if (changed || !m_applied) {
        m_applied = true;
        qApp->setStyleSheet(buildStyleSheet(theme));

        // 完整覆盖应用调色板。仅设置 Window 是不够的：滚动区视口、文本类控件
        // 使用的是 QPalette::Base，若不在主题里覆盖，它们会退回系统/Qt 样式的
        // 默认色（浅色系统下恒为浅灰），于是深色主题下聊天区仍显示浅色，
        // 表现为「深色/浅色模式的聊天区底色相反」。
        const bool dark = theme.dark;
        const QColor fg   = dark ? QColor(0xF2, 0xF3, 0xF5) : QColor(0x1D, 0x21, 0x29);
        const QColor base = backgroundColor();
        QPalette pal = qApp->palette();
        pal.setColor(QPalette::Window,          base);
        pal.setColor(QPalette::Base,            base);   // 滚动区视口/输入框底色
        pal.setColor(QPalette::AlternateBase,   dark ? QColor(0x1E, 0x1E, 0x20)
                                                     : QColor(0xF7, 0xF8, 0xFA));
        pal.setColor(QPalette::WindowText,      fg);
        pal.setColor(QPalette::Text,            fg);
        pal.setColor(QPalette::Button,          base);
        pal.setColor(QPalette::ButtonText,      fg);
        pal.setColor(QPalette::ToolTipBase,     base);
        pal.setColor(QPalette::ToolTipText,     fg);
        pal.setColor(QPalette::PlaceholderText, dark ? QColor(0x4E, 0x59, 0x69)
                                                     : QColor(0xC9, 0xCD, 0xD4));
        pal.setColor(QPalette::Highlight,       theme.brand);
        pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
        qApp->setPalette(pal);

        emit themeChanged(theme);
    }
}

QColor ThemeManager::iconColor() const {
    return m_theme.dark ? QColor(0xC9, 0xCD, 0xD4) : QColor(0x4E, 0x59, 0x69);
}

QColor ThemeManager::accentColor() const {
    return m_theme.dark ? m_theme.brand.lighter(120) : m_theme.brand;
}

QColor ThemeManager::backgroundColor() const {
    return m_theme.dark ? QColor(0x17, 0x17, 0x1A) : QColor(0xFF, 0xFF, 0xFF);
}

} // namespace studentai
