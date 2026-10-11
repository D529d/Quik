#pragma once
// ============================================================================
// ChatInputBar：底部悬浮输入区（Step5 增加图片附件）
// - 输入卡片居中，宽度占主区域 80%，大圆角 + 柔和阴影，不与窗口底贴死
// - 左侧：拍照/上传图片按钮
// - 输入框：Ctrl+Enter 发送、Enter 换行；聚焦时输入卡片出现品牌色柔光边框
// - 右侧：圆形品牌色发送按钮（生成中切换为"停止"）
// - 图片附件：点击按钮选图 / Ctrl+V 粘贴图片 / 拖拽图片到输入区，
//             选中后在输入框上方显示缩略图预览，可点击 × 移除
// ============================================================================

#include <QTextEdit>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QToolButton;

namespace studentai {

// 多行输入框子类：拦截 Ctrl+Enter 触发发送，上报聚焦状态，
// 并支持直接粘贴剪贴板图片（保存为临时文件后交由输入栏预览）
class InputTextEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit InputTextEdit(QWidget *parent = nullptr);

signals:
    void sendRequested();          // 用户按下 Ctrl+Enter
    void focusIn();                // 输入框获得焦点
    void focusOut();               // 输入框失去焦点
    void imagePasted(const QString &path); // 粘贴了图片（已存为临时文件）

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void insertFromMimeData(const QMimeData *source) override;
};

// 悬浮输入区（图片附件预览 + 图片按钮 + 输入框 + 发送/停止按钮）
class ChatInputBar : public QWidget {
    Q_OBJECT
public:
    explicit ChatInputBar(QWidget *parent = nullptr);

    void setGenerating(bool generating); // true=停止，false=发送
    QString text() const;
    void clear();
    void focusInput();
    // 弹出选图对话框（供"拍照识题"导航入口调用）
    void promptForImage();

    // ---- 图片附件 ----
    QString imagePath() const { return m_imagePath; }
    bool hasImage() const { return !m_imagePath.isEmpty(); }
    // 设置附件（加载失败时自动忽略并清空）
    void setImageAttachment(const QString &path);
    void clearImageAttachment();

signals:
    void sendRequested(const QString &text);
    void stopRequested();
    void imageAttached(const QString &path); // 新增图片附件
    void imageCleared();                     // 移除图片附件

protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void refreshIcons();            // 主题切换时重着图片按钮图标
    void openImageDialog();         // 打开本地选图对话框
    void updateAttachmentUi();      // 依据当前附件刷新预览区显隐

    QFrame *m_card = nullptr;
    QWidget *m_previewRow = nullptr;   // 附件预览条
    QLabel *m_previewThumb = nullptr;  // 缩略图
    QLabel *m_previewName = nullptr;   // 文件名
    QToolButton *m_previewClose = nullptr;
    InputTextEdit *m_input = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QToolButton *m_imageBtn = nullptr;

    QString m_imagePath;   // 当前附件路径（空 = 无附件）
    bool m_generating = false;
};

} // namespace studentai