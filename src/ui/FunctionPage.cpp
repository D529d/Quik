#include "ui/FunctionPage.h"

#include "ui/ThinkBlock.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace studentai {

namespace {

// 结果区在思考阶段的占位提示（首个正式答案 token 到达时清除）
const QString kAnswerPlaceholder = QStringLiteral("正在生成正式回答…");

// 功能页内的多行输入框（有边框、可聚焦，样式见 QSS #funcInput）
QTextEdit *makeTextEdit(const QString &placeholder, int minHeight, QWidget *parent) {
    auto *edit = new QTextEdit(parent);
    edit->setObjectName(QStringLiteral("funcInput"));
    edit->setAcceptRichText(false);
    edit->setPlaceholderText(placeholder);
    edit->setMinimumHeight(minHeight);
    // 占位符颜色（QSS 不支持 placeholder 取色）
    QPalette pal = edit->palette();
    pal.setColor(QPalette::PlaceholderText, QColor(0x86, 0x90, 0x9C));
    edit->setPalette(pal);
    return edit;
}

} // namespace

FunctionPage::FunctionPage(const Config &config, QWidget *parent)
    : QWidget(parent), m_config(config) {
    buildUi(config);
}

void FunctionPage::buildUi(const Config &config) {
    // 页面自身底色跟随主题（与聊天页一致）
    setObjectName(QStringLiteral("chatContainer"));
    setAutoFillBackground(true);
    // 以应用调色板为基准取整份调色板：控件一旦 setPalette 就不再跟随应用调色板，
    // 若沿用自身调色板只改 Window，其它角色会停留在旧主题值上（与设置页同理）
    const auto applyBackground = [this]() {
        QPalette p = QApplication::palette();
        p.setColor(QPalette::Window, ThemeManager::instance()->backgroundColor());
        setPalette(p);
    };
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [applyBackground](const Theme &) { applyBackground(); });
    applyBackground();

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->viewport()->setBackgroundRole(QPalette::Window);

    auto *content = new QWidget(scroll);
    // 滚动内容容器必须显式跟随主题：它与设置页是同一种结构，不命名且不在主题切换时
    // 重新应用调色板的话，切换后会继续带着旧主题颜色绘制，表现为「深色模式下背景
    // 是浅色、浅色模式下背景是深色」。
    content->setObjectName(QStringLiteral("chatContainer"));
    const auto applyContentBg = [content]() {
        QPalette p = QApplication::palette();
        p.setColor(QPalette::Window, ThemeManager::instance()->backgroundColor());
        content->setPalette(p);
        content->setAutoFillBackground(true);
    };
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, content, applyContentBg);
    applyContentBg();

    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(14);

    // ---- 标题 ----
    auto *title = new QLabel(config.title, content);
    title->setObjectName(QStringLiteral("funcTitle"));
    lay->addWidget(title);

    auto *sub = new QLabel(config.subtitle, content);
    sub->setObjectName(QStringLiteral("funcSub"));
    sub->setWordWrap(true);
    lay->addWidget(sub);

    // ---- 输入卡片 ----
    auto *inputCard = new QFrame(content);
    inputCard->setObjectName(QStringLiteral("settingCard"));
    auto *inputLay = new QVBoxLayout(inputCard);
    inputLay->setContentsMargins(20, 16, 20, 16);
    inputLay->setSpacing(8);

    auto *inputLabel = new QLabel(config.inputLabel, inputCard);
    inputLabel->setObjectName(QStringLiteral("funcLabel"));
    inputLay->addWidget(inputLabel);

    m_input = makeTextEdit(config.inputPlaceholder, 140, inputCard);
    inputLay->addWidget(m_input);

    // 第二输入框（仅错题解析使用）
    if (!config.secondLabel.isEmpty()) {
        m_secondLabelWidget = new QLabel(config.secondLabel, inputCard);
        m_secondLabelWidget->setObjectName(QStringLiteral("funcLabel"));
        inputLay->addWidget(m_secondLabelWidget);

        m_secondInput = makeTextEdit(config.secondPlaceholder, 90, inputCard);
        inputLay->addWidget(m_secondInput);
    }

    // ---- 操作按钮 ----
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);
    m_runBtn = new QPushButton(config.runText, inputCard);
    m_runBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_runBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(m_runBtn, 0, Qt::AlignLeft);

    auto *clearBtn = new QPushButton(QStringLiteral("清空"), inputCard);
    clearBtn->setObjectName(QStringLiteral("ghostBtn"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(clearBtn, 0, Qt::AlignLeft);
    btnRow->addStretch(1);
    inputLay->addLayout(btnRow);

    lay->addWidget(inputCard);

    // ---- 结果卡片 ----
    auto *resultCard = new QFrame(content);
    resultCard->setObjectName(QStringLiteral("settingCard"));
    auto *resultLay = new QVBoxLayout(resultCard);
    resultLay->setContentsMargins(20, 16, 20, 16);
    resultLay->setSpacing(8);

    auto *resultTitle = new QLabel(QStringLiteral("结果"), resultCard);
    resultTitle->setObjectName(QStringLiteral("settingTitle"));
    resultLay->addWidget(resultTitle);

    // 深度思考区块：位于结果正文上方，思考阶段显示动画 + 推理文本
    m_think = new ThinkBlock(resultCard);
    m_think->hide();
    resultLay->addWidget(m_think);

    m_result = new QTextEdit(resultCard);
    m_result->setObjectName(QStringLiteral("funcResult"));
    m_result->setReadOnly(true);
    m_result->setMinimumHeight(220);
    // 结果用等宽保留格式（模型会输出 Markdown 风格的标题/列表）
    resultLay->addWidget(m_result);

    lay->addWidget(resultCard, 1);

    scroll->setWidget(content);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);

    // ---- 信号 ----
    connect(m_runBtn, &QPushButton::clicked, this, [this]() {
        if (m_running) {
            emit stopRequested();
            return;
        }
        const QString first = m_input->toPlainText().trimmed();
        if (first.isEmpty()) {
            showError(QStringLiteral("请先输入内容"));
            return;
        }
        const QString second = m_secondInput ? m_secondInput->toPlainText().trimmed()
                                            : QString();
        if (m_config.buildPrompt) {
            emit runRequested(m_config.buildPrompt(first, second));
        }
    });

    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_input->clear();
        if (m_secondInput) {
            m_secondInput->clear();
        }
        m_result->clear();
        m_parser.reset();
        if (m_think) {
            m_think->showNoThink();
        }
    });
}

void FunctionPage::setRunning(bool running) {
    m_running = running;
    m_runBtn->setText(running ? QStringLiteral("停止") : m_config.runText);
    m_input->setReadOnly(running);
    if (m_secondInput) {
        m_secondInput->setReadOnly(running);
    }
}

void FunctionPage::beginStreaming() {
    m_result->clear();
    m_result->setPlainText(kAnswerPlaceholder);
    m_parser.reset();
    if (m_think) {
        m_think->begin();
    }
    setRunning(true);
}

void FunctionPage::appendPiece(const QString &piece) {
    if (!m_think || !m_result) {
        return;
    }
    // 解析器分流：思考段进思考区块，正式答案进结果区
    m_parser.feed(
        piece,
        [this](const QString &t) { m_think->appendThink(t); },
        [this](const QString &a) {
            if (m_parser.sawThinkTag()) {
                m_think->markAnswerStarted(); // 思考完成 → 绿色对勾
            } else {
                m_think->showNoThink();       // 未按协议 → 收起思考区块
            }
            // 首个正式答案 token 到达：清掉占位提示
            if (m_result->toPlainText() == kAnswerPlaceholder) {
                m_result->clear();
            }
            m_result->moveCursor(QTextCursor::End);
            m_result->insertPlainText(a);
            m_result->moveCursor(QTextCursor::End);
        });
}

void FunctionPage::endStreaming() {
    if (m_think) {
        // 冲掉解析器留尾缓冲，并收尾思考区块
        m_parser.finish(
            [this](const QString &t) { m_think->appendThink(t); },
            [this](const QString &a) {
                if (m_parser.sawThinkTag()) {
                    m_think->markAnswerStarted();
                } else {
                    m_think->showNoThink();
                }
                if (m_result->toPlainText() == kAnswerPlaceholder) {
                    m_result->clear();
                }
                m_result->moveCursor(QTextCursor::End);
                m_result->insertPlainText(a);
                m_result->moveCursor(QTextCursor::End);
            });
        // 没有任何思考内容（模型未按协议，或思考阶段被停止）：整块收起
        if (!m_parser.sawThinkTag() && !m_think->hasThinkText()) {
            m_think->showNoThink();
        }
        m_think->finish();
    }
    setRunning(false);
    const QString text = m_result->toPlainText();
    if (text.isEmpty() || text == kAnswerPlaceholder) {
        m_result->setPlainText(QStringLiteral("（本次没有生成内容）"));
    }
}

void FunctionPage::showError(const QString &message) {
    if (m_think) {
        m_think->showNoThink();
    }
    m_parser.reset();
    m_result->setPlainText(message);
    setRunning(false);
}

QString FunctionPage::resultText() const {
    return m_result->toPlainText();
}

} // namespace studentai