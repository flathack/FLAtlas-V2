#include "UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

namespace flatlas::tools {

UpdateInstaller::UpdateInstaller(QObject *parent)
    : QObject(parent)
{
}

InstallResult UpdateInstaller::prepare(const QString &zipPath, const QString &appDir)
{
    m_appDir = appDir;

    // Staging-Verzeichnis im Temp
    m_stagingDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                   + QStringLiteral("/flatlas_update");

    // Altes Staging aufräumen
    QDir staging(m_stagingDir);
    if (staging.exists())
        staging.removeRecursively();
    staging.mkpath(QStringLiteral("."));

    // ZIP entpacken
    auto result = extractZip(zipPath, m_stagingDir);
    if (!result.success)
        return result;

    // Batch-Skript erzeugen, das Dateien kopiert und App neu startet
    m_scriptPath = m_stagingDir + QStringLiteral("/update.cmd");
    QFile script(m_scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, tr("Cannot create update script")};
    }

    const QString appExe = QCoreApplication::applicationFilePath();

    QTextStream out(&script);
    out << "@echo off\r\n";
    out << "echo Waiting for FLAtlas to close...\r\n";
    out << "timeout /t 2 /nobreak >nul\r\n";
    out << "echo Installing update...\r\n";
    out << "xcopy /E /Y /Q \"" << QDir::toNativeSeparators(m_stagingDir) << "\\*\" \""
        << QDir::toNativeSeparators(appDir) << "\\\"\r\n";
    out << "echo Starting FLAtlas...\r\n";
    out << "start \"\" \"" << QDir::toNativeSeparators(appExe) << "\"\r\n";
    out << "del \"%~f0\"\r\n"; // Batch löscht sich selbst
    script.close();

    return {true, {}};
}

bool UpdateInstaller::executeAndRestart()
{
    if (m_scriptPath.isEmpty() || !QFile::exists(m_scriptPath))
        return false;

    // Batch-Skript detached starten
    bool ok = QProcess::startDetached(
        QStringLiteral("cmd.exe"),
        {QStringLiteral("/C"), QDir::toNativeSeparators(m_scriptPath)});

    if (ok)
        QCoreApplication::quit();

    return ok;
}

QString UpdateInstaller::stagingDir() const
{
    return m_stagingDir;
}

QString UpdateInstaller::scriptPath() const
{
    return m_scriptPath;
}

InstallResult UpdateInstaller::extractZip(const QString &zipPath, const QString &destDir)
{
    if (!QFile::exists(zipPath))
        return {false, QObject::tr("ZIP file not found: %1").arg(zipPath)};

    QDir().mkpath(destDir);

    // Verwende PowerShell Expand-Archive
    QProcess proc;
    proc.setProgram(QStringLiteral("powershell"));
    proc.setArguments({
        QStringLiteral("-NoProfile"),
        QStringLiteral("-Command"),
        QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force")
            .arg(zipPath, destDir)
    });
    proc.start();
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
        const QString err = QString::fromUtf8(proc.readAllStandardError());
        return {false, QObject::tr("Failed to extract ZIP: %1").arg(err)};
    }

    return {true, {}};
}

} // namespace flatlas::tools
