// hello_world.cpp
// ============================================================================
// Step1 验收程序：验证 "CMake 工程 + llama.cpp 集成 + 模型加载 + 流式单轮对话"
//
// 用法:
//   hello_world.exe -m <模型路径> [-p "问题"] [-n 最大token] [-t 温度] [-c 上下文长度]
//
// 不带 -p 时进入交互模式（多轮对话，输入 exit 退出）
// ============================================================================

#include "core/LLMEngine.h"
#include "utils/Logger.h"

#include <QCoreApplication>
#include <QTextStream>

#include <atomic>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace studentai;

// 交互模式：读一行输入并触发生成（在 main 之后定义）
static void readNextLine(LLMEngine &engine, QTextStream &out, std::atomic<bool> &stopLoop);

static void printUsage() {
    QTextStream out(stdout);
    out << "轻量离线学生AI助手 - 控制台验证程序\n"
        << "用法: hello_world.exe -m <模型路径> [选项]\n"
        << "  -m <path>       GGUF 模型文件路径（必填）\n"
        << "  -p <prompt>     单轮问题（可选；不填则进入交互模式）\n"
        << "  -n <int>        最大生成 token 数（默认 1024）\n"
        << "  -t <float>      温度（默认 0.7）\n"
        << "  -top_p <float>  Top-P（默认 0.9）\n"
        << "  -c <int>        上下文长度（默认 4096）\n"
        << "  -h              显示帮助\n";
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    // 控制台使用 UTF-8，保证中文显示正常
    SetConsoleOutputCP(CP_UTF8);
#endif

    QCoreApplication app(argc, argv);

    // ---- 解析命令行参数 ----
    QString modelPath;
    QString prompt;
    GenParams params;

    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args[i];
        auto next = [&]() -> QString { return (i + 1 < args.size()) ? args[++i] : QString(); };
        if (a == "-m")      modelPath = next();
        else if (a == "-p") prompt = next();
        else if (a == "-n") params.n_predict = next().toInt();
        else if (a == "-t") params.temperature = next().toFloat();
        else if (a == "-top_p") params.top_p = next().toFloat();
        else if (a == "-c") params.n_ctx = next().toInt();
        else if (a == "-h") { printUsage(); return 0; }
        else { QTextStream(stderr) << "未知参数: " << a << "\n"; printUsage(); return 1; }
    }

    if (modelPath.isEmpty()) {
        QTextStream(stderr) << "错误: 必须指定模型路径 (-m)\n";
        printUsage();
        return 1;
    }

    // ---- 日志 ----
    Logger::init("hello_world.log");

    QTextStream out(stdout);

    // ---- 引擎 ----
    LLMEngine engine;
    engine.setGenParams(params);

    QObject::connect(&engine, &LLMEngine::modelLoadingProgress,
                     [&](int percent, const QString &info) {
        if (percent < 100) {
            out << "\r" << info << QString(40, ' ');
            out.flush();
        }
    });
    QObject::connect(&engine, &LLMEngine::modelLoaded, [&](bool ok, const QString &error) {
        if (!ok) {
            QTextStream(stderr) << "\n错误: " << error << "\n";
            app.exit(1);
            return;
        }
        const auto info = engine.modelInfo();
        out << "\n[模型已加载] " << info.desc
            << " | 参数: " << (info.nParams / 1000000000.0) << "B"
            << " | 大小: " << info.sizeMB << "MB\n";
        out.flush();
    });
    QObject::connect(&engine, &LLMEngine::errorOccurred, [&](const QString &message) {
        QTextStream(stderr) << "\n[错误] " << message << "\n";
        app.exit(1);
    });

    // ---- 加载模型 ----
    out << "正在加载模型: " << modelPath << "\n";
    out.flush();
    engine.loadModelAsync(modelPath);

    // ---- 交互流程 ----
    const bool interactive = prompt.isEmpty();
    if (interactive) {
        // 交互模式：读一行 -> 生成 -> 生成完成后再读下一行
        // （推理在独立线程，阻塞读 stdin 不影响 worker；读取只在空闲时进行）
        auto &stopLoop = *(new std::atomic<bool>(false));
        QObject::connect(&engine, &LLMEngine::modelLoaded, [&](bool ok, const QString &) {
            if (!ok) return;
            QObject::connect(&engine, &LLMEngine::generationFinished,
                             [&](const QString &, bool, double) {
                readNextLine(engine, out, stopLoop);
            });
            readNextLine(engine, out, stopLoop);
        });
        QObject::connect(&engine, &LLMEngine::tokenGenerated, [&](const QString &piece) {
            if (stopLoop.load()) return;
            out << piece;
            out.flush();
        });
    } else {
        QObject::connect(&engine, &LLMEngine::tokenGenerated, [&](const QString &piece) {
            out << piece;
            out.flush();
        });
        QObject::connect(&engine, &LLMEngine::modelLoaded, [&](bool ok, const QString &) {
            if (ok) {
                out << "AI  > ";
                out.flush();
                engine.generateAsync(prompt);
            }
        });
        QObject::connect(&engine, &LLMEngine::generationFinished,
                         [&](const QString &, bool, double) { app.quit(); });
    }

    return app.exec();
}

// 从 stdin 读一行并触发生成；输入 exit 退出
static void readNextLine(LLMEngine &engine, QTextStream &out, std::atomic<bool> &stopLoop) {
    if (stopLoop.load()) {
        return;
    }
    out << "\n你 > ";
    out.flush();
    std::string line;
    if (!std::getline(std::cin, line)) {   // 阻塞读取（此时 worker 空闲）
        stopLoop = true;
        QCoreApplication::quit();
        return;
    }
    QString qline = QString::fromStdString(line).trimmed();
    if (qline.isEmpty()) {
        readNextLine(engine, out, stopLoop); // 空行继续等待
        return;
    }
    if (qline == "exit" || qline == "quit") {
        stopLoop = true;
        QCoreApplication::quit();
        return;
    }
    out << "AI  > ";
    out.flush();
    engine.generateAsync(qline); // 生成完成后会再次调用 readNextLine
}
