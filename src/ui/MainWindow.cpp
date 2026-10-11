#include "ui/MainWindow.h"

#include "core/HistoryManager.h"
#include "core/LLMEngine.h"
#include "core/PromptTemplates.h"
#include "core/ThinkStreamParser.h"
#include "ui/ChatInputBar.h"
#include "ui/ChatWidget.h"
#include "ui/FunctionPage.h"
#include "ui/HistoryPanel.h"
#include "ui/Icons.h"
#include "ui/SettingsPage.h"
#include "ui/ThemeManager.h"
#include "utils/SystemInfo.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <QAbstractNativeEventFilter>

#include <functional>

#ifdef Q_OS_WIN
// 全局快捷键需要 Win32 的 RegisterHotKey；先定义这两个宏避免与 Qt 冲突
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace studentai {

namespace {

// 导航页中文名（占位页/提示用）
QString navPageName(NavPage page) {
    switch (page) {
        case NavPage::Chat:        return QStringLiteral("对话问答");
        case NavPage::PhotoQuiz:   return QStringLiteral("拍照识题");
        case NavPage::NoteSummary: return QStringLiteral("笔记总结");
        case NavPage::Translate:   return QStringLiteral("离线翻译");
        case NavPage::WrongQuiz:   return QStringLiteral("错题解析");
        case NavPage::Settings:    return QStringLiteral("设置");
    }
    return QStringLiteral("未知功能");
}

// 占位页图标（与导航页对应，P1 功能后续实现）
QIcon (*placeholderIcon(NavPage page))(const QColor &, int) {
    switch (page) {
        case NavPage::PhotoQuiz:   return &Icons::camera;
        case NavPage::NoteSummary: return &Icons::note;
        case NavPage::Translate:   return &Icons::globe;
        case NavPage::WrongQuiz:   return &Icons::bulb;
        default:                   return &Icons::chat;
    }
}

} // namespace

// ============================================================================
// 工具功能页配置（Step6）
// 提示词统一由 PromptTemplates 组装，页面只负责收集输入与展示结果
// ============================================================================
namespace {

FunctionPage::Config noteSummaryConfig() {
    FunctionPage::Config c;
    c.title = QStringLiteral("笔记总结");
    c.subtitle = QStringLiteral(
        "把课堂笔记或长段落粘进来，按「核心要点 / 重要细节 / 应用场景」生成结构化总结");
    c.inputLabel = QStringLiteral("笔记内容");
    c.inputPlaceholder = QStringLiteral("把要总结的笔记粘贴到这里…");
    c.runText = QStringLiteral("生成总结");
    c.buildPrompt = [](const QString &text, const QString &) {
        return PromptTemplates::noteSummary(text);
    };
    return c;
}

FunctionPage::Config translateConfig() {
    FunctionPage::Config c;
    c.title = QStringLiteral("离线翻译");
    c.subtitle = QStringLiteral(
        "中英互译，自动判断方向（中文→英文 / 英文→中文），保持原文格式，专业术语不乱翻");
    c.inputLabel = QStringLiteral("原文");
    c.inputPlaceholder = QStringLiteral("粘贴要翻译的中文或英文…");
    c.runText = QStringLiteral("翻译");
    c.buildPrompt = [](const QString &text, const QString &) {
        return PromptTemplates::translate(text);
    };
    return c;
}

FunctionPage::Config wrongQuizConfig() {
    FunctionPage::Config c;
    c.title = QStringLiteral("错题解析");
    c.subtitle = QStringLiteral(
        "填上题目和你当时的答案，按「错误原因 / 正确解法 / 知识点总结」帮你分析");
    c.inputLabel = QStringLiteral("题目");
    c.inputPlaceholder = QStringLiteral("粘贴题目，可带选项…");
    c.secondLabel = QStringLiteral("我的答案（可留空）");
    c.secondPlaceholder = QStringLiteral("写下你当时的解答或所选选项…");
    c.runText = QStringLiteral("分析错因");
    c.buildPrompt = [](const QString &question, const QString &answer) {
        return PromptTemplates::wrongQuiz(question, answer);
    };
    return c;
}

} // namespace

// ============================================================================
// 历史记录中的图片消息编码（Step5）
// 用户消息含图片时，将图片路径以 <<IMG:路径>> 前缀一并存入 SQLite，
// 重新打开会话时据此还原缩略图；纯文本消息不做任何包装。
// ============================================================================
namespace {

const QString kImagePrefix = QStringLiteral("<<IMG:");

QString encodeUserContent(bool withImage, const QString &imagePath, const QString &text) {
    if (!withImage) {
        return text;
    }
    return kImagePrefix + imagePath + QStringLiteral(">>") + text;
}

// 解析用户消息；outText 为去除图片标记后的正文，返回图片路径（无图则空）
QString decodeUserContent(const QString &content, QString *outText) {
    if (!content.startsWith(kImagePrefix)) {
        *outText = content;
        return QString();
    }
    const int end = content.indexOf(QStringLiteral(">>"), kImagePrefix.size());
    if (end < 0) {
        *outText = content;
        return QString();
    }
    *outText = content.mid(end + 2);
    return content.mid(kImagePrefix.size(), end - kImagePrefix.size());
}

} // namespace

// ============================================================================
// 全局快捷键（P2）：Windows 下用 RegisterHotKey 注册系统级热键 Ctrl+Alt+A
// ============================================================================
namespace {

#ifdef Q_OS_WIN
constexpr int kHotkeyId = 0x5A01; // 本进程内唯一的固定 id

// Qt 会把原生消息转发给已安装的过滤器；这里捕获 WM_HOTKEY 后触发回调
class HotkeyEventFilter : public QAbstractNativeEventFilter {
public:
    explicit HotkeyEventFilter(std::function<void()> callback)
        : m_callback(std::move(callback)) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
            auto *msg = static_cast<MSG *>(message);
            if (msg != nullptr && msg->message == WM_HOTKEY
                && static_cast<int>(msg->wParam) == kHotkeyId) {
                if (m_callback) {
                    m_callback();
                }
                return true;
            }
        }
        return false;
    }

private:
    std::function<void()> m_callback;
};
#endif

} // namespace

MainWindow::MainWindow(const QString &modelPathOverride, QWidget *parent)
    : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Quik - 轻量离线学习助手"));
    resize(1240, 820);
    setMinimumSize(980, 640);

    buildUi();
    applyTheme();
    applyGenParams();
    connectSignals();
    openDatabase();
    loadModel(modelPathOverride);
    setupGlobalHotkey();

    // 状态栏内存定时刷新（每 2 秒一次）
    auto *memTimer = new QTimer(this);
    connect(memTimer, &QTimer::timeout, this, &MainWindow::refreshMemory);
    memTimer->start(2000);
}

MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
    // 注销系统级热键，避免占用被残留到进程退出之后
    if (m_hotkeyHwnd != 0) {
        UnregisterHotKey(reinterpret_cast<HWND>(m_hotkeyHwnd), kHotkeyId);
    }
#endif
    if (m_hotkeyFilter) {
        QApplication::instance()->removeNativeEventFilter(m_hotkeyFilter.get());
    }
    // m_engine 由 Qt 父子关系自动析构，析构时等待推理线程退出
}

void MainWindow::buildUi() {
    m_engine = new LLMEngine(this);
    m_chat = new ChatWidget(this);
    m_inputBar = new ChatInputBar(this);
    m_sideBar = new SideBar(this);
    m_history = new HistoryPanel(this);
    m_settingsPage = new SettingsPage(this);

    // ---- 中央区：功能页堆叠 ----
    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("chatContainer"));
    // 纯 QWidget 默认不绘制背景：autoFillBackground 用调色板 Window 色填充，
    // 配合 QSS 的 #chatContainer 规则（polish 会把调色板设为 @bg）保证深色下跟随主题
    m_pages->setAutoFillBackground(true);

    // 0. 对话问答页：聊天区 + 悬浮输入区
    auto *chatPage = new QWidget(m_pages);
    chatPage->setObjectName(QStringLiteral("mainContent")); // 背景随主题（@bg）
    chatPage->setAutoFillBackground(true);
    auto *chatLayout = new QVBoxLayout(chatPage);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);
    chatLayout->addWidget(m_chat, 1);
    chatLayout->addWidget(m_inputBar, 0);
    m_pages->addWidget(chatPage);

    // 1. 拍照识题：入口已并入对话页（见 goToPage），保留占位以维持页索引与导航枚举一致
    m_pages->addWidget(makePlaceholderPage(NavPage::PhotoQuiz));

    // 2~4. 工具功能页（Step6：笔记总结 / 离线翻译 / 错题解析）
    m_notePage = new FunctionPage(noteSummaryConfig(), m_pages);
    m_pages->addWidget(m_notePage);
    m_translatePage = new FunctionPage(translateConfig(), m_pages);
    m_pages->addWidget(m_translatePage);
    m_wrongPage = new FunctionPage(wrongQuizConfig(), m_pages);
    m_pages->addWidget(m_wrongPage);

    wireFunctionPage(m_notePage);
    wireFunctionPage(m_translatePage);
    wireFunctionPage(m_wrongPage);

    // 5. 设置页
    m_pages->addWidget(m_settingsPage);

    // ---- 历史面板收起后的展开条（窄条 + 按钮）----
    m_historyReopen = new QWidget(this);
    m_historyReopen->setObjectName(QStringLiteral("historyReopen"));
    m_historyReopen->setFixedWidth(40);
    auto *reopenLayout = new QVBoxLayout(m_historyReopen);
    reopenLayout->setContentsMargins(0, 12, 0, 0);
    reopenLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_reopenBtn = new QToolButton(m_historyReopen);
    m_reopenBtn->setObjectName(QStringLiteral("iconBtn"));
    m_reopenBtn->setIconSize(QSize(16, 16));
    m_reopenBtn->setCursor(Qt::PointingHandCursor);
    m_reopenBtn->setToolTip(QStringLiteral("展开历史面板"));
    m_reopenBtn->setIcon(Icons::chevronRight(ThemeManager::instance()->iconColor(), 16));
    // 主题变化时重着展开图标
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const Theme &) {
        m_reopenBtn->setIcon(Icons::chevronRight(ThemeManager::instance()->iconColor(), 16));
    });
    reopenLayout->addWidget(m_reopenBtn);
    m_historyReopen->hide();

    // ---- 三栏布局：导航 | 中央 | 历史（或展开条）----
    auto *root = new QHBoxLayout;
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_sideBar);
    root->addWidget(m_pages, 1);
    root->addWidget(m_history);
    root->addWidget(m_historyReopen);

    auto *rootWidget = new QWidget(this);
    rootWidget->setObjectName(QStringLiteral("appRoot")); // 中央根容器背景随主题
    rootWidget->setAutoFillBackground(true);
    rootWidget->setLayout(root);
    setCentralWidget(rootWidget);

    // 主内容区背景兜底：QSS polish 会重置 autoFillBackground，且顶层原生填充
    // 使用应用调色板，故在 themeChanged（即 setTheme 完成后）再强制设置调色板
    // + autoFill，保证深色模式下中间聊天区与两侧侧栏一致变深。
    const auto applyContentBackground = [this]() {
        const QColor bg = ThemeManager::instance()->backgroundColor();
        const auto paint = [bg](QWidget *w) {
            // 必须以「应用调色板」为基准重新取整份调色板。
            // QWidget::setPalette 一旦调用，该控件就不再跟随应用调色板；
            // 若只改 Window 而沿用控件自带调色板，Base/Text 等角色会停留在
            // 上一个主题的值上（滚动区视口用 Base），切换主题后聊天区底色
            // 就会重新变反。用应用调色板做基准可保证每次都同步到当前主题。
            QPalette p = QApplication::palette();
            p.setColor(QPalette::Window, bg);
            w->setPalette(p);
            w->setAutoFillBackground(true);
        };
        paint(centralWidget()); // rootWidget(#appRoot)：兜住整个中央区
        paint(m_pages);         // 堆叠容器(#chatContainer)
        for (int i = 0; i < m_pages->count(); ++i) {
            paint(m_pages->widget(i)); // 各功能页（mainContent / 占位页 / 设置页）
        }
    };
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, applyContentBackground);

    // ---- 状态栏：模型名 / 速度 / 内存 ----
    m_statusModel = new QLabel(QStringLiteral("模型: 未加载"), this);
    m_statusSpeed = new QLabel(QStringLiteral("速度: --"), this);
    m_statusMem = new QLabel(this);

    statusBar()->addWidget(m_statusModel, 1);
    statusBar()->addPermanentWidget(m_statusSpeed);
    statusBar()->addPermanentWidget(m_statusMem);

    refreshMemory();
    m_pages->setCurrentIndex(0);
}

void MainWindow::applyTheme() {
    // 从 QSettings 载入主题（浅色/深色、品牌色、密度、毛玻璃、气泡样式）
    QSettings settings;
    m_theme.dark = settings.value(QStringLiteral("theme/dark"), false).toBool();
    m_theme.brand = QColor(settings.value(
        QStringLiteral("theme/brand"), QColor(0x16, 0x5D, 0xFF).name()).toString());
    m_theme.density = settings.value(QStringLiteral("theme/density"), 1).toInt();
    m_theme.glass = settings.value(QStringLiteral("theme/glass"), true).toBool();
    m_theme.aiBubbleFilled =
        settings.value(QStringLiteral("theme/bubble"), true).toBool();

    ThemeManager::instance()->setTheme(m_theme);
    m_settingsPage->setThemeValues(m_theme);
}

void MainWindow::applyGenParams() {
    // 生成参数不硬编码：从配置读取，缺省沿用引擎默认值（温度 0.7 / Top-P 0.9 / 4096）
    QSettings settings;
    GenParams params = m_engine->genParams();
    params.temperature =
        settings.value(QStringLiteral("gen/temperature"), params.temperature).toFloat();
    params.top_p = settings.value(QStringLiteral("gen/topP"), params.top_p).toFloat();
    params.n_ctx = settings.value(QStringLiteral("gen/nCtx"), params.n_ctx).toInt();
    m_engine->setGenParams(params);
    m_settingsPage->setSamplingValues(params.temperature, params.top_p, params.n_ctx);
}

void MainWindow::saveTheme() {
    QSettings settings;
    settings.setValue(QStringLiteral("theme/dark"), m_theme.dark);
    settings.setValue(QStringLiteral("theme/brand"), m_theme.brand.name());
    settings.setValue(QStringLiteral("theme/density"), m_theme.density);
    settings.setValue(QStringLiteral("theme/glass"), m_theme.glass);
    settings.setValue(QStringLiteral("theme/bubble"), m_theme.aiBubbleFilled);
    ThemeManager::instance()->setTheme(m_theme);
    m_settingsPage->setThemeValues(m_theme);
}

void MainWindow::connectSignals() {
    // ---- 引擎 -> UI ----
    connect(m_engine, &LLMEngine::modelLoadingProgress, this,
            [this](int percent, const QString &info) {
        m_statusModel->setText(QStringLiteral("模型加载中: %1% %2")
                                   .arg(percent).arg(info));
    });

    connect(m_engine, &LLMEngine::modelLoaded, this, [this](bool ok, const QString &error) {
        if (!ok) {
            m_statusModel->setText(QStringLiteral("模型加载失败"));
            QMessageBox::warning(this, QStringLiteral("模型加载失败"), error);
            return;
        }
        const ModelInfo info = m_engine->modelInfo();
        m_statusModel->setText(QStringLiteral("模型: %1 (%2B)")
                                   .arg(info.desc)
                                   .arg(info.nParams / 1e9, 0, 'f', 1));
        m_settingsPage->setModelPath(info.path);
        statusBar()->showMessage(QStringLiteral("模型加载成功"), 3000);
        // 主模型就绪后再加载视觉编码器（mmproj 依赖主模型词汇表）
        loadMmproj();
    });

    connect(m_engine, &LLMEngine::mmprojLoaded, this, [this](bool ok, const QString &error) {
        if (ok) {
            statusBar()->showMessage(QStringLiteral("视觉编码器加载成功，可在输入栏上传图片识题"), 4000);
        } else if (!error.isEmpty()) {
            // mmproj 为可选能力：失败只提示，不影响纯文本对话
            statusBar()->showMessage(QStringLiteral("视觉编码器加载失败：%1").arg(error), 6000);
        }
    });

    connect(m_engine, &LLMEngine::errorOccurred, this, [this](const QString &message) {
        m_inputBar->setGenerating(false);
        // 工具页生成出错时在页内提示，不打断操作
        if (m_activeFunction) {
            m_activeFunction->showError(message);
            m_activeFunction = nullptr;
            return;
        }
        QMessageBox::warning(this, QStringLiteral("错误"), message);
    });

    // 流式输出：按当前生成来源分发到功能页或聊天区
    connect(m_engine, &LLMEngine::tokenGenerated, this, [this](const QString &piece) {
        if (m_activeFunction) {
            m_activeFunction->appendPiece(piece);
        } else {
            m_chat->appendAssistantPiece(piece);
        }
    });

    connect(m_engine, &LLMEngine::generationFinished, this,
            [this](const QString &fullResponse, bool interrupted, double tokPerSec) {
        // 生成来源：功能页 or 聊天区（先取出再清空，供下方分支判断）
        FunctionPage *finishedPage = m_activeFunction;
        m_activeFunction = nullptr;

        if (finishedPage) {
            finishedPage->endStreaming();
        } else {
            m_chat->finishAssistantMessage();
        }
        m_inputBar->setGenerating(false);
        if (interrupted) {
            m_statusSpeed->setText(QStringLiteral("已停止"));
        } else {
            m_statusSpeed->setText(QStringLiteral("速度: %1 tok/s").arg(tokPerSec, 0, 'f', 1));
        }
        // 仅聊天区回复写入会话历史；工具页结果属于一次性工具输出，不入库。
        // 入库前剥离 <思考>…</思考> 推理段，只保存正式答案，
        // 这样重新打开历史会话时不会把推理过程再显示一遍。
        if (!finishedPage && m_currentSessionId > 0 && !fullResponse.isEmpty()) {
            const QString answer = ThinkStreamParser::extractAnswer(fullResponse).trimmed();
            if (!answer.isEmpty()) {
                m_historyMgr->addMessage(m_currentSessionId, QStringLiteral("assistant"), answer);
                refreshSessionList();
            }
        }
    });

    // ---- 输入栏 -> 引擎 ----
    connect(m_inputBar, &ChatInputBar::sendRequested, this, [this](const QString &text) {
        if (!m_engine->isModelLoaded()) {
            statusBar()->showMessage(QStringLiteral("模型尚未加载完成，请稍候…"), 3000);
            return;
        }
        if (m_engine->isGenerating()) {
            statusBar()->showMessage(QStringLiteral("正在生成中，请先停止或等待完成"), 3000);
            return;
        }

        const QString imagePath = m_inputBar->imagePath();
        const bool withImage = !imagePath.isEmpty();
        if (withImage && !m_engine->isMmprojLoaded()) {
            statusBar()->showMessage(
                QStringLiteral("未配置视觉编码器（mmproj），无法识别图片，请到设置页配置"), 6000);
            return;
        }

        // 提示词：带图走「拍照识题」模板，纯文字走「学习答疑」模板
        const QString prompt = withImage ? PromptTemplates::photoQuiz(text)
                                         : PromptTemplates::studyAssistant(text);

        // UI 展示用户消息（带图时气泡内显示缩略图）
        if (withImage) {
            m_chat->addUserImageMessage(imagePath, text);
        } else {
            m_chat->addUserMessage(text);
        }

        // 首次提问时自动创建会话（标题取首条消息截断）
        if (m_currentSessionId <= 0) {
            QString title = text;
            if (title.isEmpty()) {
                title = QStringLiteral("图片识题");
            }
            if (title.size() > 20) {
                title = title.left(20) + QStringLiteral("…");
            }
            m_currentSessionId = m_historyMgr->createSession(title);
            refreshSessionList();
        }
        if (m_currentSessionId > 0) {
            // 图片路径随消息落库，重新打开会话时可还原缩略图
            m_historyMgr->addMessage(m_currentSessionId, QStringLiteral("user"),
                                     encodeUserContent(withImage, imagePath, text));
        }

        m_inputBar->clear();
        m_inputBar->clearImageAttachment();
        m_inputBar->setGenerating(true);
        m_statusSpeed->setText(QStringLiteral("生成中…"));
        m_chat->beginAssistantMessage();

        if (withImage) {
            m_engine->generateWithImageAsync(imagePath, prompt);
        } else {
            m_engine->generateAsync(prompt);
        }
    });

    connect(m_inputBar, &ChatInputBar::stopRequested, this, [this]() {
        m_engine->stopGenerating(); // 推理线程下一 token 即安全停止
    });

    // ---- 历史面板 ----
    connect(m_history, &HistoryPanel::newSessionRequested,
            this, &MainWindow::newSession);
    connect(m_history, &HistoryPanel::sessionSelected,
            this, &MainWindow::openSession);
    connect(m_history, &HistoryPanel::sessionDeleteRequested,
            this, &MainWindow::deleteSession);
    connect(m_history, &HistoryPanel::collapseRequested, this, [this]() {
        setHistoryCollapsed(true);
    });
    connect(m_history, &HistoryPanel::exportRequested,
            this, &MainWindow::exportCurrentSession);
    connect(m_reopenBtn, &QToolButton::clicked, this, [this]() {
        setHistoryCollapsed(false);
    });

    // ---- 导航栏：页面切换 ----
    connect(m_sideBar, &SideBar::pageChanged, this, &MainWindow::goToPage);
    // 导航栏底部：主题切换
    connect(m_sideBar, &SideBar::themeToggleRequested, this, [this]() {
        m_theme.dark = !m_theme.dark;
        saveTheme();
    });

    // ---- 空状态快捷卡片 -> 跳转对应功能页 ----
    connect(m_chat, &ChatWidget::quickAction, this, &MainWindow::goToPage);

    // ---- 设置页 ----
    connect(m_settingsPage, &SettingsPage::brandColorChanged, this, [this](const QColor &color) {
        m_theme.brand = color;
        saveTheme();
    });
    connect(m_settingsPage, &SettingsPage::densityChanged, this, [this](int density) {
        m_theme.density = density;
        saveTheme();
    });
    connect(m_settingsPage, &SettingsPage::glassToggled, this, [this](bool on) {
        m_theme.glass = on;
        saveTheme();
    });
    connect(m_settingsPage, &SettingsPage::bubbleStyleChanged, this, [this](bool filled) {
        m_theme.aiBubbleFilled = filled;
        saveTheme();
    });
    connect(m_settingsPage, &SettingsPage::modelChangeRequested, this, [this](const QString &path) {
        QSettings settings;
        settings.setValue(QStringLiteral("model/path"), path);
        loadModel(path);
    });
    connect(m_settingsPage, &SettingsPage::mmprojChangeRequested, this,
            [this](const QString &path) {
        QSettings settings;
        settings.setValue(QStringLiteral("model/mmproj"), path);
        loadMmproj(path);
    });

    // 温度 / Top-P：引擎每次生成前会按最新参数重建采样链，改动立即生效
    connect(m_settingsPage, &SettingsPage::samplingChanged, this,
            [this](double temperature, double topP) {
        QSettings settings;
        settings.setValue(QStringLiteral("gen/temperature"), temperature);
        settings.setValue(QStringLiteral("gen/topP"), topP);
        GenParams params = m_engine->genParams();
        params.temperature = static_cast<float>(temperature);
        params.top_p = static_cast<float>(topP);
        m_engine->setGenParams(params);
    });

    // 上下文长度只能在创建上下文时生效，因此需要重新加载模型
    connect(m_settingsPage, &SettingsPage::contextLengthChanged, this, [this](int nCtx) {
        QSettings settings;
        settings.setValue(QStringLiteral("gen/nCtx"), nCtx);
        GenParams params = m_engine->genParams();
        if (params.n_ctx == nCtx) {
            return;
        }
        params.n_ctx = nCtx;
        m_engine->setGenParams(params);

        const QString modelPath = m_engine->modelInfo().path;
        if (modelPath.isEmpty()) {
            statusBar()->showMessage(
                QStringLiteral("上下文长度已保存，下次加载模型时生效"), 5000);
            return;
        }
        statusBar()->showMessage(
            QStringLiteral("上下文长度已改为 %1，正在重新加载模型…").arg(nCtx), 8000);
        loadModel(modelPath);
    });

    m_inputBar->focusInput();
}

QWidget *MainWindow::makePlaceholderPage(NavPage page) {
    auto *w = new QWidget(m_pages);
    w->setObjectName(QStringLiteral("chatContainer"));
    w->setAutoFillBackground(true); // 占位页背景随主题
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(32, 32, 32, 32);
    lay->setSpacing(10);

    lay->addStretch(2);

    // 线性图标（主题切换时重着色）
    auto *iconLabel = new QLabel(w);
    iconLabel->setAlignment(Qt::AlignCenter);
    const auto iconFn = placeholderIcon(page);
    iconLabel->setPixmap(iconFn(ThemeManager::instance()->accentColor(), 48).pixmap(48, 48));
    lay->addWidget(iconLabel, 0, Qt::AlignHCenter);

    auto *title = new QLabel(navPageName(page), w);
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);
    lay->addWidget(title);

    auto *desc = new QLabel(QStringLiteral("该功能将在后续版本提供，敬请期待"), w);
    desc->setObjectName(QStringLiteral("pageDesc"));
    desc->setAlignment(Qt::AlignCenter);
    lay->addWidget(desc);

    lay->addStretch(3);

    // 主题变化时重着图标
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, w,
            [iconLabel, iconFn](const Theme &) {
        iconLabel->setPixmap(
            iconFn(ThemeManager::instance()->accentColor(), 48).pixmap(48, 48));
    });
    return w;
}

void MainWindow::loadModel(const QString &overridePath) {
    // 模型路径不硬编码：命令行参数 > QSettings 设置 > 程序目录 models\ > 弹窗选择
    QSettings settings;
    QString modelPath = overridePath;
    if (modelPath.isEmpty()) {
        modelPath = settings.value(QStringLiteral("model/path")).toString();
    }

    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        // 便携版：程序目录带了 models\ 就直接用，无需手工选择
        modelPath = findBundledModel(false);
    }

    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        statusBar()->showMessage(QStringLiteral("请选择 GGUF 模型文件"), 5000);
        modelPath = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("选择 GGUF 模型文件"),
            QString(),
            QStringLiteral("GGUF 模型 (*.gguf)"));
        if (modelPath.isEmpty()) {
            m_statusModel->setText(QStringLiteral("模型: 未选择"));
            return;
        }
    }

    // 记住用户选择（命令行指定时也记录下来，下次启动免选）
    settings.setValue(QStringLiteral("model/path"), modelPath);
    m_settingsPage->setModelPath(modelPath);

    m_engine->loadModelAsync(modelPath);
}

QString MainWindow::findBundledModel(bool wantMmproj) const {
    // 随包附带的模型放在 exe 同级的 models\ 下：
    //   主模型  取体积最大的、文件名不含 mmproj 的 gguf
    //   视觉编码器 取文件名含 mmproj 的 gguf
    const QDir dir(QCoreApplication::applicationDirPath() + QStringLiteral("/models"));
    if (!dir.exists()) {
        return QString();
    }
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*.gguf")}, QDir::Files, QDir::Size);
    QFileInfo best;
    for (const QFileInfo &fi : files) {
        const bool isMmproj =
            fi.fileName().contains(QStringLiteral("mmproj"), Qt::CaseInsensitive);
        if (isMmproj != wantMmproj) {
            continue;
        }
        if (!best.exists() || fi.size() > best.size()) {
            best = fi;
        }
    }
    return best.exists() ? best.absoluteFilePath() : QString();
}

void MainWindow::loadMmproj(const QString &overridePath) {
    // 视觉编码器路径同样不硬编码：参数 > QSettings > 程序目录 models\；
    // 未配置时静默跳过（纯文本对话不受影响，拍照识题会被禁用）
    QSettings settings;
    QString mmprojPath = overridePath;
    if (mmprojPath.isEmpty()) {
        mmprojPath = settings.value(QStringLiteral("model/mmproj")).toString();
    }
    if (mmprojPath.isEmpty() || !QFileInfo::exists(mmprojPath)) {
        mmprojPath = findBundledModel(true);
    }
    if (mmprojPath.isEmpty() || !QFileInfo::exists(mmprojPath)) {
        m_settingsPage->setMmprojPath(QString());
        return;
    }
    settings.setValue(QStringLiteral("model/mmproj"), mmprojPath);
    m_settingsPage->setMmprojPath(mmprojPath);
    m_engine->loadMmprojAsync(mmprojPath);
}

void MainWindow::refreshMemory() {
    const quint64 kb = currentProcessMemoryKB();
    m_statusMem->setText(QStringLiteral("内存: %1").arg(formatMemorySize(kb * 1024)));
}

void MainWindow::setHistoryCollapsed(bool collapsed) {
    if (m_historyCollapsed == collapsed) {
        return;
    }
    m_historyCollapsed = collapsed;
    m_history->setVisible(!collapsed);
    m_historyReopen->setVisible(collapsed);
}

void MainWindow::goToPage(NavPage page) {
    // 「拍照识题」不做独立页面：跳转对话页并直接唤起图片选择，
    // 识别结果与后续追问都在同一对话上下文里
    if (page == NavPage::PhotoQuiz) {
        m_sideBar->setCurrentPage(NavPage::Chat);
        m_pages->setCurrentIndex(static_cast<int>(NavPage::Chat));
        m_inputBar->focusInput();
        m_inputBar->promptForImage();
        return;
    }
    m_sideBar->setCurrentPage(page);
    m_pages->setCurrentIndex(static_cast<int>(page));
    if (page == NavPage::Chat) {
        m_inputBar->focusInput();
    }
}

// ============================================================================
// 工具功能页（Step6）
// ============================================================================

void MainWindow::wireFunctionPage(FunctionPage *page) {
    connect(page, &FunctionPage::runRequested, this,
            [this, page](const QString &prompt) { runFunctionPage(page, prompt); });
    connect(page, &FunctionPage::stopRequested, this, [this]() {
        m_engine->stopGenerating();
    });
}

void MainWindow::runFunctionPage(FunctionPage *page, const QString &prompt) {
    if (!m_engine->isModelLoaded()) {
        page->showError(QStringLiteral("模型尚未加载完成，请稍候…"));
        return;
    }
    if (m_engine->isGenerating()) {
        page->showError(QStringLiteral("正在生成中，请先停止或等待完成"));
        return;
    }
    m_activeFunction = page; // 之后的 token 都流向本页
    page->beginStreaming();
    m_statusSpeed->setText(QStringLiteral("生成中…"));
    m_engine->generateOnceAsync(prompt);
}

// ============================================================================
// SQLite 历史记录（Step4）
// ============================================================================

void MainWindow::openDatabase() {
    m_historyMgr = new HistoryManager(this);
    // 数据库放在应用数据目录（%APPDATA%/StudentAI/学生AI助手/student_ai.db）
    // 目录名来自 applicationName，产品改名 Quik 后刻意未动，避免历史记录迁移
    const QString dbDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbDir);
    if (!m_historyMgr->open(dbDir + QStringLiteral("/student_ai.db"))) {
        statusBar()->showMessage(
            QStringLiteral("历史记录数据库打开失败: %1").arg(m_historyMgr->lastError()), 8000);
        return;
    }
    refreshSessionList();
}

void MainWindow::refreshSessionList() {
    if (m_historyMgr && m_historyMgr->isOpen()) {
        m_history->setSessions(m_historyMgr->listSessions());
        m_history->setActiveSession(m_currentSessionId);
    }
}

void MainWindow::newSession() {
    if (m_engine->isGenerating()) {
        m_engine->stopGenerating();
    }
    m_currentSessionId = -1;
    m_chat->clear();
    m_engine->resetContext(); // 清空引擎多轮记忆
    m_history->setActiveSession(-1);
    m_inputBar->focusInput();
    statusBar()->showMessage(QStringLiteral("已新建会话"), 2000);
}

void MainWindow::openSession(qint64 sessionId) {
    if (sessionId <= 0 || !m_historyMgr || !m_historyMgr->isOpen()) {
        return;
    }
    if (m_engine->isGenerating()) {
        m_engine->stopGenerating();
    }
    m_currentSessionId = sessionId;
    m_engine->resetContext(); // 切换会话，引擎从新输入开始累积上下文

    // 重放历史消息到聊天区（仅 UI 展示，带原始时间戳）
    m_chat->clear();
    const auto messages = m_historyMgr->loadMessages(sessionId);
    for (const MessageInfo &msg : messages) {
        const QString time = msg.createdAt.mid(11, 5); // HH:mm
        if (msg.role == QStringLiteral("user")) {
            // 带图消息：解析出图片路径还原缩略图（图片文件已不在则退化为纯文本）
            QString text;
            const QString imagePath = decodeUserContent(msg.content, &text);
            if (!imagePath.isEmpty()) {
                m_chat->addUserImageMessage(imagePath, text, time);
            } else {
                m_chat->addUserMessage(text, time);
            }
        } else if (msg.role == QStringLiteral("assistant")) {
            m_chat->addAssistantMessage(msg.content, time);
        }
    }
    m_history->setActiveSession(sessionId);
    m_inputBar->focusInput();
}

void MainWindow::deleteSession(qint64 sessionId) {
    if (sessionId <= 0 || !m_historyMgr || !m_historyMgr->isOpen()) {
        return;
    }
    const auto ret = QMessageBox::question(
        this, QStringLiteral("删除会话"),
        QStringLiteral("确定删除该会话及其全部消息吗？此操作不可恢复。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    if (!m_historyMgr->deleteSession(sessionId)) {
        QMessageBox::warning(this, QStringLiteral("删除失败"),
                             m_historyMgr->lastError());
        return;
    }
    if (m_currentSessionId == sessionId) {
        newSession(); // 删除的是当前会话，回到空会话状态
    }
    refreshSessionList();
}

// ============================================================================
// 导出会话为 Markdown（P2）
// ============================================================================

void MainWindow::exportCurrentSession() {
    if (m_currentSessionId <= 0 || !m_historyMgr || !m_historyMgr->isOpen()) {
        statusBar()->showMessage(QStringLiteral("当前没有可导出的会话"), 3000);
        return;
    }
    const auto messages = m_historyMgr->loadMessages(m_currentSessionId);
    if (messages.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("当前会话还没有内容"), 3000);
        return;
    }

    // 会话标题（用于文档标题与默认文件名）
    QString title = QStringLiteral("会话");
    const auto sessions = m_historyMgr->listSessions();
    for (const SessionInfo &s : sessions) {
        if (s.id == m_currentSessionId) {
            title = s.title;
            break;
        }
    }

    // 默认文件名：标题去掉文件系统非法字符 + 日期
    QString base = title;
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString defaultName =
        QStringLiteral("%1_%2.md")
            .arg(base.left(30), QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd")));

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出为 Markdown"), defaultName,
        QStringLiteral("Markdown 文件 (*.md)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件：%1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // 保证中文正常写入
    out << "# " << title << "\n\n"
        << "> 导出时间："
        << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        << "  \n> 由「Quik」离线导出\n\n---\n\n";

    for (const MessageInfo &msg : messages) {
        if (msg.role == QStringLiteral("user")) {
            // 带图消息存的是 <<IMG:路径>>正文，这里还原成 Markdown 图片语法
            QString text;
            const QString imagePath = decodeUserContent(msg.content, &text);
            out << "### 我\n\n";
            if (!imagePath.isEmpty()) {
                out << "![图片](" << imagePath << ")\n\n";
            }
            out << text << "\n\n";
        } else if (msg.role == QStringLiteral("assistant")) {
            out << "### AI\n\n" << msg.content << "\n\n";
        }
    }
    file.close();

    statusBar()->showMessage(QStringLiteral("已导出到 %1").arg(path), 6000);
}

// ============================================================================
// 全局快捷键唤起（P2）：Ctrl+Alt+A
// ============================================================================

void MainWindow::setupGlobalHotkey() {
#ifdef Q_OS_WIN
    // 以主窗口句柄注册：WM_HOTKEY 会投递到该窗口，再由 Qt 转给原生事件过滤器
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_ALT, 'A')) {
        // 热键可能被其它程序占用：只提示，不影响其它功能
        statusBar()->showMessage(
            QStringLiteral("全局快捷键 Ctrl+Alt+A 注册失败（可能已被其它程序占用）"), 8000);
        return;
    }
    m_hotkeyHwnd = reinterpret_cast<quintptr>(hwnd);
    m_hotkeyFilter = std::make_unique<HotkeyEventFilter>([this]() { summonWindow(); });
    QApplication::instance()->installNativeEventFilter(m_hotkeyFilter.get());
#else
    // 非 Windows：退化为窗口内快捷键，保持跨平台可用
    auto *shortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+A")), this);
    connect(shortcut, &QShortcut::activated, this, &MainWindow::summonWindow);
#endif
}

void MainWindow::summonWindow() {
    if (isMinimized()) {
        showNormal();
    }
    show();
    raise();
    activateWindow();
    m_inputBar->focusInput();
}

} // namespace studentai
