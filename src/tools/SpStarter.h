#pragma once
// tools/SpStarter.h – Launch Freelancer in Singleplayer mode

#include <QString>
#include <QStringList>

namespace flatlas::tools {

/// Launches Freelancer.exe as a detached process.
class SpStarter {
public:
    /// Launch Freelancer with optional command-line arguments.
    /// Returns true if the process started successfully.
    static bool launch(const QString &exePath, const QStringList &args = {});

    /// Launch Freelancer in windowed mode with given resolution.
    static bool launchWindowed(const QString &exePath, int width, int height);

    /// Check whether the given path looks like a valid Freelancer.exe.
    static bool isFreelancerExe(const QString &exePath);
};

} // namespace flatlas::tools
