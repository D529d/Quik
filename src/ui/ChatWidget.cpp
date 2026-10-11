#include "ui/ChatWidget.h"

#include "core/ImageProcessor.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"
#include "ui/ThinkBlock.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPalette>
#include <QScrollBar>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

namespace studentai {

// ============================================================================
// MessageBubble：单条聊天气泡（带背景/圆角/切角的 QFrame + QLabel）
// 样式由 QSS 控制：#bubbleUser / #bubbleAI（[outline] 属性切换填色/描边）
// ============================================================================
class MessageBubble : public QFrame {
public:
    explicit MessageBubble(bool fromUser, QWidget *parent = nullptr)
        : QFrame(parent) {
        setObjectName(fromUser ? QStringLiteral("bubbleUser")
                               : QStringLiteral("bubbleAI"));
        setProperty("outline", false); // AI 气泡描边模式由 refreshTheme() 按主题设置
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(14, 10, 14, 10);
        lay->setSpacing(8);
        m_layout = lay;

        // AI 气泡顶部：深度思考区块（点阵动画 + Thinking... + 可折叠推理正文）
        // 用户在气泡内不创建；正式答案在思考区块下方单独展示
        if (!fromUser) {
            m_think = new ThinkBlock(this);
            m_think->hide();
            lay->addWidget(m_think);
        }

        // 图片附件（默认无图；Step5 拍照识题时显示在文字上方）
        m_image = new QLabel(this);
        m_image->setObjectName(QStringLiteral("bubbleImage"));
        m_image->setAlignment(Qt::AlignCenter);
        m_image->hide();
        lay->addWidget(m_image);

        m_label = new QLabel(this);
        m_label->setWordWrap(true);
        if (fromUser) {
            // 用户输入用纯文本，避免其中的 < > 被当作 HTML 解析
            m_label->setTextFormat(Qt::PlainText);
        } else {
            // 正式答案用 Markdown 渲染（支持标题 / 列表 / 加粗等基础格式）
            m_label->setTextFormat(Qt::MarkdownText);
        }
        m_label->setTextInteractionFlags(Qt::TextSelectableByMouse
                                         | Qt::TextSelectableByKeyboard);
        m_label->setObjectName(fromUser ? QStringLiteral("bubbleLabelUser")
                                        : QStringLiteral("answerText"));
        lay->addWidget(m_label);

        // 答案区占位提示：思考阶段显示，正式答案开始输出后隐藏
        if (!fromUser) {
            m_hint = new QLabel(QStringLiteral("正在生成正式回答…"), this);
            m_hint->setObjectName(QStringLiteral("answerHint"));
            m_hint->setWordWrap(true);
            m_hint->hide();
            lay->addWidget(m_hint);
        }
    }

    void setText(const QString &t) { m_label->setText(t); }
    QString text() const { return m_label->text(); }

    // 在气泡内显示一张图片（等比缩放到最大 maxSide）
    void setImage(const QPixmap &pm, int maxSide = 260) {
        if (pm.isNull()) {
            return;
        }
        const QPixmap scaled = pm.scaled(maxSide, maxSide, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        m_image->setPixmap(scaled);
        m_image->setFixedSize(scaled.size());
        m_image->show();
    }

    // 无文字时隐藏文本标签（纯图片消息）
    void hideTextIfEmpty() {
        if (m_label->text().isEmpty()) {
            m_label->hide();
        }
    }

    // ---- 双区块驱动：思考过程 → 正式答案 ----

    // 生成开始：显示思考区块（动画 + Thinking...），答案区显示占位提示
    void beginThink() {
        if (m_think) {
            m_think->begin();
        }
        if (m_hint) {
            m_hint->show();
        }
        m_label->hide();
    }

    // 追加一段推理文本
    void appendThinkPiece(const QString &piece) {
        if (!m_think) {
            return;
        }
        // 极端情形：模型先吐了答案文本，之后才出现 <思考>，此时需要重新唤起区块
        if (!m_think->isVisible()) {
            m_think->begin();
        }
        m_think->appendThink(piece);
    }

    // 思考结束、正式答案开始：动画换绿色对勾，隐藏占位，答案标签接管
    void beginAnswer() {
        if (m_think) {
            m_think->markAnswerStarted();
        }
        if (m_hint) {
            m_hint->hide();
        }
        m_label->show();
    }

    // 模型未按协议输出思考段：整块收起，退回普通回答样式
    void dropThinkBlock() {
        if (m_think) {
            m_think->showNoThink();
        }
        if (m_hint) {
            m_hint->hide();
        }
        m_label->show();
    }

    void appendAnswerPiece(const QString &piece) {
        m_label->setText(m_label->text() + piece);
        m_label->show();
    }

    // 生成收尾（含被打断的情况）
    void finishGeneration() {
        if (m_think) {
            m_think->finish();
        }
        // 若最终没有正式答案（例如在思考阶段被停止），收起答案区占位提示
        if (m_hint && m_label->text().isEmpty()) {
            m_hint->hide();
        }
    }

    // 气泡固定宽度为容器宽度的 70%（wordWrap 需要固定宽度才能换行）
    void updateWidth(int containerWidth) {
        setFixedWidth(qMax(containerWidth * 7 / 10, 260));
    }

    // 按主题切换 AI 气泡填色/描边
    void refreshStyle(bool filled) {
        if (objectName() == QLatin1String("bubbleAI")) {
            setProperty("outline", !filled);
            style()->unpolish(this);
            style()->polish(this);
        }
    }

private:
    QVBoxLayout *m_layout = nullptr;
    ThinkBlock *m_think = nullptr;         // AI 深度思考区块（用户气泡为 nullptr）
    QLabel *m_image = nullptr;             // 附件图片（可空）
    QLabel *m_label = nullptr;             // 正式答案（AI）/ 消息正文（用户）
    QLabel *m_hint = nullptr;              // 答案区占位提示（仅 AI）
};

// ============================================================================
// QuickCard：空状态快捷功能卡片（图标 + 标题 + 描述，可点击）
// ============================================================================
class QuickCard : public QFrame {
    Q_OBJECT
public:
    QuickCard(const QString &title, const QString &desc, QWidget *parent = nullptr)
        : QFrame(parent) {
        setObjectName(QStringLiteral("quickCard"));
        setFixedSize(210, 86);
        setCursor(Qt::PointingHandCursor);

        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(14, 12, 14, 12);
        lay->setSpacing(12);

        m_iconBox = new QLabel(this);
        m_iconBox->setObjectName(QStringLiteral("quickCardIconBox"));
        m_iconBox->setFixedSize(40, 40);
        m_iconBox->setAlignment(Qt::AlignCenter);

        auto *textBox = new QVBoxLayout;
        textBox->setSpacing(2);
        m_title = new QLabel(title, this);
        m_title->setObjectName(QStringLiteral("quickCardTitle"));
        m_desc = new QLabel(desc, this);
        m_desc->setObjectName(QStringLiteral("quickCardDesc"));
        m_desc->setWordWrap(true);
        textBox->addWidget(m_title);
        textBox->addWidget(m_desc);

        lay->addWidget(m_iconBox, 0, Qt::AlignTop);
        lay->addLayout(textBox, 1);
    }

    // 主题切换时更新图标颜色
    void refreshIcon(const QIcon &icon) {
        m_iconBox->setPixmap(icon.pixmap(22, 22));
    }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            setProperty("pressed", true);
            style()->unpolish(this);
            style()->polish(this);
        }
        QFrame::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        setProperty("pressed", false);
        style()->unpolish(this);
        style()->polish(this);
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            emit clicked();
        }
        QFrame::mouseReleaseEvent(event);
    }

private:
    QLabel *m_iconBox = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_desc = nullptr;
};

// ============================================================================
// ChatWidget
// ============================================================================
ChatWidget::ChatWidget(QWidget *parent) : QScrollArea(parent) {
    // objectName 供 QSS 精确指定滚动区及其视口底色（见 ThemeManager 的 #chatScroll 规则）
    setObjectName(QStringLiteral("chatScroll"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    // 滚动区视口默认以 QPalette::Base 作为底色。这里显式改为 Window 角色并开启
    // 背景填充，使视口底色严格跟随应用调色板（即主题），避免主题切换后
    // 该区域退回 Qt 默认色、出现「聊天区底色不随主题变化 / 与主题相反」。
    viewport()->setBackgroundRole(QPalette::Window);
    viewport()->setAutoFillBackground(true);

    m_container = new QWidget(this);
    // 命名以便 QSS 明确声明透明：中间层容器若不声明，主题切换后会残留旧调色板
    // 并被按那块过期颜色绘制，表现为聊天区出现颜色不符的色块
    m_container->setObjectName(QStringLiteral("chatContent"));
    m_container->setAutoFillBackground(false);
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(24, 20, 24, 20);
    m_layout->setSpacing(14);
    m_layout->addStretch(1); // 消息从上方排列，底部留弹性空间

    setWidget(m_container);

    // 定时器批量刷新流式文本（约 25fps）
    m_flushTimer.setInterval(40);
    connect(&m_flushTimer, &QTimer::timeout, this, &ChatWidget::flushPending);

    // 流式输出期间自动跟随到底部：文本增高后 rangeChanged 才带最新最大值，
    // 比在 setText 之后立即取 maximum() 更可靠；生成结束后不再抢占用户滚动。
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this,
            [this](int, int max) {
                if (m_currentAssistant) {
                    verticalScrollBar()->setValue(max);
                }
            });

    // ---- 空状态覆盖层（欢迎语 + 快捷卡片）----
    m_emptyState = new QWidget(viewport());
    m_emptyState->setObjectName(QStringLiteral("emptyState"));
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    emptyLayout->setSpacing(10);

    emptyLayout->addStretch(3);

    m_emptyLogo = new QLabel(m_emptyState);
    m_emptyLogo->setFixedSize(56, 56);
    m_emptyLogo->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyLogo, 0, Qt::AlignHCenter);

    auto *welcome = new QLabel(QStringLiteral("你好，我是 Quik"), m_emptyState);
    welcome->setObjectName(QStringLiteral("welcomeTitle"));
    welcome->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(welcome);

    auto *sub = new QLabel(QStringLiteral("完全离线运行 · 专注解答你的学习问题"), m_emptyState);
    sub->setObjectName(QStringLiteral("welcomeSub"));
    sub->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(sub);

    emptyLayout->addSpacing(8);

    // 4 个快捷功能卡片（2x2）
    auto *grid = new QGridLayout;
    grid->setSpacing(14);

    struct CardDef {
        NavPage page;
        const char *title;
        const char *desc;
        QIcon (*icon)(const QColor &, int);
    };
    const CardDef cards[] = {
        {NavPage::PhotoQuiz,   "拍照解题", "拍下题目，AI 帮你解答", &Icons::camera},
        {NavPage::NoteSummary, "笔记总结", "上传笔记，一键生成摘要", &Icons::note},
        {NavPage::Translate,   "离线翻译", "中英互译，全程离线", &Icons::globe},
        {NavPage::WrongQuiz,   "错题解析", "整理错题，逐个击破", &Icons::bulb},
    };
    for (int i = 0; i < 4; ++i) {
        auto *card = new QuickCard(QString::fromUtf8(cards[i].title),
                                   QString::fromUtf8(cards[i].desc), m_emptyState);
        const NavPage page = cards[i].page;
        connect(card, &QuickCard::clicked, this, [this, page]() {
            emit quickAction(page);
        });
        grid->addWidget(card, i / 2, i % 2);
        m_cards.append(card);
    }
    auto *gridWrap = new QWidget(m_emptyState);
    gridWrap->setObjectName(QStringLiteral("emptyGrid")); // 同上：需 QSS 声明透明
    gridWrap->setLayout(grid);
    emptyLayout->addWidget(gridWrap, 0, Qt::AlignHCenter);

    emptyLayout->addStretch(4);
    m_emptyState->hide();

    // 主题变化：空状态图标 + 气泡描边属性
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const Theme &) { refreshTheme(); });
    refreshTheme();
    showEmptyState();
}

void ChatWidget::refreshTheme() {
    ThemeManager *tm = ThemeManager::instance();
    const QColor accent = tm->accentColor();
    m_emptyLogo->setPixmap(Icons::logo(accent, 56).pixmap(56, 56));

    const QIcon iconColors[] = {
        Icons::camera(accent, 22), Icons::note(accent, 22),
        Icons::globe(accent, 22), Icons::bulb(accent, 22),
    };
    for (int i = 0; i < m_cards.size() && i < 4; ++i) {
        m_cards[i]->refreshIcon(iconColors[i]);
    }

    // 已有气泡：AI 填色/描边样式随主题切换
    const bool filled = tm->theme().aiBubbleFilled;
    for (MessageBubble *b : m_bubbles) {
        b->refreshStyle(filled);
    }
}

void ChatWidget::showEmptyState() {
    if (m_emptyState) {
        m_emptyState->show();
        m_emptyState->raise();
        m_emptyState->setGeometry(0, 0, viewport()->width(), viewport()->height());
    }
}

void ChatWidget::hideEmptyState() {
    if (m_emptyState) {
        m_emptyState->hide();
    }
}

void ChatWidget::addUserMessage(const QString &text, const QString &time) {
    addUserMessageInternal(text, time, QPixmap());
}

void ChatWidget::addUserImageMessage(const QString &imagePath, const QString &text,
                                     const QString &time) {
    // 只解码一次；图片不可用（路径失效/临时文件被清理）时退化为纯文本
    const QPixmap thumb = ImageProcessor::makeThumbnail(imagePath, 520);
    addUserMessageInternal(text, time, thumb);
}

void ChatWidget::addUserMessageInternal(const QString &text, const QString &time,
                                        const QPixmap &image) {
    hideEmptyState();

    auto *row = new QWidget(m_container);
    row->setObjectName(QStringLiteral("msgRow"));
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);
    hl->addStretch(1); // 用户消息靠右

    auto *vl = new QVBoxLayout;
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto *bubble = new MessageBubble(true, row);
    bubble->setText(text);
    if (!image.isNull()) {
        bubble->setImage(image);
        bubble->hideTextIfEmpty();
    }
    bubble->updateWidth(width());
    vl->addWidget(bubble);

    auto *t = new QLabel(time.isEmpty() ? QTime::currentTime().toString(QStringLiteral("HH:mm"))
                                        : time,
                         row);
    t->setObjectName(QStringLiteral("msgTime"));
    t->setAlignment(Qt::AlignRight);
    vl->addWidget(t);

    hl->addLayout(vl, 0);
    m_layout->addWidget(row, 0, Qt::AlignRight);

    m_rows.append(row);
    m_bubbles.append(bubble);

    m_currentAssistant = nullptr;
    m_currentTimeLabel = nullptr;
    scrollToBottom();
}

void ChatWidget::addAssistantMessage(const QString &text, const QString &time) {
    hideEmptyState();

    auto *row = new QWidget(m_container);
    row->setObjectName(QStringLiteral("msgRow"));
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    // AI 标识
    auto *avatar = new QLabel(QStringLiteral("AI"), row);
    avatar->setObjectName(QStringLiteral("aiAvatar"));
    avatar->setFixedSize(24, 24);
    avatar->setAlignment(Qt::AlignCenter);
    hl->addWidget(avatar, 0, Qt::AlignTop);

    auto *vl = new QVBoxLayout;
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto *bubble = new MessageBubble(false, row);
    // 历史记录里的回复已剥离思考段；这里再兜底一次，避免旧数据带标签显示出来
    bubble->setText(ThinkStreamParser::extractAnswer(text));
    bubble->updateWidth(width());
    vl->addWidget(bubble);

    auto *t = new QLabel(time.isEmpty() ? QTime::currentTime().toString(QStringLiteral("HH:mm"))
                                        : time,
                         row);
    t->setObjectName(QStringLiteral("msgTime"));
    t->setAlignment(Qt::AlignLeft);
    vl->addWidget(t);

    hl->addLayout(vl, 0);
    hl->addStretch(1);
    m_layout->addWidget(row, 0, Qt::AlignLeft);

    m_rows.append(row);
    m_bubbles.append(bubble);
    scrollToBottom();
}

void ChatWidget::beginAssistantMessage() {
    hideEmptyState();

    auto *row = new QWidget(m_container);
    row->setObjectName(QStringLiteral("msgRow"));
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);

    auto *avatar = new QLabel(QStringLiteral("AI"), row);
    avatar->setObjectName(QStringLiteral("aiAvatar"));
    avatar->setFixedSize(24, 24);
    avatar->setAlignment(Qt::AlignCenter);
    hl->addWidget(avatar, 0, Qt::AlignTop);

    auto *vl = new QVBoxLayout;
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto *bubble = new MessageBubble(false, row);
    bubble->updateWidth(width());
    vl->addWidget(bubble);
    // 思考阶段：气泡内显示「点阵动画 + Thinking...」区块，答案区显示占位提示
    bubble->beginThink();

    auto *t = new QLabel(QString(), row);
    t->setObjectName(QStringLiteral("msgTime"));
    t->setAlignment(Qt::AlignLeft);
    t->hide(); // 生成结束后再显示时间
    vl->addWidget(t);

    hl->addLayout(vl, 0);
    hl->addStretch(1);
    m_layout->addWidget(row, 0, Qt::AlignLeft);

    m_rows.append(row);
    m_bubbles.append(bubble);

    m_currentAssistant = bubble;
    m_currentTimeLabel = t;
    m_pendingText.clear();
    m_parser.reset();
    m_flushTimer.start();
    scrollToBottom();
}

void ChatWidget::appendAssistantPiece(const QString &piece) {
    if (!m_currentAssistant) {
        // 防御：没有活动气泡时自动创建
        beginAssistantMessage();
    }
    m_pendingText += piece;
}

void ChatWidget::flushPending() {
    if (!m_currentAssistant || m_pendingText.isEmpty()) {
        return;
    }
    // 交给解析器分流：思考段进思考区块，正式答案进答案标签。
    // 回调里用 m_parser.sawThinkTag() 判断此刻是否已进入双区块模式。
    const QString piece = m_pendingText;
    m_pendingText.clear();
    m_parser.feed(
        piece,
        [this](const QString &t) {
            if (m_currentAssistant) {
                m_currentAssistant->appendThinkPiece(t);
            }
        },
        [this](const QString &a) {
            if (!m_currentAssistant) {
                return;
            }
            if (m_parser.sawThinkTag()) {
                m_currentAssistant->beginAnswer(); // 思考完成 → 绿色对勾
            } else {
                m_currentAssistant->dropThinkBlock(); // 未按协议 → 收起思考区块
            }
            m_currentAssistant->appendAnswerPiece(a);
        });
    scrollToBottom();
}

void ChatWidget::finishAssistantMessage() {
    m_flushTimer.stop();
    flushPending();
    if (m_currentAssistant) {
        // 冲掉解析器留尾缓冲（末字可能还压在缓冲区里），再收尾思考区块
        m_parser.finish(
            [this](const QString &t) {
                if (m_currentAssistant) {
                    m_currentAssistant->appendThinkPiece(t);
                }
            },
            [this](const QString &a) {
                if (!m_currentAssistant) {
                    return;
                }
                if (m_parser.sawThinkTag()) {
                    m_currentAssistant->beginAnswer();
                } else {
                    m_currentAssistant->dropThinkBlock();
                }
                m_currentAssistant->appendAnswerPiece(a);
            });
        m_currentAssistant->finishGeneration();
        // 一个 token 都没产出的极端情况：收起思考区块，避免只剩空动画
        if (!m_parser.sawThinkTag() && m_currentAssistant->text().isEmpty()) {
            m_currentAssistant->dropThinkBlock();
            m_currentAssistant->appendAnswerPiece(QStringLiteral("（本次没有生成内容）"));
        }
    }
    if (m_currentTimeLabel) {
        m_currentTimeLabel->setText(
            QTime::currentTime().toString(QStringLiteral("HH:mm")));
        m_currentTimeLabel->show();
    }
    m_currentAssistant = nullptr;
    m_currentTimeLabel = nullptr;
    scrollToBottom();
}

void ChatWidget::clear() {
    m_flushTimer.stop();
    m_currentAssistant = nullptr;
    m_currentTimeLabel = nullptr;
    m_pendingText.clear();
    m_parser.reset();

    qDeleteAll(m_rows);
    m_rows.clear();
    m_bubbles.clear();
    showEmptyState();
}

void ChatWidget::scrollToBottom() {
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatWidget::resizeEvent(QResizeEvent *event) {
    QScrollArea::resizeEvent(event);
    // 窗口宽度变化时同步所有气泡宽度
    const int w = width();
    for (MessageBubble *bubble : m_bubbles) {
        bubble->updateWidth(w);
    }
    // 空状态覆盖层跟随视口
    if (m_emptyState && m_emptyState->isVisible()) {
        m_emptyState->setGeometry(0, 0, viewport()->width(), viewport()->height());
    }
}

} // namespace studentai

#include "ChatWidget.moc"
