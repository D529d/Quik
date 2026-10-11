#pragma once
// ============================================================================
// SystemInfo：系统信息查询工具
// ============================================================================

#include <QString>
#include <QtGlobal>

namespace studentai {

// 当前进程占用物理内存（KB）
// Windows 下通过 GetProcessMemoryInfo 查询工作集大小
quint64 currentProcessMemoryKB();

// 将字节数格式化为可读字符串（KB / MB / GB）
QString formatMemorySize(quint64 bytes);

} // namespace studentai
