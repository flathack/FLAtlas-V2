// tools/SpStarter.cpp – Launch Freelancer in Singleplayer mode

#include "SpStarter.h"

#include <QFileInfo>
#include <QProcess>

namespace flatlas::tools {

bool SpStarter::launch(const QString &exePath, const QStringList &args)
{
    if (!QFileInfo::exists(exePath))
        return false;

    // Start detached so Freelancer keeps running after FLAtlas closes.
    return QProcess::startDetached(exePath, args,
                                   QFileInfo(exePath).absolutePath());
}

bool SpStarter::launchWindowed(const QString &exePath, int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    QStringList args;
    args << QStringLiteral("-w")
         << QStringLiteral("-x") << QString::number(width)
         << QStringLiteral("-y") << QString::number(height);
    return launch(exePath, args);
}

bool SpStarter::isFreelancerExe(const QString &exePath)
{
    QFileInfo fi(exePath);
    if (!fi.exists() || !fi.isFile())
        return false;

    // Basic check: filename must be Freelancer.exe (case-insensitive)
    const QString name = fi.fileName().toLower();
    return name == QStringLiteral("freelancer.exe");
}

} // namespace flatlas::tools
