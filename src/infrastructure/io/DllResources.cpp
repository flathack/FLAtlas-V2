// infrastructure/io/DllResources.cpp – PE-DLL-String-Extraktion (Phase 12)

#include "DllResources.h"

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
    int firstBundle = minId / 16;
    int lastBundle = (maxId + 15) / 16;

    for (int bundle = firstBundle; bundle <= lastBundle; ++bundle) {
        for (int idx = 0; idx < 16; ++idx) {
            int id = bundle * 16 + idx;
            if (id < minId || id > maxId)
                continue;

            wchar_t buf[4096];
            int len = LoadStringW(hMod, static_cast<UINT>(id), buf, 4096);
            if (len > 0) {
                result.insert(id, QString::fromWCharArray(buf, len));
            }
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

    // Standard Freelancer layout:
    // DLL index 0 → IDs 0..65535
    // DLL index 1 → IDs 65536..131071
    // etc. (offsets stored as idsNumber = dllIndex * 65536 + localId)
    for (int i = 0; i < dllPaths.size(); ++i) {
        int offset = i * 65536;
        auto local = loadStrings(dllPaths[i], 0, 65535);
        for (auto it = local.constBegin(); it != local.constEnd(); ++it) {
            result.insert(it.key() + offset, it.value());
        }
    }

    return result;
}

QString DllResources::getExeVersion(const QString &)
{
    return {}; // TODO Phase 20
}

} // namespace flatlas::infrastructure
