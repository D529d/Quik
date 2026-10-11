#pragma once
// 轻量日志工具：同时输出到控制台和日志文件
// 线程安全，可在任意线程调用

#include <QString>
#include <QMutex>
#include <QFile>
#include <atomic>

namespace studentai {

class Logger {
public:
    // 日志级别
    enum class Level { Debug = 0, Info, Warn, Error };

    // 初始化（设置日志文件路径），可重复调用
    static void init(const QString &logFile);
    // 关闭日志
    static void shutdown();
    // 设置最低输出级别
    static void setLevel(Level level);
    // 输出一条日志
    static void log(Level level, const QString &message);

    // 便捷宏: LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR
    static void debug(const QString &msg) { log(Level::Debug, msg); }
    static void info (const QString &msg) { log(Level::Info,  msg); }
    static void warn (const QString &msg) { log(Level::Warn,  msg); }
    static void error(const QString &msg) { log(Level::Error, msg); }

private:
    static QString levelName(Level level);

    static QMutex    s_mutex;
    static QFile     s_file;
    static Level     s_level;
    static QString   s_fileName;
    static std::atomic<bool> s_initialized;
};

} // namespace studentai

#define LOG_DEBUG(msg) studentai::Logger::debug(msg)
#define LOG_INFO(msg)  studentai::Logger::info(msg)
#define LOG_WARN(msg)  studentai::Logger::warn(msg)
#define LOG_ERROR(msg) studentai::Logger::error(msg)
