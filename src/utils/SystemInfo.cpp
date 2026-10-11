#include "utils/SystemInfo.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif

namespace studentai {

quint64 currentProcessMemoryKB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<quint64>(pmc.WorkingSetSize) / 1024;
    }
#endif
    return 0;
}

QString formatMemorySize(quint64 bytes) {
    const double kb = static_cast<double>(bytes);
    if (kb >= 1024.0 * 1024.0) {
        return QString::number(kb / (1024.0 * 1024.0), 'f', 2) + QStringLiteral(" GB");
    }
    if (kb >= 1024.0) {
        return QString::number(kb / 1024.0, 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(kb, 'f', 0) + QStringLiteral(" KB");
}

} // namespace studentai
