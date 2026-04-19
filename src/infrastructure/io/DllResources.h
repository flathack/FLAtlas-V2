#pragma once
// infrastructure/io/DllResources.h – PE-DLL-String-Extraktion (Phase 12)

#include <QString>
#include <QMap>

namespace flatlas::infrastructure {

class DllResources {
public:
    /// Load all string resources from a DLL file (Windows LoadLibraryEx API).
    /// Returns map of IDS number → string. Range can be limited with minId/maxId.
    static QMap<int, QString> loadStrings(const QString &dllPath,
                                           int minId = 0, int maxId = 0x10000);

    /// Load strings from multiple DLL files (resources.dll, infocards.dll, etc.)
    /// Standard Freelancer layout: IDs 0-65535 in first DLL, 65536-131071 in second, etc.
    static QMap<int, QString> loadMultipleDlls(const QStringList &dllPaths);

    /// EXE-Versionsinformation lesen.
    static QString getExeVersion(const QString &exePath);
};

} // namespace flatlas::infrastructure
