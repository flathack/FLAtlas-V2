#include "UpdateInstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>

namespace flatlas::tools {

namespace {
QString powershellQuote(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

QString payloadRootForStaging(const QString &stagingDir)
{
    const QDir staging(stagingDir);
    const QFileInfoList files = staging.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    const QFileInfoList dirs = staging.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (files.isEmpty() && dirs.size() == 1)
        return dirs.first().absoluteFilePath();
    return stagingDir;
}
} // namespace

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

    const QString payloadDir = payloadRootForStaging(m_stagingDir);

    // Batch-Skript erzeugen, das Dateien kopiert und App neu startet
    m_scriptPath = m_stagingDir + QStringLiteral("/update.cmd");
    QFile script(m_scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {false, tr("Cannot create update script")};
    }

    const QString appExe = QCoreApplication::applicationFilePath();
    const QString appExeName = QFileInfo(appExe).fileName();
    const QString logPath = m_stagingDir + QStringLiteral("/update-install.log");

    QTextStream out(&script);
    out << "@echo off\r\n";
    out << "setlocal\r\n";
    out << "set \"APP_EXE=" << QDir::toNativeSeparators(appExe) << "\"\r\n";
    out << "set \"APP_EXE_NAME=" << appExeName << "\"\r\n";
    out << "set \"PAYLOAD_DIR=" << QDir::toNativeSeparators(payloadDir) << "\"\r\n";
    out << "set \"APP_DIR=" << QDir::toNativeSeparators(appDir) << "\"\r\n";
    out << "set \"LOG_FILE=" << QDir::toNativeSeparators(logPath) << "\"\r\n";
    out << "echo [%date% %time%] Waiting for FLAtlas to close...>>\"%LOG_FILE%\"\r\n";
    out << "for /L %%i in (1,1,60) do (\r\n";
    out << "  tasklist /FI \"IMAGENAME eq %APP_EXE_NAME%\" 2>nul | find /I \"%APP_EXE_NAME%\" >nul\r\n";
    out << "  if errorlevel 1 goto :install\r\n";
    out << "  timeout /t 1 /nobreak >nul\r\n";
    out << ")\r\n";
    out << "echo [%date% %time%] FLAtlas is still running. Update aborted.>>\"%LOG_FILE%\"\r\n";
    out << "echo FLAtlas is still running. Update aborted.\r\n";
    out << "pause\r\n";
    out << "exit /b 1\r\n";
    out << ":install\r\n";
    out << "echo [%date% %time%] Installing update...>>\"%LOG_FILE%\"\r\n";
    out << "xcopy /E /Y /Q \"%PAYLOAD_DIR%\\*\" \"%APP_DIR%\\\"\r\n";
    out << "if errorlevel 1 (\r\n";
    out << "  echo [%date% %time%] Copy failed with error %errorlevel%.>>\"%LOG_FILE%\"\r\n";
    out << "  echo Update copy failed. See \"%LOG_FILE%\".\r\n";
    out << "  pause\r\n";
    out << "  exit /b 1\r\n";
    out << ")\r\n";
    out << "echo [%date% %time%] Starting FLAtlas...>>\"%LOG_FILE%\"\r\n";
    out << "start \"\" \"%APP_EXE%\"\r\n";
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

bool UpdateInstaller::executeExternalInstallerAndQuit(const QString &installerPath)
{
    if (!QFile::exists(installerPath))
        return false;

    const bool ok = QProcess::startDetached(QDir::toNativeSeparators(installerPath), {});
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
        QStringLiteral("Expand-Archive -LiteralPath %1 -DestinationPath %2 -Force")
            .arg(powershellQuote(zipPath), powershellQuote(destDir))
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
