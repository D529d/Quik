#pragma once
// ============================================================================
// FunctionPage：通用工具功能页（Step6）
// ----------------------------------------------------------------------------
// 笔记总结 / 离线翻译 / 错题解析三页共用同一套交互：
//   标题 + 说明 → 输入卡片（1~2 个输入框） → 开始/停止按钮 → 结果卡片（流式输出）
// 页面本身不接触推理引擎，只通过 runRequested 把组好的提示词交给 MainWindow，
// 再由 MainWindow 调用 LLMEngine 并把 token 流回灌到本页。
// ============================================================================

#include <QString>
#include <QWidget>
#include <functional>

#include "core/ThinkStreamParser.h"

class QLabel;
class QPushButton;
class QTextEdit;

namespace studentai {

class ThinkBlock;

class FunctionPage : public QWidget {
    Q_OBJECT
public:
    // 页面配置（由 MainWindow 按功能注入）
    struct Config {
        QString title;              // 页面标题
        QString subtitle;           // 说明文字
        QString inputLabel;         // 主输入框标签
        QString inputPlaceholder;   // 主输入框占位文字
        QString secondLabel;        // 第二输入框标签（为空表示不需要）
        QString secondPlaceholder;
        QString runText = QStringLiteral("开始"); // 主按钮文字
        // 由主输入与第二输入组装最终提示词
        std::function<QString(const QString &, const QString &)> buildPrompt;
    };

    explicit FunctionPage(const Config &config, QWidget *parent = nullptr);

    // ---- 供 MainWindow 驱动流式输出 ----
    void beginStreaming();                    // 清空结果区并提示生成中
    void appendPiece(const QString &piece);   // 追加一段 token 文本
    void endStreaming();                      // 收尾（恢复正常按钮状态）
    void showError(const QString &message);   // 在结果区展示错误
    void setRunning(bool running);            // 生成中禁用输入与按钮

    bool isRunning() const { return m_running; }
    QString resultText() const;

signals:
    void runRequested(const QString &prompt); // 用户点击开始，携带完整提示词
    void stopRequested();                     // 生成中点击停止

private:
    void buildUi(const Config &config);

    QTextEdit *m_input = nullptr;
    QLabel *m_secondLabelWidget = nullptr;
    QTextEdit *m_secondInput = nullptr;
    QPushButton *m_runBtn = nullptr;
    ThinkBlock *m_think = nullptr;    // 结果区上方的深度思考区块
    QTextEdit *m_result = nullptr;

    Config m_config;
    bool m_running = false;
    ThinkStreamParser m_parser;       // 把流式文本切成「思考 / 答案」两段
};

} // namespace studentai