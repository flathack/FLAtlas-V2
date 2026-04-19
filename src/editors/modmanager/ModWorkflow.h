#pragma once
// editors/modmanager/ModWorkflow.h – Mod aktivieren/deaktivieren (Phase 13)

#include <QString>
#include <QStringList>
#include "domain/ModProfile.h"
#include "ConflictDetector.h"

namespace flatlas::editors {

class ModWorkflow {
public:
    /// Set the game installation directory.
    void setGamePath(const QString &gamePath) { m_gamePath = gamePath; }

    /// Set the backup directory for originals.
    void setBackupPath(const QString &backupPath) { m_backupPath = backupPath; }

    /// Activate a mod: backup originals, copy mod files into game dir.
    /// Returns list of files that were copied.
    QStringList activateMod(const ModInfo &mod);

    /// Deactivate a mod: restore backed-up originals.
    /// Returns list of files that were restored.
    QStringList deactivateMod(const ModInfo &mod);

    /// Save profile to JSON file.
    bool saveProfile(const flatlas::domain::ModProfile &profile, const QString &filePath) const;

    /// Load profile from JSON file.
    flatlas::domain::ModProfile loadProfile(const QString &filePath) const;

    /// Get last error message.
    QString lastError() const { return m_lastError; }

private:
    bool backupFile(const QString &gameRelPath);
    bool restoreFile(const QString &gameRelPath);

    QString m_gamePath;
    QString m_backupPath;
    QString m_lastError;
};

} // namespace flatlas::editors
