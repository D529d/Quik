#include "core/ImageProcessor.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>

namespace studentai {

bool ImageProcessor::isLoadable(const QString &filePath) {
    if (filePath.isEmpty()) {
        return false;
    }
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }
    // canRead() 只做格式探测，不整图解码，避免在 UI 线程上做大图解码
    return QImageReader(filePath).canRead();
}

QPixmap ImageProcessor::makeThumbnail(const QString &filePath, int maxSize) {
    if (!isLoadable(filePath)) {
        return QPixmap();
    }
    QImageReader reader(filePath);
    reader.setAutoTransform(true); // 尊重 EXIF 旋转
    const QSize orig = reader.size();
    if (orig.isEmpty()) {
        return QPixmap();
    }
    // 等比缩放，保持在 maxSize x maxSize 内
    const QSize scaled = orig.scaled(QSize(maxSize, maxSize), Qt::KeepAspectRatio);
    reader.setScaledSize(scaled);
    const QImage img = reader.read();
    if (img.isNull()) {
        return QPixmap();
    }
    return QPixmap::fromImage(img);
}

QString ImageProcessor::supportedExtensions() {
    return QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*.*)");
}

} // namespace studentai