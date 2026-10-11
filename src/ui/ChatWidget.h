#pragma once
// ============================================================================
// ChatWidget：聊天消息区（现代气泡风格）
// - 用户：右侧品牌色气泡，右下小切角；AI：左侧浅灰气泡 + AI 标识 + 左下小切角
// - 每条消息气泡下方显示小字时间戳
// - 流式输出：逐 token 追加，定时器批量刷新（避免每 token 重绘）
// - 空状态：居中欢迎语 + 4 个快捷功能卡片
// ============================================================================

#include <QList>
#include <QScrollArea>
#include <QTimer>

#include "core/ThinkStreamParser.h"
#include "ui/SideBar.h" // NavPage

class QLabel;
class QPixmap;
class QVBoxLayout;
class QWidget;

namespace studentai {

class MessageBubble;
class QuickCard;

class ChatWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    // 添加用户消息（time 为空则取当前时间）
    void addUserMessage(const QString &text, const QString &time = QString());
    // 添加带图的用户消息（Step5 拍照识题）：图片显示在文字气泡上方
    void addUserImageMessage(const QString &imagePath, const QString &text,
                             const QString &time = QString());
    // 直接添加完整 AI 消息（加载历史记录用）
    void addAssistantMessage(const QString &text, const QString &time = QString());
    // 流式输出：开始 / 追加 / 结束
    void beginAssistantMessage();
    void appendAssistantPiece(const QString &piece);
    void finishAssistantMessage();
    // 清空全部消息并显示空状态
    void clear();

signals:
    void quickAction(NavPage page); // 点击空状态快捷卡片

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void flushPending();       // 把缓存的文本片段刷到气泡
    void scrollToBottom();
    void showEmptyState();     // 显示空状态（无消息时）
    void hideEmptyState();
    void refreshTheme();       // 主题切换：气泡描边属性 + 空状态图标重着色
    // 用户消息统一实现（image 非空时在气泡内显示图片）
    void addUserMessageInternal(const QString &text, const QString &time,
                                const QPixmap &image);

    QWidget *m_container = nullptr;
    QVBoxLayout *m_layout = nullptr;

    QList<QWidget *> m_rows;        // 行容器（含气泡/时间/头像）
    QList<MessageBubble *> m_bubbles;

    MessageBubble *m_currentAssistant = nullptr; // 正在流式输出的气泡
    QLabel *m_currentTimeLabel = nullptr;        // 对应时间戳
    QString m_pendingText;                       // 待刷新文本缓存
    QTimer m_flushTimer;                         // 批量刷新定时器
    ThinkStreamParser m_parser;                  // 把流式文本切成「思考 / 答案」两段

    QWidget *m_emptyState = nullptr; // 空状态覆盖层
    QLabel *m_emptyLogo = nullptr;
    QList<QuickCard *> m_cards;
};

} // namespace studentai
