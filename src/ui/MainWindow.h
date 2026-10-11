#pragma once
// ============================================================================
// MainWindow：主窗口（三栏式 AI 经典布局）
//   左：导航栏(可折叠) | 中：QStackedWidget 各功能页 | 右：历史面板(可收起)
//   底部：极窄半透明状态栏（模型名 / 速度 / 内存）
// 主题：浅色/深色 + 品牌色/密度/毛玻璃/气泡样式，由 ThemeManager 统一分发
// ============================================================================

#include <QMainWindow>
#include <QVector>
#include <memory>

#include "ui/SideBar.h" // NavPage
#include "ui/ThemeManager.h" // Theme（m_theme 按值存储）

class QAbstractNativeEventFilter;
class QLabel;
class QStackedWidget;
class QToolButton;

namespace studentai {

class LLMEngine;
class ChatWidget;
class ChatInputBar;
class HistoryPanel;
class HistoryManager;
class SettingsPage;
class FunctionPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // modelPathOverride: 可选，命令行传入的模型路径（优先级最高）
    explicit MainWindow(const QString &modelPathOverride = QString(),
                        QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void buildUi();        // 三栏布局 + 状态栏 + 堆叠页
    void applyTheme();     // 从 QSettings 载入主题并应用
    void saveTheme();      // 主题写入 QSettings
    // 从 QSettings 载入生成参数（温度/Top-P/上下文长度）并应用到引擎
    void applyGenParams();
    void connectSignals(); // 引擎信号 <-> UI
    void loadModel(const QString &overridePath = QString());
    // 加载多模态视觉编码器（mmproj，Step5）；未配置则静默跳过
    void loadMmproj(const QString &overridePath = QString());
    // 在程序目录 models\ 下查找随包附带的模型文件（便携版解压即用）
    QString findBundledModel(bool wantMmproj) const;
    void refreshMemory();  // 刷新状态栏内存显示

    // 功能占位页（P1 功能后续实现）
    QWidget *makePlaceholderPage(NavPage page);

    // ---- 历史记录（Step4）----
    void openDatabase();
    void refreshSessionList();
    void newSession();
    void openSession(qint64 sessionId);
    void deleteSession(qint64 sessionId);

    // 历史面板收起 / 展开 ----
    void setHistoryCollapsed(bool collapsed);
    // 切换功能页；「拍照识题」已并入对话页（跳转并唤起图片选择）
    void goToPage(NavPage page);

    // ---- 工具功能页（Step6：笔记总结 / 离线翻译 / 错题解析）----
    void wireFunctionPage(FunctionPage *page);        // 连接该页的请求信号
    void runFunctionPage(FunctionPage *page, const QString &prompt); // 发起一次性生成

    // ---- 导出会话为 Markdown（P2）----
    void exportCurrentSession();

    // ---- 全局快捷键唤起（P2）----
    void setupGlobalHotkey();  // 注册 Ctrl+Alt+A
    void summonWindow();       // 唤起并激活主窗口

    LLMEngine *m_engine = nullptr;
    ChatWidget *m_chat = nullptr;
    ChatInputBar *m_inputBar = nullptr;
    SideBar *m_sideBar = nullptr;
    HistoryPanel *m_history = nullptr;
    HistoryManager *m_historyMgr = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    FunctionPage *m_notePage = nullptr;      // 笔记总结
    FunctionPage *m_translatePage = nullptr; // 离线翻译
    FunctionPage *m_wrongPage = nullptr;     // 错题解析
    // 正在生成的功能页（nullptr 表示 token 流向聊天区）
    FunctionPage *m_activeFunction = nullptr;

    QStackedWidget *m_pages = nullptr;   // 各功能页堆叠
    QWidget *m_historyReopen = nullptr;  // 历史面板收起后的展开条
    QToolButton *m_reopenBtn = nullptr;

    qint64 m_currentSessionId = -1; // 当前会话 id（-1 = 无/新会话）
    Theme m_theme;                  // 当前主题（源数据，持久化）

    bool m_historyCollapsed = false; // 历史面板是否已收起

    QLabel *m_statusModel = nullptr; // 模型名称
    QLabel *m_statusSpeed = nullptr; // 生成速度
    QLabel *m_statusMem = nullptr;   // 进程内存

    // 全局快捷键：Windows 下通过 RegisterHotKey 注册系统级热键
    std::unique_ptr<QAbstractNativeEventFilter> m_hotkeyFilter;
    quintptr m_hotkeyHwnd = 0; // 注册热键时使用的窗口句柄（注销时用）
};

} // namespace studentai
