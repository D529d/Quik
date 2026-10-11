#include "ui/ChatInputBar.h"

#include "core/ImageProcessor.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFocusEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

namespace studentai {

namespace {

// 剪贴板图片落盘为临时 PNG，交给附件预览与后续推理使用
QString saveTempImage(const QImage &image) {
    if (image.isNull()) {
        return QString();
    }
    const QString dir = QDir::tempPath() + QStringLiteral("/student_ai");
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/pasted_%1.png")
                                   .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    return image.save(file, "PNG") ? file : QString();
}

} // namespace

// ============================================================================
// InputTextEdit
// ============================================================================
InputTextEdit::InputTextEdit(QWidget *parent) : QTextEdit(parent) {
    setObjectName(QStringLiteral("chatInput"));
    setAcceptRichText(false);
    setPlaceholderText(QStringLiteral("输入你的问题...（Ctrl+Enter 发送，可粘贴/拖拽图片）"));
    // 占位符使用弱化色（QSS 无法设置 placeholder 颜色，用 QPalette 完成）
    QPalette pal = palette();
    pal.setColor(QPalette::PlaceholderText,
                 ThemeManager::instance()->theme().dark
                     ? QColor(0x4E, 0x59, 0x69)
                     : QColor(0xC9, 0xCD, 0xD4));
    setPalette(pal);
}

void InputTextEdit::keyPressEvent(QKeyEvent *event) {
    // Ctrl+Enter = 发送；Enter 单独按下 = 换行
    if (event->key() == Qt::Key_Return && (event->modifiers() & Qt::ControlModifier)) {
        event->accept();
        emit sendRequested();
        return;
    }
    QTextEdit::keyPressEvent(event);
}

void InputTextEdit::focusInEvent(QFocusEvent *event) {
    QTextEdit::focusInEvent(event);
    emit focusIn();
}

void InputTextEdit::focusOutEvent(QFocusEvent *event) {
    QTextEdit::focusOutEvent(event);
    emit focusOut();
}

void InputTextEdit::insertFromMimeData(const QMimeData *source) {
    // 剪贴板里是图片（截图/复制的图）时，落盘为临时文件并交给附件区展示，
    // 不把图片塞进纯文本输入框
    if (source && source->hasImage()) {
        const QImage img = qvariant_cast<QImage>(source->imageData());
        const QString path = saveTempImage(img);
        if (!path.isEmpty()) {
            emit imagePasted(path);
            return;
        }
    }
    QTextEdit::insertFromMimeData(source);
}

// ============================================================================
// ChatInputBar
// ============================================================================
ChatInputBar::ChatInputBar(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("inputBarContainer"));
    setAcceptDrops(true); // 支持拖拽图片到输入区

    // 外层：左右留白，上下留间距（不贴死窗口底）
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 6, 24, 16);

    // 悬浮卡片：居中、宽度 80%
    auto *centerRow = new QHBoxLayout;
    centerRow->setContentsMargins(0, 0, 0, 0);
    centerRow->addStretch(1);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("inputCard"));
    centerRow->addWidget(m_card, 0);
    centerRow->addStretch(1);
    outer->addLayout(centerRow);

    // 柔和弥散阴影
    auto *shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(ThemeManager::instance()->theme().dark
                         ? QColor(0, 0, 0, 90)
                         : QColor(0, 0, 0, 22));
    m_card->setGraphicsEffect(shadow);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(10, 8, 10, 8);
    cardLayout->setSpacing(6);

    // ---- 附件预览条（默认隐藏）----
    m_previewRow = new QWidget(m_card);
    m_previewRow->setObjectName(QStringLiteral("attachRow"));
    auto *previewLayout = new QHBoxLayout(m_previewRow);
    previewLayout->setContentsMargins(4, 0, 4, 0);
    previewLayout->setSpacing(8);

    m_previewThumb = new QLabel(m_previewRow);
    m_previewThumb->setObjectName(QStringLiteral("attachThumb"));
    m_previewThumb->setFixedSize(52, 52);
    m_previewThumb->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewThumb, 0, Qt::AlignVCenter);

    m_previewName = new QLabel(m_previewRow);
    m_previewName->setObjectName(QStringLiteral("attachName"));
    previewLayout->addWidget(m_previewName, 1);

    m_previewClose = new QToolButton(m_previewRow);
    m_previewClose->setObjectName(QStringLiteral("attachClose"));
    m_previewClose->setFixedSize(24, 24);
    m_previewClose->setIconSize(QSize(14, 14));
    m_previewClose->setCursor(Qt::PointingHandCursor);
    m_previewClose->setToolTip(QStringLiteral("移除图片"));
    previewLayout->addWidget(m_previewClose, 0, Qt::AlignVCenter);

    m_previewRow->hide();
    cardLayout->addWidget(m_previewRow);

    // ---- 输入行 ----
    auto *lay = new QHBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    // 左侧：拍照/上传图片按钮
    m_imageBtn = new QToolButton(m_card);
    m_imageBtn->setObjectName(QStringLiteral("imageBtn"));
    m_imageBtn->setFixedSize(40, 40);
    m_imageBtn->setIconSize(QSize(22, 22));
    m_imageBtn->setCursor(Qt::PointingHandCursor);
    m_imageBtn->setToolTip(QStringLiteral("上传图片（也可直接粘贴或拖拽图片）"));
    lay->addWidget(m_imageBtn, 0, Qt::AlignBottom);

    // 输入框
    m_input = new InputTextEdit(m_card);
    m_input->setMinimumHeight(52);
    m_input->setMaximumHeight(104);
    lay->addWidget(m_input, 1);

    // 右侧：圆形品牌色发送按钮
    m_sendBtn = new QPushButton(m_card);
    m_sendBtn->setObjectName(QStringLiteral("sendBtn"));
    m_sendBtn->setFixedSize(40, 40);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_sendBtn->setIcon(Icons::send(Qt::white, 20));
    m_sendBtn->setIconSize(QSize(20, 20));
    m_sendBtn->setToolTip(QStringLiteral("发送（Ctrl+Enter）"));
    lay->addWidget(m_sendBtn, 0, Qt::AlignBottom);

    cardLayout->addLayout(lay);

    refreshIcons();

    // 聚焦：输入卡片出现品牌色柔光边框
    connect(m_input, &InputTextEdit::focusIn, this, [this]() {
        m_card->setProperty("focused", true);
        m_card->style()->unpolish(m_card);
        m_card->style()->polish(m_card);
    });
    connect(m_input, &InputTextEdit::focusOut, this, [this]() {
        m_card->setProperty("focused", false);
        m_card->style()->unpolish(m_card);
        m_card->style()->polish(m_card);
    });

    // Ctrl+Enter 发送
    connect(m_input, &InputTextEdit::sendRequested, this, [this]() {
        const QString t = m_input->toPlainText().trimmed();
        // 允许「只有图片没有文字」的发送
        if (!t.isEmpty() || hasImage()) {
            emit sendRequested(t);
        }
    });

    // 粘贴图片 -> 作为附件
    connect(m_input, &InputTextEdit::imagePasted, this, [this](const QString &path) {
        setImageAttachment(path);
    });

    // 图片按钮 -> 选图
    connect(m_imageBtn, &QToolButton::clicked, this, &ChatInputBar::openImageDialog);

    // 移除附件
    connect(m_previewClose, &QToolButton::clicked, this,
            &ChatInputBar::clearImageAttachment);

    // 发送/停止
    connect(m_sendBtn, &QPushButton::clicked, this, [this]() {
        if (m_generating) {
            emit stopRequested();
            return;
        }
        const QString t = m_input->toPlainText().trimmed();
        if (!t.isEmpty() || hasImage()) {
            emit sendRequested(t);
        }
    });

    // 主题变化：图标重着色
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const Theme &) { refreshIcons(); });
}

void ChatInputBar::refreshIcons() {
    const QColor normal = ThemeManager::instance()->iconColor();
    m_imageBtn->setIcon(Icons::image(normal, 22));
    m_previewClose->setIcon(Icons::close(normal, 14));
    // 发送/停止图标为白色，固定不变
    m_sendBtn->setIcon(m_generating ? Icons::stop(Qt::white, 20)
                                    : Icons::send(Qt::white, 20));
}

void ChatInputBar::openImageDialog() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择图片"), QString(), ImageProcessor::supportedExtensions());
    if (path.isEmpty()) {
        return;
    }
    setImageAttachment(path);
}

void ChatInputBar::setImageAttachment(const QString &path) {
    if (!ImageProcessor::isLoadable(path)) {
        return; // 非法/损坏图片：忽略，保持原状
    }
    m_imagePath = path;
    m_previewThumb->setPixmap(ImageProcessor::makeThumbnail(path, 96));
    m_previewName->setText(QFileInfo(path).fileName());
    updateAttachmentUi();
    emit imageAttached(path);
}

void ChatInputBar::clearImageAttachment() {
    if (m_imagePath.isEmpty()) {
        return;
    }
    m_imagePath.clear();
    m_previewThumb->clear();
    m_previewName->clear();
    updateAttachmentUi();
    emit imageCleared();
}

void ChatInputBar::updateAttachmentUi() {
    const bool has = !m_imagePath.isEmpty();
    m_previewRow->setVisible(has);
    m_imageBtn->setToolTip(has ? QStringLiteral("更换图片")
                               : QStringLiteral("上传图片（也可直接粘贴或拖拽图片）"));
}

void ChatInputBar::dragEnterEvent(QDragEnterEvent *event) {
    // 拖入内容含图片文件 URL 时接收
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && ImageProcessor::isLoadable(url.toLocalFile())) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    if (event->mimeData()->hasImage()) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void ChatInputBar::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && ImageProcessor::isLoadable(url.toLocalFile())) {
                setImageAttachment(url.toLocalFile());
                event->acceptProposedAction();
                return;
            }
        }
    }
    if (event->mimeData()->hasImage()) {
        const QString path =
            saveTempImage(qvariant_cast<QImage>(event->mimeData()->imageData()));
        if (!path.isEmpty()) {
            setImageAttachment(path);
            event->acceptProposedAction();
            return;
        }
    }
    QWidget::dropEvent(event);
}

void ChatInputBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 卡片宽度 = 主区域 80%（不低于 320px）
    m_card->setFixedWidth(qMax(static_cast<int>(width() * 0.8), 320));
}

void ChatInputBar::setGenerating(bool generating) {
    m_generating = generating;
    refreshIcons();
    m_sendBtn->setToolTip(generating ? QStringLiteral("停止生成")
                                     : QStringLiteral("发送（Ctrl+Enter）"));
}

QString ChatInputBar::text() const {
    return m_input->toPlainText().trimmed();
}

void ChatInputBar::clear() {
    m_input->clear();
}

void ChatInputBar::focusInput() {
    m_input->setFocus();
}

void ChatInputBar::promptForImage() {
    openImageDialog();
}

} // namespace studentai