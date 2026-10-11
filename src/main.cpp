// main.cpp
// ============================================================================
// 学生AI助手 - GUI 入口（Qt Widgets）
// ============================================================================

#include "ui/MainWindow.h"
#include "utils/Logger.h"

#include <QApplication>
#include <QFont>

using namespace studentai;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // applicationName 参与 QStandardPaths::AppDataLocation 的路径推导
    // （%APPDATA%/StudentAI/学生AI助手/）。产品改名为 Quik 后此处刻意保持原值，
    // 否则历史记录数据库会换位置、旧会话全部丢失；对外展示改用 DisplayName。
    QApplication::setApplicationName(QStringLiteral("学生AI助手"));
    QApplication::setOrganizationName(QStringLiteral("StudentAI"));
    QApplication::setApplicationDisplayName(QStringLiteral("Quik"));

    // 默认字体使用中文字体（QSS 中 font-family 失效时的兜底，保证中文可读）
    QApplication::setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));

    // 日志初始化（与 CLI 共用一套：控制台 + 文件）
    Logger::init("student_ai.log");

    // 可选：命令行第一个参数 = 模型路径（便于测试与快捷方式，不硬编码）
    MainWindow window(app.arguments().value(1));
    window.show();

    return app.exec();
}
