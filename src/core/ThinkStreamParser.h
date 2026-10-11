#pragma once
// ============================================================================
// ThinkStreamParser：把「先思考后作答」的流式文本切成两个区块
// ----------------------------------------------------------------------------
// 职责一：清洗。剥离训练格式自带的对话控制标签
//   <start_of_turn> / </start_of_turn> / <end_of_turn> 及其后的角色词 user / model。
//   这些标记在 llama.cpp 解析不了模型内嵌 Jinja 模板、回退到手动拼接格式时
//   会以字面文本出现在正文里，必须在解析层彻底清掉。
//
// 职责二：分块。协议（由 PromptTemplates 注入提示词）要求模型先输出
//   <思考>…</思考> 的推理过程，再输出正式答案。
//
// 职责三：时序。严格保证「思考一定先于答案渲染」：
//   还没确认出现 <思考> 之前，答案文本只进内部缓存队列、不上屏；
//   直到出现 </思考>（思考结束）才把缓存接续放行。
//   若模型压根不按协议走，缓存超过 kDegradeChars 就降级为普通回答。
//
// 难点在于 token 是逐个片段到达的，标签很可能被切在两次 feed 之间
// （例如 "…收尾</思" + "考>答案正文…"）。这里的做法是「留尾缓冲」：
// 只有当缓冲区末尾已经不可能是任何敏感标记的前缀时，才把安全的部分吐出去，
// 因此标签即使被任意切分也不会漏判，也不会把半截标记当正文显示。
// ============================================================================

#include <QString>
#include <functional>

namespace studentai {

class ThinkStreamParser {
public:
    enum class Phase {
        WaitingThink, // 还没见到 <思考>（答案先进缓存队列，等待确认）
        InThink,      // 正在输出思考内容
        InAnswer,     // 思考已结束，正在输出正式答案
    };

    void reset();

    // 喂入一个流式片段；onThink / onAnswer 只在确有内容时被调用。
    // 注意：WaitingThink 阶段不会调用 onAnswer —— 内容先缓存，保证时序正确。
    void feed(const QString &piece,
              const std::function<void(const QString &)> &onThink,
              const std::function<void(const QString &)> &onAnswer);

    // 生成结束：把留尾缓冲里剩下的字符冲出去，避免末字丢失
    void finish(const std::function<void(const QString &)> &onThink,
                const std::function<void(const QString &)> &onAnswer);

    // 一次性剥离思考段，只取正式答案（用于历史入库 / 加载历史记录显示）。
    // 若文本里根本没有思考标签，原样返回（控制标签仍会被清洗）。
    static QString extractAnswer(const QString &raw);

    bool sawThinkTag() const { return m_sawThink; }
    Phase phase() const { return m_phase; }
    // 是否因为模型没按协议输出而放弃了「等思考」的等待
    bool degraded() const { return m_degraded; }

private:
    // 把答案缓存队列一次性交给 onAnswer（时序放行点）
    void flushAnswerQueue(const std::function<void(const QString &)> &onAnswer);

    QString m_buf;                     // 尚未确定归属的尾部缓冲
    QString m_answerQueue;             // 等待思考结束后才能上屏的答案文本
    Phase m_phase = Phase::WaitingThink;
    bool m_sawThink = false;           // 是否见到过 <思考> 开标签
    bool m_stripRole = false;          // 刚剥掉 <start_of_turn>，下一步要吃角色词
    bool m_crossed = false;            // 已越过 <end_of_turn>，后续内容全部丢弃
    bool m_degraded = false;           // 是否走了「模型不按协议」的降级路径
};

} // namespace studentai