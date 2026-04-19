#pragma once
// editors/modmanager/ConflictDetector.h – Dateikonflikte zwischen Mods (Phase 13)

#include <QString>
#include <QVector>
#include <QHash>
#include <QStringList>

namespace flatlas::editors {

struct ModInfo {
    QString name;
    QString path;       // root directory of mod
    QStringList files;  // relative file paths within mod
};

struct FileConflict {
    QString relativePath;    // conflicted file path
    QStringList modNames;    // mods that touch this file
};

class ConflictDetector {
public:
    /// Scan a directory for mods (each subfolder = one mod).
    QVector<ModInfo> scanMods(const QString &modsDir) const;

    /// Scan a single mod directory and list its files.
    ModInfo scanSingleMod(const QString &modDir) const;

    /// Detect file conflicts between the given mods.
    QVector<FileConflict> detectConflicts(const QVector<ModInfo> &mods) const;

    /// Detect conflicts only among selected mods (by name).
    QVector<FileConflict> detectConflicts(const QVector<ModInfo> &mods,
                                           const QStringList &activeModNames) const;
};

} // namespace flatlas::editors
