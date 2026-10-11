#pragma once
// ============================================================================
// PromptTemplates：各功能页提示词模板
// ----------------------------------------------------------------------------
// 对话侧以「用户消息前缀」的方式注入角色与输出格式要求，不改动引擎的
// 聊天模板流程，保持推理链路简单。
// ============================================================================

#include <QString>

namespace studentai {
namespace PromptTemplates {

// 学习答疑：大学生学习助手角色，详细但不啰嗦，数学题必须带步骤
QString studyAssistant(const QString &question);

// 拍照识题：先描述图片内容再解答，看不清时明确说明
QString photoQuiz(const QString &question);

// 笔记总结：按「核心要点 / 重要细节 / 应用场景」三段式输出
QString noteSummary(const QString &text);

// 离线翻译：中英互译，保持原文格式，专业术语不瞎翻
QString translate(const QString &text);

// 错题解析：分「错误原因 / 正确解法 / 知识点总结」三部分
QString wrongQuiz(const QString &question, const QString &wrongAnswer);

} // namespace PromptTemplates
} // namespace studentai