#include "core/HistoryManager.h"

#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace studentai {

// SQLite 连接名（全局唯一，避免多实例冲突）
static const QString kConnName = QStringLiteral("history_conn");

HistoryManager::HistoryManager(QObject *parent) : QObject(parent) {
    // 使用独立命名连接，避免与其它 QSqlDatabase 冲突
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnName);
}

HistoryManager::~HistoryManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
    const QString name = m_db.connectionName();
    m_db = QSqlDatabase();
    if (QSqlDatabase::contains(name)) {
        QSqlDatabase::removeDatabase(name);
    }
}

bool HistoryManager::open(const QString &dbPath) {
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    // 开启外键约束（级联删除 messages）
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    if (!ensureSchema()) {
        return false;
    }
    m_open = true;
    return true;
}

bool HistoryManager::isOpen() const {
    return m_open && m_db.isOpen();
}

QString HistoryManager::lastError() const {
    return m_lastError;
}

bool HistoryManager::ensureSchema() {
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS sessions ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " title TEXT NOT NULL,"
            " created_at TEXT NOT NULL,"
            " updated_at TEXT NOT NULL)"))) {
        m_lastError = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS messages ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " session_id INTEGER NOT NULL,"
            " role TEXT NOT NULL,"
            " content TEXT NOT NULL,"
            " created_at TEXT NOT NULL,"
            " FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE)"))) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

qint64 HistoryManager::createSession(const QString &title) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sessions(title, created_at, updated_at) VALUES(?, ?, ?)"));
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    q.addBindValue(title);
    q.addBindValue(now);
    q.addBindValue(now);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool HistoryManager::deleteSession(qint64 sessionId) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM sessions WHERE id = ?"));
    q.addBindValue(sessionId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

QVector<SessionInfo> HistoryManager::listSessions() {
    QVector<SessionInfo> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id, title, created_at FROM sessions ORDER BY updated_at DESC, id DESC"))) {
        m_lastError = q.lastError().text();
        return out;
    }
    while (q.next()) {
        SessionInfo s;
        s.id = q.value(0).toLongLong();
        s.title = q.value(1).toString();
        s.createdAt = q.value(2).toString();
        out.append(s);
    }
    return out;
}

bool HistoryManager::addMessage(qint64 sessionId, const QString &role, const QString &content) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO messages(session_id, role, content, created_at) VALUES(?, ?, ?, ?)"));
    q.addBindValue(sessionId);
    q.addBindValue(role);
    q.addBindValue(content);
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }

    // 刷新会话更新时间（用于列表排序）
    QSqlQuery u(m_db);
    u.prepare(QStringLiteral("UPDATE sessions SET updated_at = ? WHERE id = ?"));
    u.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    u.addBindValue(sessionId);
    u.exec();
    return true;
}

QVector<MessageInfo> HistoryManager::loadMessages(qint64 sessionId) {
    QVector<MessageInfo> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, role, content, created_at FROM messages WHERE session_id = ? ORDER BY id ASC"));
    q.addBindValue(sessionId);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return out;
    }
    while (q.next()) {
        MessageInfo m;
        m.id = q.value(0).toLongLong();
        m.role = q.value(1).toString();
        m.content = q.value(2).toString();
        m.createdAt = q.value(3).toString();
        out.append(m);
    }
    return out;
}

} // namespace studentai
