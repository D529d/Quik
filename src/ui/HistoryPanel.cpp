#include "ui/HistoryPanel.h"

#include "core/HistoryManager.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"

#include <QDate>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace studentai {

// ============================================================================
// SessionItemWidget：单个会话条目（标题 + 时间 + 悬停删除按钮 + 当前标记）
// ============================================================================
class SessionItemWidget : public QFrame {
    Q_OBJECT
public:
    SessionItemWidget(const SessionInfo &session, QWidget *parent = nullptr)
        : QFrame(parent), m_id(session.id) {
        setObjectName(QStringLiteral("sessionItem"));
        setFixedHeight(56);
        setCursor(Qt::PointingHandCursor);

        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(12, 8, 6, 8);
        lay->setSpacing(4);

        auto *textBox = new QVBoxLayout;
        textBox->setSpacing(2);

        m_title = new QLabel(this);
        m_title->setObjectName(QStringLiteral("sessionTitle"));
        QString title = session.title;
        if (title.size() > 10) {
            title = title.left(10) + QStringLiteral("…");
        }
        m_title->setText(title);

        m_time = new QLabel(session.createdAt.mid(11, 5), this);
        m_time->setObjectName(QStringLiteral("sessionTime"));

        textBox->addWidget(m_title);
        textBox->addWidget(m_time);
        lay->addLayout(textBox, 1);

        // 悬停时显示删除按钮
        m_delBtn = new QToolButton(this);
        m_delBtn->setObjectName(QStringLiteral("iconBtn"));
        m_delBtn->setFixedSize(26, 26);
        m_delBtn->setIconSize(QSize(15, 15));
        m_delBtn->setCursor(Qt::PointingHandCursor);
        m_delBtn->setToolTip(QStringLiteral("删除该会话"));
        m_delBtn->setIcon(Icons::trash(ThemeManager::instance()->iconColor(), 15));
        m_delBtn->hide();
        lay->addWidget(m_delBtn, 0, Qt::AlignVCenter);

        connect(m_delBtn, &QToolButton::clicked, this, [this]() {
            emit deleteRequested(m_id);
        });

        // 主题切换：删除图标重着色
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](const Theme &) {
            m_delBtn->setIcon(Icons::trash(ThemeManager::instance()->iconColor(), 15));
        });
    }

    void setActive(bool on) {
        setProperty("active", on);
        m_title->setProperty("active", on);
        style()->unpolish(this);
        style()->polish(this);
        m_title->style()->unpolish(m_title);
        m_title->style()->polish(m_title);
    }

signals:
    void clicked(qint64 sessionId);
    void deleteRequested(qint64 sessionId);
    void contextMenuRequested(qint64 sessionId, const QPoint &globalPos);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            emit clicked(m_id);
        }
        QFrame::mouseReleaseEvent(event);
    }

    void enterEvent(QEnterEvent *event) override {
        m_delBtn->show();
        QFrame::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        m_delBtn->hide();
        QFrame::leaveEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        emit contextMenuRequested(m_id, event->globalPos());
        event->accept();
    }

private:
    qint64 m_id = -1;
    QLabel *m_title = nullptr;
    QLabel *m_time = nullptr;
    QToolButton *m_delBtn = nullptr;
};

// ============================================================================
// HistoryPanel
// ============================================================================
HistoryPanel::HistoryPanel(QWidget *parent) : QWidget(parent) {
    setFixedWidth(260);
    setObjectName(QStringLiteral("historyPanel"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 14, 10, 10);
    layout->setSpacing(6);

    // 标题行：「历史对话」 + 「新建对话」
    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("历史对话"), this);
    title->setObjectName(QStringLiteral("panelTitle"));

    m_newBtn = new QPushButton(QStringLiteral("新建对话"), this);
    m_newBtn->setObjectName(QStringLiteral("primaryBtn"));
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->setToolTip(QStringLiteral("新建会话"));

    // 导出当前会话为 Markdown（P2 功能）
    m_exportBtn = new QPushButton(QStringLiteral("导出"), this);
    m_exportBtn->setObjectName(QStringLiteral("ghostBtn"));
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setToolTip(QStringLiteral("把当前会话导出为 Markdown 文件"));

    titleRow->addWidget(title, 1);
    titleRow->addWidget(m_exportBtn, 0);
    titleRow->addWidget(m_newBtn, 0);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("historyList"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_hint = new QLabel(QStringLiteral("暂无历史会话\n点击「新建对话」开始"), this);
    m_hint->setObjectName(QStringLiteral("panelHint"));
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);

    // 底部：收起按钮
    m_collapseBtn = new QToolButton(this);
    m_collapseBtn->setObjectName(QStringLiteral("iconBtn"));
    m_collapseBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_collapseBtn->setIconSize(QSize(16, 16));
    m_collapseBtn->setCursor(Qt::PointingHandCursor);
    m_collapseBtn->setToolTip(QStringLiteral("收起历史面板"));
    refreshCollapseIcon();

    layout->addLayout(titleRow);
    layout->addWidget(m_hint, 0);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_collapseBtn, 0, Qt::AlignLeft);

    m_list->hide();

    connect(m_newBtn, &QPushButton::clicked, this, &HistoryPanel::newSessionRequested);
    connect(m_exportBtn, &QPushButton::clicked, this, &HistoryPanel::exportRequested);
    connect(m_collapseBtn, &QToolButton::clicked, this, &HistoryPanel::collapseRequested);

    // 主题切换：收起图标重着色
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const Theme &) { refreshCollapseIcon(); });
}

void HistoryPanel::refreshCollapseIcon() {
    m_collapseBtn->setIcon(Icons::chevronRight(ThemeManager::instance()->iconColor(), 16));
    m_collapseBtn->setText(QStringLiteral("收起"));
}

namespace {

// 日期分组标签：今天 / 昨天 / 今年 MM-dd / 往年 yyyy-MM-dd
QString groupLabel(const QString &date) {
    const QDate d = QDate::fromString(date, QStringLiteral("yyyy-MM-dd"));
    if (!d.isValid()) {
        return date;
    }
    const QDate today = QDate::currentDate();
    if (d == today) {
        return QStringLiteral("今天");
    }
    if (d == today.addDays(-1)) {
        return QStringLiteral("昨天");
    }
    if (d.year() == today.year()) {
        return d.toString(QStringLiteral("MM-dd"));
    }
    return d.toString(QStringLiteral("yyyy-MM-dd"));
}

} // namespace

void HistoryPanel::setSessions(const QVector<SessionInfo> &sessions) {
    m_list->clear();
    QString lastGroup;

    for (const SessionInfo &s : sessions) {
        const QString date = s.createdAt.left(10);
        const QString group = groupLabel(date);

        // 日期分组头（不可选中/点击）
        if (group != lastGroup) {
            lastGroup = group;
            auto *hdr = new QListWidgetItem(m_list);
            hdr->setFlags(Qt::NoItemFlags);
            auto *hdrLabel = new QLabel(group, m_list);
            hdrLabel->setObjectName(QStringLiteral("groupHeader"));
            hdr->setSizeHint(hdrLabel->sizeHint());
            m_list->setItemWidget(hdr, hdrLabel);
        }

        // 会话条目
        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, s.id);
        auto *widget = new SessionItemWidget(s, m_list);
        item->setSizeHint(widget->sizeHint());
        m_list->setItemWidget(item, widget);

        connect(widget, &SessionItemWidget::clicked, this, [this](qint64 id) {
            emit sessionSelected(id);
        });
        connect(widget, &SessionItemWidget::deleteRequested, this, [this](qint64 id) {
            emit sessionDeleteRequested(id);
        });
        connect(widget, &SessionItemWidget::contextMenuRequested, this,
                [this](qint64 id, const QPoint &globalPos) {
            QMenu menu(this);
            QAction *del = menu.addAction(QStringLiteral("删除该会话"));
            if (menu.exec(globalPos) == del) {
                emit sessionDeleteRequested(id);
            }
        });
    }

    m_hint->setVisible(sessions.isEmpty());
    m_list->setVisible(!sessions.isEmpty());
}

void HistoryPanel::setActiveSession(qint64 sessionId) {
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        auto *w = qobject_cast<SessionItemWidget *>(m_list->itemWidget(item));
        if (!w) {
            continue;
        }
        w->setActive(item->data(Qt::UserRole).toLongLong() == sessionId);
        if (item->data(Qt::UserRole).toLongLong() == sessionId) {
            m_list->setCurrentItem(item);
        }
    }
}

qint64 HistoryPanel::selectedSessionId() const {
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toLongLong() : -1;
}

} // namespace studentai

#include "HistoryPanel.moc"
