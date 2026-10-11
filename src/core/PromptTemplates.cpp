#include "core/PromptTemplates.h"

namespace studentai {
namespace PromptTemplates {

namespace {

// 统一的「深度思考」输出协议：
// 模型先在 <思考>…</思考> 里写推理过程，再输出正式答案；
// 前端由 ThinkStreamParser 按这两个标记把流式输出切成「思考区 / 答案区」两个区块。
// 模型偶尔不遵守时不会出错：解析不到标记就整段当答案显示，思考区自动收起。
QString thinkProtocol() {
    return QStringLiteral(
        "【输出格式】请严格按两段输出：\n"
        "1. 先输出你的思考推理过程，整段用 <思考> 和 </思考> 包裹：写清你如何理解这道题、"
        "打算用什么方法、有哪些容易出错的地方，可以分点、可以自问自答；\n"
        "2. 再另起一段输出正式答案。答案部分不要再复述思考内容，也不要出现 <思考> 标签。\n\n");
}

} // namespace

QString studyAssistant(const QString &question) {
    return thinkProtocol() + QStringLiteral(
               "你是耐心的大学生学习助手。请用中文回答，讲解详细但不啰嗦，"
               "数学与计算题必须给出推导步骤和最终结果。\n\n"
               "问题：%1")
        .arg(question);
}

QString photoQuiz(const QString &question) {
    const QString ask = question.isEmpty()
                            ? QStringLiteral("请讲解这道题。")
                            : question;
    return thinkProtocol() + QStringLiteral(
               "你是大学生学习助手。请在答案部分先简要描述你在图片中看到的内容（题目文字、"
               "图表或图形），再给出解答。若图片模糊、内容不完整或你无法确定，"
               "请在答案中明确说明「我看不太清」，不要编造题目内容。"
               "数学与计算题必须给出推导步骤和最终结果。\n\n"
               "%1")
        .arg(ask);
}

QString noteSummary(const QString &text) {
    return thinkProtocol() + QStringLiteral(
               "你是大学生学习助手。请在答案部分对下面的笔记做结构化总结，严格按以下三段输出，"
               "每段用标题标明，不要输出与总结无关的开场白：\n"
               "## 核心要点\n（3~6 条，覆盖最主要的知识点）\n"
               "## 重要细节\n（容易混淆、需要记住的细节、公式或条件）\n"
               "## 应用场景\n（这些知识能用来解决什么问题，尽量结合例题或实际场景）\n\n"
               "笔记原文：\n%1")
        .arg(text);
}

QString translate(const QString &text) {
    return thinkProtocol() + QStringLiteral(
               "你是专业翻译。请翻译下面的内容：若原文是中文则翻译成英文，"
               "若原文是英文则翻译成中文。要求：\n"
               "1. 思考部分只写你判断语种、处理专业术语与保持格式的思路，不要写出完整译文；\n"
               "2. 答案部分只输出译文，保持原文的段落、列表、编号等格式，不要解释翻译过程；\n"
               "3. 专业术语使用领域内通用译法，不确定时保留原文术语并加括号说明，不要杜撰。\n\n"
               "原文：\n%1")
        .arg(text);
}

QString wrongQuiz(const QString &question, const QString &wrongAnswer) {
    const QString ans = wrongAnswer.isEmpty()
                            ? QStringLiteral("(学生未提供错误答案，请按常见错误推断可能原因)")
                            : wrongAnswer;
    return thinkProtocol() + QStringLiteral(
               "你是大学生学习助手，正在帮助学生分析错题。请在答案部分严格按以下三部分输出，"
               "用标题标明：\n"
               "## 错误原因\n（指出错误出在哪一步、属于概念错误/计算错误/审题错误中的哪一类）\n"
               "## 正确解法\n（给出完整推导步骤和最终结果）\n"
               "## 知识点总结\n（这道题涉及的知识点，以及以后避免同类错误的方法）\n\n"
               "题目：\n%1\n\n"
               "我的错误答案：\n%2")
        .arg(question, ans);
}

} // namespace PromptTemplates
} // namespace studentai