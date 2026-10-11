#include "core/ThinkStreamParser.h"

#include <QRegularExpression>
#include <QStringList>

namespace studentai {

namespace {

// 「深度思考」协议的两个标记，用中文尖括号包裹，模型基本不会在正文里自然产出
const QString kOpenTag  = QStringLiteral("<思考>");
const QString kCloseTag = QStringLiteral("</思考>");

// 训练格式自带的对话控制标签。按长度倒序存放，保证优先匹配更长的串
// （否则 "<start_of_turn>" 会被 "</start_of_turn>" 的部分匹配干扰）。
const QStringList kControlTags = {
    QStringLiteral("</start_of_turn>"),
    QStringLiteral("<end_of_turn>"),
    QStringLiteral("<start_of_turn>"),
};
const QString kRoleUser  = QStringLiteral("user");
const QString kRoleModel = QStringLiteral("model");

// 等待 <思考> 的上限：模型完全不理协议时，攒够这么多答案字符就放弃等待，
// 直接把内容当普通回答放行，避免答案区长时间空着。
constexpr int kDegradeChars = 160;

// 返回 buf 末尾「可能是 tag 前缀」的字符数（0 表示末尾没有半截标签）
int heldBackLength(const QString &buf, const QString &tag) {
    const int maxLen = qMin(buf.size(), tag.size() - 1);
    for (int len = maxLen; len > 0; --len) {
        if (buf.endsWith(tag.left(len))) {
            return len;
        }
    }
    return 0;
}

// 缓冲区末尾需要留住的字符数：只要末尾可能是任一敏感标记的前缀就不能吐出去
int sensitiveHeldBack(const QString &buf, const QStringList &tags, const QString &expect) {
    int hold = 0;
    for (const QString &t : tags) {
        hold = qMax(hold, heldBackLength(buf, t));
    }
    if (!expect.isEmpty()) {
        hold = qMax(hold, heldBackLength(buf, expect));
    }
    return hold;
}

// 清洗一块正文：把 3 个及以上连续换行压成空行分隔（Markdown 段落仍然正常）
QString tidy(const QString &s) {
    QString r = s;
    static const QRegularExpression kBlankRun(QStringLiteral("\n{3,}"));
    r.replace(kBlankRun, QStringLiteral("\n\n"));
    return r;
}

} // namespace

void ThinkStreamParser::reset() {
    m_buf.clear();
    m_answerQueue.clear();
    m_phase = Phase::WaitingThink;
    m_sawThink = false;
    m_stripRole = false;
    m_crossed = false;
    m_degraded = false;
}

void ThinkStreamParser::flushAnswerQueue(const std::function<void(const QString &)> &onAnswer) {
    if (m_answerQueue.isEmpty()) {
        return;
    }
    const QString out = tidy(m_answerQueue);
    m_answerQueue.clear();
    if (!out.isEmpty()) {
        onAnswer(out);
    }
}

void ThinkStreamParser::feed(const QString &piece,
                             const std::function<void(const QString &)> &onThink,
                             const std::function<void(const QString &)> &onAnswer) {
    m_buf += piece;

    // 正文落地：思考阶段进思考区，答案阶段进答案区；
    // WaitingThink 阶段（还没确认思考开始）一律先压入答案缓存、不上屏，
    // 这样就能严格保证「思考过程一定先于最终答案渲染」。
    auto deliver = [this, &onThink, &onAnswer](const QString &text) {
        if (text.isEmpty()) {
            return;
        }
        const QString out = tidy(text);
        if (out.isEmpty()) {
            return;
        }
        switch (m_phase) {
            case Phase::InThink:
                onThink(out);
                break;
            case Phase::InAnswer:
                onAnswer(out);
                break;
            case Phase::WaitingThink:
                m_answerQueue += out;
                break;
        }
    };

    // 一次 feed 的文本里可能同时出现多个标记，因此循环推进状态
    while (true) {
        if (m_crossed) {
            // 已越过 <end_of_turn>：后面都是模型臆造的下一轮对话，全部丢弃
            m_buf.clear();
            return;
        }

        // (A) 上一轮剥掉了 <start_of_turn>，这里再把紧跟的角色词 user / model 吃掉
        if (m_stripRole) {
            const int nl = m_buf.indexOf(QLatin1Char('\n'));
            if (nl < 0) {
                if (m_buf.size() < 12) {
                    return; // 角色词还没到齐，留着等下一片
                }
                m_stripRole = false; // 异常：迟迟没有换行，放弃剥离，按正文处理
            } else {
                const QString head = m_buf.left(nl).trimmed();
                if (head == kRoleUser || head == kRoleModel) {
                    m_buf = m_buf.mid(nl + 1);
                }
                m_stripRole = false;
                continue;
            }
        }

        // (B) 找最早出现的「敏感标记」：控制标签 + 当前阶段期待的思考标签
        const QString expect = (m_phase == Phase::WaitingThink) ? kOpenTag
                             : (m_phase == Phase::InThink)      ? kCloseTag
                                                                : QString();
        int edge = -1;
        bool edgeIsControl = false;
        QString edgeTag;
        for (const QString &t : kControlTags) {
            const int i = m_buf.indexOf(t);
            if (i >= 0 && (edge < 0 || i < edge)) {
                edge = i;
                edgeTag = t;
                edgeIsControl = true;
            }
        }
        if (!expect.isEmpty()) {
            const int i = m_buf.indexOf(expect);
            if (i >= 0 && (edge < 0 || i < edge)) {
                edge = i;
                edgeTag = expect;
                edgeIsControl = false;
            }
        }

        if (edge < 0) {
            // 没有完整标记：只放行「绝不可能组成标记前缀」的安全部分
            const int hold = sensitiveHeldBack(m_buf, kControlTags, expect);
            const int safeLen = m_buf.size() - hold; // 注意不能叫 emit：那是 Qt 宏
            if (safeLen > 0) {
                deliver(m_buf.left(safeLen));
                m_buf = m_buf.mid(safeLen);
            }
            break; // 剩下的要么是半截标记，要么已清空
        }

        // 标记之前的那段是正文
        if (edge > 0) {
            deliver(m_buf.left(edge));
        }
        m_buf = m_buf.mid(edge + edgeTag.size());

        if (edgeIsControl) {
            if (edgeTag == QStringLiteral("<end_of_turn>")) {
                // 模型越过了本轮，开始臆造后续对话：立即截断，不再上屏任何内容
                m_crossed = true;
                m_buf.clear();
                return;
            }
            if (edgeTag == QStringLiteral("<start_of_turn>")) {
                m_stripRole = true; // 下一次循环吃掉 user / model
            }
            continue; // 控制标签本身绝不进入正文
        }

        // ---- 思考协议标记 ----
        if (edgeTag == kOpenTag) {
            m_sawThink = true;
            m_phase = Phase::InThink; // 进入思考阶段，答案继续留在缓存里
        } else { // kCloseTag
            m_phase = Phase::InAnswer;
            // ★ 关键时序节点：思考结束，才把此前缓存的答案放行，
            //   保证「思考区先渲染完 → 答案区才开始出字」
            flushAnswerQueue(onAnswer);
        }
    }

    // (C) 降级：模型压根没按协议思考，攒够一定长度仍未见 <思考>
    if (m_phase == Phase::WaitingThink && m_answerQueue.size() >= kDegradeChars) {
        m_degraded = true;
        m_phase = Phase::InAnswer;
        flushAnswerQueue(onAnswer);
    }
}

void ThinkStreamParser::finish(const std::function<void(const QString &)> &onThink,
                               const std::function<void(const QString &)> &onAnswer) {
    // 末尾残留的角色词直接丢掉，不要当成正文渲染
    if (m_stripRole) {
        const int nl = m_buf.indexOf(QLatin1Char('\n'));
        const QString head = (nl < 0 ? m_buf : m_buf.left(nl)).trimmed();
        if (head == kRoleUser || head == kRoleModel) {
            m_buf = (nl < 0) ? QString() : m_buf.mid(nl + 1);
        }
        m_stripRole = false;
    }
    if (m_crossed) {
        m_buf.clear();
        m_answerQueue.clear();
        return;
    }

    if (m_phase == Phase::WaitingThink) {
        // 全程没出现 <思考>：模型没按协议走，缓存内容当普通答案一次性放行。
        // 此时 sawThinkTag() 仍为 false，UI 会收起思考区块、退回普通回答样式。
        if (!m_buf.isEmpty()) {
            m_answerQueue += m_buf;
            m_buf.clear();
        }
        flushAnswerQueue(onAnswer);
        return;
    }

    if (m_phase == Phase::InThink) {
        // 思考被截断（没等到 </思考>）：剩余部分仍归思考区
        if (!m_buf.isEmpty()) {
            onThink(tidy(m_buf));
            m_buf.clear();
        }
        flushAnswerQueue(onAnswer);
        return;
    }

    // InAnswer：留尾缓冲里的都是正式答案
    if (!m_buf.isEmpty()) {
        onAnswer(tidy(m_buf));
        m_buf.clear();
    }
}

QString ThinkStreamParser::extractAnswer(const QString &raw) {
    ThinkStreamParser p;
    QString answer;
    const auto noop = [](const QString &) {};
    const auto collect = [&answer](const QString &piece) { answer += piece; };
    p.feed(raw, noop, collect);
    p.finish(noop, collect);
    return answer;
}

} // namespace studentai