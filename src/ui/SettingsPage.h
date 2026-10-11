#pragma once
// ============================================================================
// SettingsPage：设置页
// - 「模型」卡片：当前 GGUF 模型路径 + 更换模型；视觉编码器(mmproj)路径配置
// - 「生成参数」卡片：温度 / Top-P / 上下文长度
// - 「外观」卡片：主题色 / 界面密度 / 毛玻璃效果 / AI 气泡样式
// 所有修改通过信号上抛，由 MainWindow 统一持久化与应用
// ============================================================================

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QScrollArea;
class QToolButton;

namespace studentai {

struct Theme;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

    // 外部主题变化时同步控件显示（不触发信号）
    void setThemeValues(const Theme &theme);
    void setModelPath(const QString &path);
    // 同步视觉编码器路径显示（不触发信号）
    void setMmprojPath(const QString &path);
    // 同步生成参数显示（不触发信号）
    void setSamplingValues(double temperature, double topP, int nCtx);

signals:
    void brandColorChanged(const QColor &color);     // 主题色
    void densityChanged(int density);                // 0紧凑/1标准/2宽松
    void glassToggled(bool on);                      // 毛玻璃
    void bubbleStyleChanged(bool filled);            // AI 气泡填色/描边
    void modelChangeRequested(const QString &path);  // 更换模型
    void mmprojChangeRequested(const QString &path); // 更换视觉编码器(mmproj)
    // 采样参数变化：温度/Top-P 可即时生效，无需重载模型
    void samplingChanged(double temperature, double topP);
    // 上下文长度变化：需要重建上下文，由 MainWindow 重新加载模型
    void contextLengthChanged(int nCtx);

private:
    QWidget *buildModelCard();
    QWidget *buildGenerationCard();
    QWidget *buildAppearanceCard();
    void refreshSwatchChecks();

    QList<QToolButton *> m_colorBtns; // 主题色板（顺序与预设色一致）
    QComboBox *m_density = nullptr;
    QCheckBox *m_glass = nullptr;
    QComboBox *m_bubbleStyle = nullptr;
    QLabel *m_modelPath = nullptr;
    QLabel *m_mmprojPath = nullptr;
    QDoubleSpinBox *m_temperature = nullptr;
    QDoubleSpinBox *m_topP = nullptr;
    QComboBox *m_nCtx = nullptr;
};

} // namespace studentai
