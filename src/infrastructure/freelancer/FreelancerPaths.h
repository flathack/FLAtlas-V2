#pragma once
// infrastructure/freelancer/FreelancerPaths.h – Freelancer-Installation finden
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
    /// Alle bekannten Installationspfade.
    static QStringList knownInstallPaths();
    /// freelancer.ini im DATA-Verzeichnis finden (case-insensitive).
    static QString findFreelancerIni(const QString &dataDir);
};
} // namespace flatlas::infrastructure
