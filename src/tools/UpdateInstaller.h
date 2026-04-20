#pragma once

#include <QObject>
#include <QString>

namespace flatlas::tools {

/// Ergebnis einer Update-Installation.
struct InstallResult {
    bool success = false;
    QString errorMessage;
};

/// Entpackt ein ZIP und installiert das Update.
/// Da sich die EXE nicht selbst überschreiben kann, wird ein
/// Batch-Skript erzeugt, das nach Beendigung der App die Dateien ersetzt.
class UpdateInstaller : public QObject
{
    Q_OBJECT

public:
    explicit UpdateInstaller(QObject *parent = nullptr);

    /// ZIP entpacken und Install-Skript vorbereiten.
    /// @param zipPath  Pfad zur heruntergeladenen ZIP-Datei
    /// @param appDir   Installationsverzeichnis der App
    /// @return Ergebnis der Vorbereitung
    InstallResult prepare(const QString &zipPath, const QString &appDir);

    /// Install-Skript ausführen und App beenden.
    /// Ruft das vorbereitete Batch-Skript auf und beendet die App.
    bool executeAndRestart();

    /// Pfad zum vorbereiteten Update-Verzeichnis.
    QString stagingDir() const;

    /// Pfad zum Install-Skript.
    QString scriptPath() const;

    /// ZIP-Datei entpacken (mittels externem Tool oder Minizip).
    static InstallResult extractZip(const QString &zipPath, const QString &destDir);

private:
    QString m_stagingDir;
    QString m_scriptPath;
    QString m_appDir;
};

} // namespace flatlas::tools
