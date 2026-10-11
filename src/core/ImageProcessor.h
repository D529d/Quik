#pragma once
// ============================================================================
// ImageProcessor: 图片处理工具（Step5 多模态）
// ----------------------------------------------------------------------------
// 职责：
//   - 校验用户选择的图片文件（格式 / 可解码）
//   - 生成聊天气泡里的预览缩略图（等比缩放中轴裁剪，避免 UI 过宽）
//   - 供 mtmd 解码的原始图像数据实际不在此处缩放：llama.cpp 的
//     mtmd_helper_bitmap_init_from_file 内部会按视觉模型要求自动缩放，
//     因此本类只负责 UI 预览展示，不重复实现图像缩放逻辑。
// ============================================================================

#include <QPixmap>
#include <QSize>
#include <QString>

namespace studentai {

class ImageProcessor {
public:
    // 是否可显示（存在 / 非空 / Qt 可解码）
    static bool isLoadable(const QString &filePath);

    // 生成等比例缩略图：最长边缩到 maxSize，另一侧保持比例。
    // 返回空 pixmap 表示失败。
    static QPixmap makeThumbnail(const QString &filePath, int maxSize = 220);

    // 支持的图片扩展名（用于文件对话框过滤）
    static QString supportedExtensions();
};

} // namespace studentai