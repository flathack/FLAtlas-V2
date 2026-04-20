// infrastructure/io/DllResources.cpp - PE-DLL string extraction (Phase 12)

#include "DllResources.h"
#include "PeVersionInfo.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace flatlas::infrastructure {

QMap<int, QString> DllResources::loadStrings(const QString &dllPath, int minId, int maxId)
{
    QMap<int, QString> result;

#ifdef Q_OS_WIN
    HMODULE hMod = LoadLibraryExW(
        reinterpret_cast<LPCWSTR>(dllPath.utf16()),
        nullptr,
        LOAD_LIBRARY_AS_DATAFILE);

    if (!hMod)
        return result;

    // String resources are stored in bundles of 16.
    // Bundle N contains IDs N*16 .. N*16+15.
    const int firstBundle = minId / 16;
    const int lastBundle = (maxId + 15) / 16;

    for (int bundle = firstBundle; bundle <= lastBundle; ++bundle) {
        for (int idx = 0; idx < 16; ++idx) {
            const int id = bundle * 16 + idx;
            if (id < minId || id > maxId)
                continue;

            wchar_t buf[4096];
            const int len = LoadStringW(hMod, static_cast<UINT>(id), buf, 4096);
            if (len > 0)
                result.insert(id, QString::fromWCharArray(buf, len));
        }
    }

    FreeLibrary(hMod);
#else
    Q_UNUSED(dllPath);
    Q_UNUSED(minId);
    Q_UNUSED(maxId);
#endif

    return result;
}

QMap<int, QString> DllResources::loadMultipleDlls(const QStringList &dllPaths)
{
    QMap<int, QString> result;

    // Freelancer stores IDS values in 65536-sized blocks, and the first DLL
    // listed in [Resources] starts at 65536 rather than 0.
    // Example: third DLL + local string id 1 -> 196609.
    for (int i = 0; i < dllPaths.size(); ++i) {
        const int offset = (i + 1) * 65536;
        const auto local = loadStrings(dllPaths[i], 0, 65535);
        for (auto it = local.constBegin(); it != local.constEnd(); ++it)
            result.insert(it.key() + offset, it.value());
    }

    return result;
}

QString DllResources::getExeVersion(const QString &exePath)
{
    PeVersionInfo info(exePath);
    return info.isValid() ? info.fileVersion() : QString();
}

} // namespace flatlas::infrastructure
