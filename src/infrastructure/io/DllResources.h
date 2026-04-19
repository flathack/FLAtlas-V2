#pragma once
// infrastructure/io/DllResources.h – PE-DLL-String-Extraktion
// TODO Phase 12/20
#include <QString>
#include <QMap>
namespace flatlas::infrastructure {
class DllResources {
public:
    /// String-Resources aus einer DLL laden (Windows API).
    static QMap<int, QString> loadStrings(const QString &dllPath);
    /// EXE-Versionsinformation lesen.
    static QString getExeVersion(const QString &exePath);
};
} // namespace flatlas::infrastructure
