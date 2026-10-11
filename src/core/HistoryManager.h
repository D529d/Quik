#pragma once
// ============================================================================
// HistoryManager：SQLite 历史记录管理（会话 + 消息）
// ----------------------------------------------------------------------------
// 表结构:
//   sessions(id, title, created_at, updated_at)   -- 会话
//   messages(id, session_id, role, content, created_at) -- 消息（外键级联删除）
// 线程: 仅 UI 线程使用（SQLite 连接不跨线程）
// ============================================================================

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace studentai {

// 会话摘要（历史面板列表项）
struct SessionInfo {
    qint64 id = -1;
    QString title;     // 会话标题（默认取首条用户消息）
    QString createdAt; // 创建时间 yyyy-MM-dd HH:mm:ss
};

// 会话内的一条消息
struct MessageInfo {
    qint64 id = -1;
    QString role;      // user / assistant
    QString content;   // 消息全文
    QString createdAt; // 消息时间 yyyy-MM-dd HH:mm:ss
};

class HistoryManager : public QObject {
    Q_OBJECT
public:
    explicit HistoryManager(QObject *parent = nullptr);
    ~HistoryManager() override;

    // 打开（不存在则创建）数据库；失败返回 false，可用 lastError() 查看原因
    bool open(const QString &dbPath);
    bool isOpen() const;
    QString lastError() const;

    // 新建会话（title 通常为首条用户消息截断），返回会话 id，失败返回 -1
    qint64 createSession(const QString &title);
    // 删除会话及其全部消息
    bool deleteSession(qint64 sessionId);
    // 全部会话（按更新时间倒序，最新在前）
    QVector<SessionInfo> listSessions();
    // 追加一条消息并刷新会话更新时间
    bool addMessage(qint64 sessionId, const QString &role, const QString &content);
    // 会话内全部消息（按 id 正序）
    QVector<MessageInfo> loadMessages(qint64 sessionId);

private:
    bool ensureSchema(); // 建表（幂等）

    QSqlDatabase m_db;
    QString m_lastError;
    bool m_open = false;
};

} // namespace studentai
