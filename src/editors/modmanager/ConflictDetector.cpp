// editors/modmanager/ConflictDetector.cpp – Dateikonflikte zwischen Mods (Phase 13)

#include "ConflictDetector.h"

#include <QDir>
#include <QDirIterator>

namespace flatlas::editors {

QVector<ModInfo> ConflictDetector::scanMods(const QString &modsDir) const
{
    QVector<ModInfo> mods;
    QDir dir(modsDir);
    if (!dir.exists()) return mods;

    for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        mods.append(scanSingleMod(entry.absoluteFilePath()));
    }
    return mods;
}

ModInfo ConflictDetector::scanSingleMod(const QString &modDir) const
{
    ModInfo info;
    QDir dir(modDir);
    info.name = dir.dirName();
    info.path = modDir;

    QDirIterator it(modDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        info.files.append(dir.relativeFilePath(it.filePath()));
    }

    return info;
}

QVector<FileConflict> ConflictDetector::detectConflicts(const QVector<ModInfo> &mods) const
{
    // Map: relative file path → list of mod names that provide it
    QHash<QString, QStringList> fileToMods;

    for (const auto &mod : mods) {
        for (const auto &file : mod.files) {
            QString key = file.toLower();
            fileToMods[key].append(mod.name);
        }
    }

    // Collect conflicts (files touched by 2+ mods)
    QVector<FileConflict> conflicts;
    for (auto it = fileToMods.constBegin(); it != fileToMods.constEnd(); ++it) {
        if (it.value().size() > 1) {
            FileConflict c;
            c.relativePath = it.key();
            c.modNames = it.value();
            conflicts.append(c);
        }
    }

    return conflicts;
}

QVector<FileConflict> ConflictDetector::detectConflicts(const QVector<ModInfo> &mods,
                                                         const QStringList &activeModNames) const
{
    QVector<ModInfo> filtered;
    for (const auto &mod : mods) {
        if (activeModNames.contains(mod.name, Qt::CaseInsensitive))
            filtered.append(mod);
    }
    return detectConflicts(filtered);
}

} // namespace flatlas::editors
