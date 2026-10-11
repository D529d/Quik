#pragma once
// ============================================================================
// HistoryPanel：右侧历史记录面板（固定 260px，可收起）
// - 顶部：标题「历史对话」+「新建对话」主色小按钮
// - 列表：按日期分组（今天/昨天/日期），每条显示摘要 + 时间
// - hover 高亮并显示删除按钮；当前会话左侧品牌色竖条标记
// - 底部：收起按钮（隐藏面板后对话区铺满宽度）
// ============================================================================

#include <QVector>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QToolButton;

namespace studentai {

struct SessionInfo;

class HistoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPanel(QWidget *parent = nullptr);

    // 刷新会话列表（全量重建，含日期分组）
    void setSessions(const QVector<SessionInfo> &sessions);
    // 高亮当前会话（-1 表示无）
    void setActiveSession(qint64 sessionId);
    qint64 selectedSessionId() const;

signals:
    void newSessionRequested();                 // 点击"新建对话"
    void sessionSelected(qint64 sessionId);     // 点击某个会话
    void sessionDeleteRequested(qint64 sessionId); // 删除会话
    void exportRequested();                     // 导出当前会话为 Markdown
    void collapseRequested();                   // 点击"收起"

private:
    void refreshCollapseIcon(); // 主题切换时重着收起图标

    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QToolButton *m_collapseBtn = nullptr;
};

} // namespace studentai
