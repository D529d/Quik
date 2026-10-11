#pragma once
// ============================================================================
// Icons：线性风格图标（内联 SVG，描边式，可着色）
// ----------------------------------------------------------------------------
// Qt QSS 无法给图标重新着色，因此用"内联 SVG + 目标颜色"动态生成 QIcon，
// 主题切换时由各控件重新调用生成即可。
// ============================================================================

#include <QColor>
#include <QIcon>
#include <QString>

namespace studentai {
namespace Icons {

// 通用：按 svgBody（24x24 viewBox 内的描边路径）与颜色生成图标
QIcon fromSvg(const QString &svgBody, const QColor &color, int size = 20);

// 应用 Logo：品牌色圆角方块 + 白色闪电
QIcon logo(const QColor &brand, int size = 36);

// 导航图标
QIcon chat(const QColor &color, int size = 20);      // 对话问答
QIcon camera(const QColor &color, int size = 20);    // 拍照识题
QIcon note(const QColor &color, int size = 20);      // 笔记总结
QIcon globe(const QColor &color, int size = 20);     // 离线翻译
QIcon bulb(const QColor &color, int size = 20);      // 错题解析
QIcon sliders(const QColor &color, int size = 20);   // 设置

// 功能图标
QIcon plus(const QColor &color, int size = 20);      // 新建
QIcon send(const QColor &color, int size = 20);      // 发送
QIcon stop(const QColor &color, int size = 20);      // 停止
QIcon image(const QColor &color, int size = 20);     // 拍照/图片
QIcon trash(const QColor &color, int size = 20);     // 删除
QIcon moon(const QColor &color, int size = 20);      // 深色主题
QIcon sun(const QColor &color, int size = 20);       // 浅色主题
QIcon chevronLeft(const QColor &color, int size = 20);  // 折叠/返回
QIcon chevronRight(const QColor &color, int size = 20); // 展开/收起
QIcon chevronDown(const QColor &color, int size = 20);  // 已展开（可折叠）
QIcon check(const QColor &color, int size = 20);        // 完成对勾
QIcon close(const QColor &color, int size = 20);        // 移除附件

} // namespace Icons
} // namespace studentai
