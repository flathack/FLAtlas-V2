#pragma once

#include <QString>
#include <QStringList>

namespace flatlas::infrastructure {

class ResourceDllWriter {
public:
    static QString preferredFlatlasDllName();
    static bool isFlatlasResourceDll(const QString &dllName);
    static QStringList resourceDllsFromFreelancerIni(const QString &freelancerIniPath);
    static QString findExistingFlatlasResourceDll(const QString &freelancerIniPath);
    static QString targetFlatlasResourceDll(const QString &freelancerIniPath);
    static int slotForDll(const QString &freelancerIniPath, const QString &dllName);
    static int makeGlobalId(int slot, int localId);

    static bool ensureResourceDllRegistered(const QString &freelancerIniPath,
                                            const QString &dllName,
                                            QString *errorMessage = nullptr);

    static bool ensureFlatlasResourceDll(const QString &freelancerIniPath,
                                         QString *outDllName,
                                         QString *errorMessage = nullptr);

    static bool ensureStringResource(const QString &freelancerIniPath,
                                     const QString &dllName,
                                     int currentGlobalId,
                                     const QString &text,
                                     int *outGlobalId,
                                     QString *errorMessage = nullptr);
};

} // namespace flatlas::infrastructure
