#pragma once
// ============================================================================
// ThemeManager：主题管理（轻玻璃拟态 · 浅色/深色双主题）
// ----------------------------------------------------------------------------
// 设计准则（对标主流 AI 软件）：
//   - 品牌主色科技蓝 #165DFF，低饱和辅助色，圆角体系（控件12px/卡片16-20px）
//   - 柔和弥散阴影、悬浮分层、克制动效（0.2~0.3s）
//   - 全局 QSS 由 buildStyleSheet() 按主题参数动态生成（QSS 不支持 CSS 变量，
//     故采用"令牌替换"方式注入颜色/间距）
// 用法：
//   ThemeManager::instance()->setTheme(theme); // 应用全局 QSS + 广播 themeChanged
//   各控件连接 themeChanged 信号刷新线性图标颜色
// ============================================================================

#include <QColor>
#include <QObject>
#include <QString>

namespace studentai {

// 主题参数（设置页「外观」板块可调）
struct Theme {
    bool dark = false;            // 浅色 / 深色
    QColor brand = QColor(0x16, 0x5D, 0xFF); // 品牌主色（科技蓝）
    int   density = 1;            // 0=紧凑 1=标准 2=宽松
    bool  glass = true;           // 毛玻璃透明度（半透明面板）
    bool  aiBubbleFilled = true;  // AI 气泡样式：true 填色 / false 描边
};

// 按主题参数生成全局 QSS 样式表
QString buildStyleSheet(const Theme &theme);

// 主题管理器：单例，负责应用全局样式并通知各控件刷新图标
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager *instance();

    Theme theme() const;
    // 应用全局 QSS 并广播 themeChanged（各控件据此重新着色图标）
    void setTheme(const Theme &theme);

    // 当前主题下的常用颜色（供代码绘制/图标使用）
    QColor iconColor() const;        // 常规图标（灰）
    QColor accentColor() const;      // 品牌色（高亮/强调）
    QColor backgroundColor() const;  // 主内容区底色（深色 #17171A / 浅色 #FFFFFF）

signals:
    void themeChanged(const Theme &theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);

    Theme m_theme;
    bool m_applied = false; // 样式是否已应用过（首次必须强制应用）
};

} // namespace studentai
