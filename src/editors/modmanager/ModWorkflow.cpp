// editors/modmanager/ModWorkflow.cpp – Mod aktivieren/deaktivieren (Phase 13)

#include "ModWorkflow.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace flatlas::editors {

bool ModWorkflow::backupFile(const QString &gameRelPath)
{
    QDir gameDir(m_gamePath);
    QDir backupDir(m_backupPath);

    QString srcPath = gameDir.filePath(gameRelPath);
    QString dstPath = backupDir.filePath(gameRelPath);

    if (!QFile::exists(srcPath))
        return true; // nothing to backup

    // Already backed up? Skip.
    if (QFile::exists(dstPath))
        return true;

    // Ensure target directory exists
    QFileInfo dstInfo(dstPath);
    backupDir.mkpath(dstInfo.path());

    if (!QFile::copy(srcPath, dstPath)) {
        m_lastError = QStringLiteral("Failed to backup: %1").arg(srcPath);
        return false;
    }
    return true;
}

bool ModWorkflow::restoreFile(const QString &gameRelPath)
{
    QDir gameDir(m_gamePath);
    QDir backupDir(m_backupPath);

    QString backupPath = backupDir.filePath(gameRelPath);
    QString gamePath = gameDir.filePath(gameRelPath);

    if (!QFile::exists(backupPath)) {
        // No backup → remove the mod file from game dir
        QFile::remove(gamePath);
        return true;
    }

    // Remove current mod file and restore backup
    QFile::remove(gamePath);

    if (!QFile::copy(backupPath, gamePath)) {
        m_lastError = QStringLiteral("Failed to restore: %1").arg(gameRelPath);
        return false;
    }

    // Remove backup after successful restore
    QFile::remove(backupPath);
    return true;
}

QStringList ModWorkflow::activateMod(const ModInfo &mod)
{
    QStringList copied;
    m_lastError.clear();

    QDir gameDir(m_gamePath);
    QDir modDir(mod.path);

    for (const auto &relPath : mod.files) {
        // Backup the original first
        if (!backupFile(relPath))
            return copied;

        QString src = modDir.filePath(relPath);
        QString dst = gameDir.filePath(relPath);

        // Ensure target directory exists
        QFileInfo dstInfo(dst);
        gameDir.mkpath(dstInfo.path());

        // Remove existing and copy mod file
        QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            m_lastError = QStringLiteral("Failed to copy: %1 → %2").arg(src, dst);
            return copied;
        }

        copied.append(relPath);
    }

    return copied;
}

QStringList ModWorkflow::deactivateMod(const ModInfo &mod)
{
    QStringList restored;
    m_lastError.clear();

    for (const auto &relPath : mod.files) {
        if (!restoreFile(relPath))
            return restored;
        restored.append(relPath);
    }

    return restored;
}

bool ModWorkflow::saveProfile(const flatlas::domain::ModProfile &profile,
                               const QString &filePath) const
{
    QJsonDocument doc(profile.toJson());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

flatlas::domain::ModProfile ModWorkflow::loadProfile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return flatlas::domain::ModProfile::fromJson(doc.object());
}

} // namespace flatlas::editors
