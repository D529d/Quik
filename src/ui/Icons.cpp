#include "ui/Icons.h"

#include <QPainter>
#include <QSvgRenderer>

namespace studentai {
namespace Icons {

namespace {

// 用 svgBody + 颜色渲染成 QPixmap
QPixmap render(const QString &svgDoc, int size) {
    QSvgRenderer renderer(svgDoc.toUtf8());
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p);
    p.end();
    return pm;
}

// 描边式图标包装
QString strokeDoc(const QString &body, const QColor &color, int size) {
    return QStringLiteral(
               "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" "
               "viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"%2\" stroke-width=\"1.6\" "
               "stroke-linecap=\"round\" stroke-linejoin=\"round\">%3</svg>")
        .arg(size)
        .arg(color.name(QColor::HexRgb))
        .arg(body);
}

} // namespace

QIcon fromSvg(const QString &svgBody, const QColor &color, int size) {
    return QIcon(render(strokeDoc(svgBody, color, size), size));
}

QIcon logo(const QColor &brand, int size) {
    const QString doc = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" viewBox=\"0 0 24 24\">"
        "<rect x=\"2.5\" y=\"2.5\" width=\"19\" height=\"19\" rx=\"5.5\" fill=\"%2\" stroke=\"none\"/>"
        "<path d=\"M13.6 6.5L8 13.5h3.4l-1.2 4 5.6-7h-3.4z\" fill=\"#ffffff\" stroke=\"none\"/></svg>")
        .arg(size)
        .arg(brand.name(QColor::HexRgb));
    return QIcon(render(doc, size));
}

QIcon chat(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M4 5.5A1.5 1.5 0 0 1 5.5 4h13A1.5 1.5 0 0 1 20 5.5v9a1.5 1.5 0 0 1-1.5 1.5H9l-4 3.5v-3.5H5.5A1.5 1.5 0 0 1 4 14.5z\"/>"),
                   c, size);
}

QIcon camera(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<rect x=\"3.5\" y=\"6.5\" width=\"17\" height=\"12.5\" rx=\"2.5\"/>"
                       "<path d=\"M8.5 6.5L10 4h4l1.5 2.5\"/><circle cx=\"12\" cy=\"12.5\" r=\"3.2\"/>"),
                   c, size);
}

QIcon note(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M6 3h8l4 4v14H6z\"/><path d=\"M14 3v4h4\"/>"
                       "<path d=\"M9 12h6M9 16h6\"/>"),
                   c, size);
}

QIcon globe(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<circle cx=\"12\" cy=\"12\" r=\"8.5\"/><path d=\"M3.5 12h17\"/>"
                       "<path d=\"M12 3.5a14 14 0 0 1 0 17M12 3.5a14 14 0 0 0 0 17\"/>"),
                   c, size);
}

QIcon bulb(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M12 3.5a6 6 0 0 1 3.4 10.9c-.8.6-1.4 1.2-1.4 2.1h-4c0-.9-.6-1.5-1.4-2.1A6 6 0 0 1 12 3.5z\"/>"
                       "<path d=\"M9.5 20h5M10.5 17h3\"/>"),
                   c, size);
}

QIcon sliders(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M4 7h16M4 12h16M4 17h16\"/>"
                       "<circle cx=\"9\" cy=\"7\" r=\"2\"/><circle cx=\"15\" cy=\"12\" r=\"2\"/>"
                       "<circle cx=\"10\" cy=\"17\" r=\"2\"/>"),
                   c, size);
}

QIcon plus(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M12 5.5v13M5.5 12h13\"/>"), c, size);
}

QIcon send(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M4.5 11.5L19.5 4.5l-4 15-3.5-6.5z\"/>"), c, size);
}

QIcon stop(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<rect x=\"8.2\" y=\"8.2\" width=\"7.6\" height=\"7.6\" rx=\"1.6\"/>"),
                   c, size);
}

QIcon image(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<rect x=\"3.5\" y=\"5\" width=\"17\" height=\"14\" rx=\"2.5\"/>"
                       "<circle cx=\"9\" cy=\"10\" r=\"1.6\"/>"
                       "<path d=\"M4 16.5l4.5-4.5 3 3 3.5-3.5 5 5\"/>"),
                   c, size);
}

QIcon trash(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M5 7h14M9.5 7V5h5v2M7 7l1 12.5h8L17 7M10.2 11v5M13.8 11v5\"/>"),
                   c, size);
}

QIcon moon(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<path d=\"M20 14.5A7.5 7.5 0 0 1 9.5 4a7.5 7.5 0 1 0 10.5 10.5z\"/>"),
                   c, size);
}

QIcon sun(const QColor &c, int size) {
    return fromSvg(QStringLiteral(
                       "<circle cx=\"12\" cy=\"12\" r=\"4\"/>"
                       "<path d=\"M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4"
                       "M18.4 5.6L17 7M7 17l-1.4 1.4\"/>"),
                   c, size);
}

QIcon chevronLeft(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M14.5 6L8.5 12l6 6\"/>"), c, size);
}

QIcon chevronRight(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M9.5 6l6 6-6 6\"/>"), c, size);
}

QIcon chevronDown(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M6 9.5l6 6 6-6\"/>"), c, size);
}

QIcon check(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M5 12.5l4.5 4.5L19 7.5\"/>"), c, size);
}

QIcon close(const QColor &c, int size) {
    return fromSvg(QStringLiteral("<path d=\"M7 7l10 10M17 7L7 17\"/>"), c, size);
}

} // namespace Icons
} // namespace studentai
