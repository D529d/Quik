#include "ui/SettingsPage.h"

#include "ui/ThemeManager.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace studentai {

namespace {

// 预设主题色（科技蓝 / 神秘紫 / 清新绿 / 活力橙 / 热情红）
const QColor kBrandPresets[] = {
    QColor(0x16, 0x5D, 0xFF),
    QColor(0x72, 0x2E, 0xD1),
    QColor(0x00, 0xB4, 0x2A),
    QColor(0xFF, 0x7D, 0x00),
    QColor(0xF5, 0x3F, 0x3F),
};

const QString kDensityNames[] = {
    QStringLiteral("紧凑"), QStringLiteral("标准"), QStringLiteral("宽松"),
};

} // namespace

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("chatContainer")); // 继承聊天区底色
    // QSS polish 会重置 autoFillBackground，主题切换后再强制应用，保证深色下跟随主题
    const auto applyContentBg = [content]() {
        // 以应用调色板为基准重新取整份调色板：控件一旦 setPalette 就不再跟随
        // 应用调色板，若沿用自身调色板只改 Window，其它角色会停留在旧主题值上
        QPalette p = QApplication::palette();
        p.setColor(QPalette::Window, ThemeManager::instance()->backgroundColor());
        content->setPalette(p);
        content->setAutoFillBackground(true);
    };
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, content, applyContentBg);
    auto *lay = new QVBoxLayout(content);
    lay->setContentsMargins(32, 28, 32, 28);
    lay->setSpacing(16);

    lay->addWidget(buildModelCard());
    lay->addWidget(buildGenerationCard());
    lay->addWidget(buildAppearanceCard());
    lay->addStretch(1);

    scroll->setWidget(content);
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(scroll);
}

QWidget *SettingsPage::buildModelCard() {
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingCard"));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("模型"), card);
    title->setObjectName(QStringLiteral("settingTitle"));
    lay->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("本地 GGUF 模型，完全离线运行（设置后下次启动自动加载）"), card);
    hint->setObjectName(QStringLiteral("settingHint"));
    hint->setWordWrap(true);
    lay->addWidget(hint);

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    m_modelPath = new QLabel(QStringLiteral("未选择"), card);
    m_modelPath->setObjectName(QStringLiteral("settingHint"));
    m_modelPath->setWordWrap(true);
    m_modelPath->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *btn = new QPushButton(QStringLiteral("更换模型"), card);
    btn->setObjectName(QStringLiteral("ghostBtn"));
    btn->setCursor(Qt::PointingHandCursor);
    connect(btn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择 GGUF 模型文件"), QString(),
            QStringLiteral("GGUF 模型 (*.gguf)"));
        if (!path.isEmpty()) {
            m_modelPath->setText(path);
            emit modelChangeRequested(path);
        }
    });

    row->addWidget(m_modelPath, 1);
    row->addWidget(btn, 0, Qt::AlignTop);
    lay->addLayout(row);

    // ---- 视觉编码器（mmproj）：拍照识题 / 多模态图片识别所需 ----
    auto *mmHint = new QLabel(
        QStringLiteral("视觉编码器（mmproj）：拍照识题所需的图像编码模型，"
                       "与主模型配套，不配置则无法识别图片"), card);
    mmHint->setObjectName(QStringLiteral("settingHint"));
    mmHint->setWordWrap(true);
    lay->addWidget(mmHint);

    auto *mmRow = new QHBoxLayout;
    mmRow->setSpacing(10);
    m_mmprojPath = new QLabel(QStringLiteral("未选择"), card);
    m_mmprojPath->setObjectName(QStringLiteral("settingHint"));
    m_mmprojPath->setWordWrap(true);
    m_mmprojPath->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *mmBtn = new QPushButton(QStringLiteral("选择 mmproj"), card);
    mmBtn->setObjectName(QStringLiteral("ghostBtn"));
    mmBtn->setCursor(Qt::PointingHandCursor);
    connect(mmBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择视觉编码器文件（mmproj）"), QString(),
            QStringLiteral("GGUF 模型 (*.gguf)"));
        if (!path.isEmpty()) {
            m_mmprojPath->setText(path);
            emit mmprojChangeRequested(path);
        }
    });

    mmRow->addWidget(m_mmprojPath, 1);
    mmRow->addWidget(mmBtn, 0, Qt::AlignTop);
    lay->addLayout(mmRow);
    return card;
}

QWidget *SettingsPage::buildGenerationCard() {
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingCard"));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("生成参数"), card);
    title->setObjectName(QStringLiteral("settingTitle"));
    lay->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("温度越高回答越随机，答疑建议 0.7；Top-P 越小候选越集中。"
                       "上下文长度决定能记住多长的对话，修改后会自动重新加载模型"
                       "（数值越大越占内存，8GB 内存建议不超过 8192）"), card);
    hint->setObjectName(QStringLiteral("settingHint"));
    hint->setWordWrap(true);
    lay->addWidget(hint);

    // ---- 温度 ----
    auto *tempRow = new QHBoxLayout;
    tempRow->setSpacing(12);
    auto *tempLabel = new QLabel(QStringLiteral("温度"), card);
    tempLabel->setObjectName(QStringLiteral("settingLabel"));
    m_temperature = new QDoubleSpinBox(card);
    m_temperature->setObjectName(QStringLiteral("settingSpin"));
    m_temperature->setRange(0.0, 2.0);
    m_temperature->setSingleStep(0.1);
    m_temperature->setDecimals(1);
    m_temperature->setFixedWidth(100);
    tempRow->addWidget(tempLabel);
    tempRow->addWidget(m_temperature, 0, Qt::AlignLeft);
    tempRow->addStretch(1);
    lay->addLayout(tempRow);

    // ---- Top-P ----
    auto *topPRow = new QHBoxLayout;
    topPRow->setSpacing(12);
    auto *topPLabel = new QLabel(QStringLiteral("Top-P"), card);
    topPLabel->setObjectName(QStringLiteral("settingLabel"));
    m_topP = new QDoubleSpinBox(card);
    m_topP->setObjectName(QStringLiteral("settingSpin"));
    m_topP->setRange(0.05, 1.0);
    m_topP->setSingleStep(0.05);
    m_topP->setDecimals(2);
    m_topP->setFixedWidth(100);
    topPRow->addWidget(topPLabel);
    topPRow->addWidget(m_topP, 0, Qt::AlignLeft);
    topPRow->addStretch(1);
    lay->addLayout(topPRow);

    // ---- 上下文长度 ----
    auto *ctxRow = new QHBoxLayout;
    ctxRow->setSpacing(12);
    auto *ctxLabel = new QLabel(QStringLiteral("上下文长度"), card);
    ctxLabel->setObjectName(QStringLiteral("settingLabel"));
    m_nCtx = new QComboBox(card);
    const int ctxPresets[] = {2048, 4096, 8192, 16384};
    for (int v : ctxPresets) {
        m_nCtx->addItem(QStringLiteral("%1 tokens").arg(v), v);
    }
    ctxRow->addWidget(ctxLabel);
    ctxRow->addWidget(m_nCtx, 0, Qt::AlignLeft);
    ctxRow->addStretch(1);
    lay->addLayout(ctxRow);

    // 温度/Top-P 改动即可生效（引擎每次生成前会重建采样链）
    const auto emitSampling = [this]() {
        emit samplingChanged(m_temperature->value(), m_topP->value());
    };
    connect(m_temperature, &QDoubleSpinBox::valueChanged, this, [emitSampling](double) {
        emitSampling();
    });
    connect(m_topP, &QDoubleSpinBox::valueChanged, this, [emitSampling](double) {
        emitSampling();
    });
    // 上下文长度改动需要重建上下文（由 MainWindow 重新加载模型）
    connect(m_nCtx, &QComboBox::currentIndexChanged, this, [this](int) {
        emit contextLengthChanged(m_nCtx->currentData().toInt());
    });

    return card;
}

QWidget *SettingsPage::buildAppearanceCard() {
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingCard"));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(20, 16, 20, 16);
    lay->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("外观"), card);
    title->setObjectName(QStringLiteral("settingTitle"));
    lay->addWidget(title);

    // ---- 主题色 ----
    auto *colorLabel = new QLabel(QStringLiteral("主题色"), card);
    colorLabel->setObjectName(QStringLiteral("settingLabel"));
    lay->addWidget(colorLabel);

    auto *swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(10);
    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    const int n = static_cast<int>(sizeof(kBrandPresets) / sizeof(kBrandPresets[0]));
    for (int i = 0; i < n; ++i) {
        auto *btn = new QToolButton(card);
        btn->setObjectName(QStringLiteral("colorSwatch"));
        btn->setFixedSize(30, 30);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(kBrandPresets[i].name(QColor::HexRgb).toUpper());
        group->addButton(btn, i);
        swatchRow->addWidget(btn);
        m_colorBtns.append(btn);
    }
    swatchRow->addStretch(1);
    lay->addLayout(swatchRow);
    refreshSwatchChecks();

    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        if (id >= 0 && id < n) {
            emit brandColorChanged(kBrandPresets[id]);
        }
    });

    // ---- 界面密度 ----
    auto *densityRow = new QHBoxLayout;
    densityRow->setSpacing(12);
    auto *densityLabel = new QLabel(QStringLiteral("界面密度"), card);
    densityLabel->setObjectName(QStringLiteral("settingLabel"));
    m_density = new QComboBox(card);
    for (const QString &name : kDensityNames) {
        m_density->addItem(name);
    }
    densityRow->addWidget(densityLabel);
    densityRow->addWidget(m_density, 0, Qt::AlignLeft);
    densityRow->addStretch(1);
    lay->addLayout(densityRow);
    connect(m_density, &QComboBox::currentIndexChanged, this,
            [this](int index) { emit densityChanged(index); });

    // ---- 毛玻璃效果 ----
    auto *glassRow = new QHBoxLayout;
    glassRow->setSpacing(12);
    auto *glassLabel = new QLabel(QStringLiteral("毛玻璃效果"), card);
    glassLabel->setObjectName(QStringLiteral("settingLabel"));
    m_glass = new QCheckBox(QStringLiteral("启用半透明面板"), card);
    glassRow->addWidget(glassLabel);
    glassRow->addWidget(m_glass, 0, Qt::AlignLeft);
    glassRow->addStretch(1);
    lay->addLayout(glassRow);
    connect(m_glass, &QCheckBox::toggled, this, &SettingsPage::glassToggled);

    // ---- AI 气泡样式 ----
    auto *bubbleRow = new QHBoxLayout;
    bubbleRow->setSpacing(12);
    auto *bubbleLabel = new QLabel(QStringLiteral("AI 气泡样式"), card);
    bubbleLabel->setObjectName(QStringLiteral("settingLabel"));
    m_bubbleStyle = new QComboBox(card);
    m_bubbleStyle->addItem(QStringLiteral("填色气泡"));
    m_bubbleStyle->addItem(QStringLiteral("描边气泡"));
    bubbleRow->addWidget(bubbleLabel);
    bubbleRow->addWidget(m_bubbleStyle, 0, Qt::AlignLeft);
    bubbleRow->addStretch(1);
    lay->addLayout(bubbleRow);
    connect(m_bubbleStyle, &QComboBox::currentIndexChanged, this,
            [this](int index) { emit bubbleStyleChanged(index == 0); });

    // ---- 主题模式提示 ----
    auto *modeHint = new QLabel(
        QStringLiteral("提示：可在左侧导航栏底部一键切换浅色 / 深色主题；"
                       "按 Ctrl+Alt+A 可随时唤起本窗口"), card);
    modeHint->setObjectName(QStringLiteral("settingHint"));
    modeHint->setWordWrap(true);
    lay->addWidget(modeHint);

    // 主题变化时刷新色板边框与勾选态
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const Theme &) { refreshSwatchChecks(); });

    return card;
}

void SettingsPage::refreshSwatchChecks() {
    // 选中态：白色外描边 + 白色对勾
    const QString ring = ThemeManager::instance()->theme().dark
                             ? QStringLiteral("#FFFFFF")
                             : QStringLiteral("#1D2129");
    for (int i = 0; i < m_colorBtns.size(); ++i) {
        QToolButton *btn = m_colorBtns[i];
        btn->setStyleSheet(
            QStringLiteral("QToolButton { background-color: %1; color: #ffffff;"
                           " border-radius: 8px; border: 2px solid transparent;"
                           " font-size: 13px; font-weight: 700; }"
                           "QToolButton:checked { border: 2px solid %2; }")
                .arg(kBrandPresets[i].name(QColor::HexRgb))
                .arg(ring));
        btn->setText(btn->isChecked() ? QStringLiteral("✓") : QString());
    }
}

void SettingsPage::setThemeValues(const Theme &theme) {
    // 同步控件显示（阻断信号避免回环）
    QSignalBlocker b1(m_density);
    QSignalBlocker b2(m_glass);
    QSignalBlocker b3(m_bubbleStyle);
    m_density->setCurrentIndex(qBound(0, theme.density, 2));
    m_glass->setChecked(theme.glass);
    m_bubbleStyle->setCurrentIndex(theme.aiBubbleFilled ? 0 : 1);

    // 主题色：匹配预设，否则不选中
    int matched = -1;
    for (int i = 0; i < m_colorBtns.size(); ++i) {
        if (theme.brand == kBrandPresets[i]) {
            matched = i;
            break;
        }
    }
    for (int i = 0; i < m_colorBtns.size(); ++i) {
        m_colorBtns[i]->setChecked(i == matched);
    }
    refreshSwatchChecks();
}

void SettingsPage::setModelPath(const QString &path) {
    m_modelPath->setText(path.isEmpty() ? QStringLiteral("未选择") : path);
}

void SettingsPage::setMmprojPath(const QString &path) {
    m_mmprojPath->setText(path.isEmpty() ? QStringLiteral("未选择") : path);
}

void SettingsPage::setSamplingValues(double temperature, double topP, int nCtx) {
    // 同步控件显示（阻断信号避免回环写回配置）
    QSignalBlocker b1(m_temperature);
    QSignalBlocker b2(m_topP);
    QSignalBlocker b3(m_nCtx);
    m_temperature->setValue(temperature);
    m_topP->setValue(topP);
    const int idx = m_nCtx->findData(nCtx);
    m_nCtx->setCurrentIndex(idx >= 0 ? idx : m_nCtx->findData(4096));
}

} // namespace studentai
