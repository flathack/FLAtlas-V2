#pragma once
// infrastructure/freelancer/FreelancerPaths.h – Freelancer-Installation finden
// TODO Phase 2
#include <QString>
#include <QStringList>
namespace flatlas::infrastructure {
class FreelancerPaths {
public:
    /// Freelancer-Installationsverzeichnis finden (Registry + bekannte Pfade).
    static QString findInstallation();
    /// Freelancer.exe finden.
    static QString findExe(const QString &installDir);
    /// FLServer.exe finden.
    static QString findServerExe(const QString &installDir);
    /// DATA-Verzeichnis finden.
    static QString findDataDir(const QString &installDir);
};
} // namespace flatlas::infrastructure
