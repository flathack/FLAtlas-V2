#include "FreelancerPaths.h"
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <QSettings>
#endif

namespace flatlas::infrastructure {

QStringList FreelancerPaths::knownInstallPaths()
{
    return {
        QStringLiteral("C:/Freelancer"),
        QStringLiteral("C:/Games/Freelancer"),
        QStringLiteral("C:/Program Files (x86)/Microsoft Games/Freelancer"),
        QStringLiteral("D:/Freelancer"),
        QStringLiteral("D:/Games/Freelancer"),
    };
}

QString FreelancerPaths::findInstallation()
{
#ifdef Q_OS_WIN
    // Try native registry (32-bit view)
    {
        QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft Games\\Freelancer\\1.0"),
                       QSettings::NativeFormat);
        QString path = reg.value(QStringLiteral("AppPath")).toString();
        if (!path.isEmpty() && QDir(path).exists())
            return QDir::cleanPath(path);
    }
    // Try WOW6432Node (64-bit OS, 32-bit app)
    {
        QSettings reg(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft Games\\Freelancer\\1.0"),
                       QSettings::NativeFormat);
        QString path = reg.value(QStringLiteral("AppPath")).toString();
        if (!path.isEmpty() && QDir(path).exists())
            return QDir::cleanPath(path);
    }
#endif
    // Fallback: check known paths
    for (const QString &path : knownInstallPaths()) {
        if (QDir(path).exists())
            return QDir::cleanPath(path);
    }
    return {};
}

QString FreelancerPaths::findExe(const QString &installDir)
{
    QDir exeDir(installDir + QStringLiteral("/EXE"));
    if (!exeDir.exists()) {
        // Case-insensitive fallback
        QDir base(installDir);
        for (const QString &entry : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.compare(QStringLiteral("EXE"), Qt::CaseInsensitive) == 0) {
                exeDir.setPath(base.filePath(entry));
                break;
            }
        }
    }
    if (!exeDir.exists())
        return {};
    for (const QString &file : exeDir.entryList(QDir::Files)) {
        if (file.compare(QStringLiteral("Freelancer.exe"), Qt::CaseInsensitive) == 0)
            return exeDir.filePath(file);
    }
    return {};
}

QString FreelancerPaths::findServerExe(const QString &installDir)
{
    QDir exeDir(installDir + QStringLiteral("/EXE"));
    if (!exeDir.exists()) {
        QDir base(installDir);
        for (const QString &entry : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.compare(QStringLiteral("EXE"), Qt::CaseInsensitive) == 0) {
                exeDir.setPath(base.filePath(entry));
                break;
            }
        }
    }
    if (!exeDir.exists())
        return {};
    for (const QString &file : exeDir.entryList(QDir::Files)) {
        if (file.compare(QStringLiteral("FLServer.exe"), Qt::CaseInsensitive) == 0)
            return exeDir.filePath(file);
    }
    return {};
}

QString FreelancerPaths::findDataDir(const QString &installDir)
{
    QDir base(installDir);
    for (const QString &entry : base.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0)
            return base.filePath(entry);
    }
    return {};
}

QString FreelancerPaths::findFreelancerIni(const QString &dataDir)
{
    QDir dir(dataDir);
    if (!dir.exists())
        return {};
    for (const QString &file : dir.entryList(QDir::Files)) {
        if (file.compare(QStringLiteral("freelancer.ini"), Qt::CaseInsensitive) == 0)
            return dir.filePath(file);
    }
    return {};
}

} // namespace flatlas::infrastructure
