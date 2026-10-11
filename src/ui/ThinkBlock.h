#pragma once
// ============================================================================
// ThinkBlock：AI 回答里的「思考过程」区块（可折叠）
// ----------------------------------------------------------------------------
// 结构：
//   标题栏  ——  左侧 DotMatrixLoader 动画（思考中）/ 绿色对勾（已完成）
//               + 状态文本（Thinking... / 思考完成 用时 X.Xs）
//               + 右侧折叠箭头
//   内容区  ——  流式输出的推理文本（次级灰、斜体），可平滑折叠
//
// 状态流转：
//   begin()             → 显示动画 + 「Thinking...」，计时器按 0.1s 累加
//   appendThink()       → 逐字追加推理文本
//   markAnswerStarted() → 动画换绿色对勾，标题变「思考完成 用时 X.Xs」
//   finish()            → 收尾（被中断时兜底走上一个状态）
//   showNoThink()       → 模型未按协议输出，整块隐藏并停掉动画
// ============================================================================

#include <QColor>
#include <QElapsedTimer>
#include <QWidget>

class QLabel;
class QTimer;
class QToolButton;
class QVBoxLayout;
class QPropertyAnimation;

namespace studentai {

// ---------------------------------------------------------------------------
// DotMatrixLoader：7x7 点阵三角形动画（QPainter 原生实现）
// 还原自 shadcn 组件 @dotmatrix/dotm-triangle-19「Pivot Ray」：
// 三角掩码内 10 个点，以中心格为轴心，按各点极角做高斯楔形取亮，
// 形成一束绕轴心旋转的探照灯光带，同时中心格自身缓慢呼吸。
// ---------------------------------------------------------------------------
class DotMatrixLoader : public QWidget {
public:
    explicit DotMatrixLoader(QWidget *parent = nullptr);

    void setDotColor(const QColor &color);
    // 扫描周期（毫秒），对应组件的 cycleMsBase=1400 与 speed 参数
    void setCycleMs(int ms);
    void start();
    void stop();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void tick();

    QTimer *m_timer = nullptr;
    QColor m_color;
    int m_cycleMs = 1400;
    qreal m_phase = 0.0; // 0..1 的循环相位
};

// ---------------------------------------------------------------------------
// ThinkBlock：点阵动画 + 状态文案 + 可折叠推理正文
// ---------------------------------------------------------------------------
class ThinkBlock : public QWidget {
public:
    explicit ThinkBlock(QWidget *parent = nullptr);

    void begin();                                   // 进入「思考中」
    void appendThink(const QString &piece);         // 追加推理文本
    void markAnswerStarted();                       // 进入「思考完成」
    void finish();                                  // 生成结束（未走完上一步时兜底）
    void showNoThink();                             // 无思考内容：整块隐藏

    bool hasThinkText() const { return !m_thinkText.isEmpty(); }
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed, bool animated = true);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void refreshElapsed();
    void applyTheme();
    void updateContentText();
    void expandAnimated(bool animated);
    void collapseAnimated(bool animated);

    DotMatrixLoader *m_loader = nullptr;  // 思考中的点阵动画
    QLabel *m_check = nullptr;            // 完成态的绿色对勾
    QLabel *m_title = nullptr;            // 状态文本
    QToolButton *m_toggle = nullptr;      // 折叠箭头
    QWidget *m_header = nullptr;          // 可点击标题栏
    QWidget *m_contentWrap = nullptr;     // 折叠动画作用对象
    QLabel *m_content = nullptr;          // 推理正文

    QTimer *m_tick = nullptr;             // 0.1s 刷新用时
    QElapsedTimer m_elapsed;
    QPropertyAnimation *m_anim = nullptr;

    QString m_thinkText;
    bool m_collapsed = false;
    bool m_finished = false;
    bool m_everStarted = false;
};

} // namespace studentai