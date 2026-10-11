#include "utils/Logger.h"

#include <QDateTime>
#include <QTextStream>
#include <QDebug>

namespace studentai {

QMutex    Logger::s_mutex;
QFile     Logger::s_file;
Logger::Level Logger::s_level = Logger::Level::Debug;
QString   Logger::s_fileName;
std::atomic<bool> Logger::s_initialized{false};

void Logger::init(const QString &logFile) {
    QMutexLocker locker(&s_mutex);
    s_fileName = logFile;
    if (s_file.isOpen()) {
        s_file.close();
    }
    s_file.setFileName(logFile);
    s_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    s_initialized = true;
}

void Logger::shutdown() {
    QMutexLocker locker(&s_mutex);
    if (s_file.isOpen()) {
        s_file.close();
    }
    s_initialized = false;
}

void Logger::setLevel(Level level) {
    s_level = level;
}

void Logger::log(Level level, const QString &message) {
    if (static_cast<int>(level) < static_cast<int>(s_level)) {
        return;
    }
    const QString line = QString("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(levelName(level))
        .arg(message);

    QMutexLocker locker(&s_mutex);

    // 输出到控制台（stderr 保证不被 stdout 缓冲影响）
    QTextStream errStream(stderr);
    errStream << line << "\n";
    errStream.flush();

    // 输出到日志文件
    if (s_file.isOpen()) {
        QTextStream fileStream(&s_file);
        fileStream << line << "\n";
        fileStream.flush();
    }
}

QString Logger::levelName(Level level) {
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info:  return "INFO ";
    case Level::Warn:  return "WARN ";
    case Level::Error: return "ERROR";
    }
    return "?????";
}

} // namespace studentai
