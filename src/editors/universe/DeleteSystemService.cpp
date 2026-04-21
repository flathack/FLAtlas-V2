#include "DeleteSystemService.h"

#include "core/PathUtils.h"
#include "domain/UniverseData.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace flatlas::editors {

using namespace flatlas::domain;
using namespace flatlas::infrastructure;

namespace {

QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower();
}

QString normalizedRelativeKey(const QString &value)
{
    return QDir::cleanPath(value.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'))).toLower();
}

QString universeDirForFile(const QString &universeFilePath)
{
    return QFileInfo(universeFilePath).absolutePath();
}

QString dataDirForUniverseFile(const QString &universeFilePath)
{
    return QFileInfo(universeDirForFile(universeFilePath)).absolutePath();
}

QString resolveRelativeAgainst(const QString &baseDir, const QString &relativePath)
{
    const QString resolved = flatlas::core::PathUtils::ciResolvePath(baseDir, relativePath);
    if (!resolved.isEmpty())
        return resolved;
    return QDir(baseDir).absoluteFilePath(relativePath);
}

QString resolveSystemFilePath(const QString &universeFilePath, const QString &relativePath)
{
    return resolveRelativeAgainst(universeDirForFile(universeFilePath), relativePath);
}

QString resolveDataFilePath(const QString &universeFilePath, const QString &relativePath)
{
    return resolveRelativeAgainst(dataDirForUniverseFile(universeFilePath), relativePath);
}

QString gotoTargetSystem(const QString &gotoValue)
{
    return gotoValue.split(QLatin1Char(',')).value(0).trimmed();
}

bool startsWithPath(const QString &childPath, const QString &parentPath)
{
    const QString child = normalizedPath(childPath);
    QString parent = normalizedPath(parentPath);
    if (!parent.endsWith(QLatin1Char('/')))
        parent.append(QLatin1Char('/'));
    return child.startsWith(parent);
}

QVector<QString> filesUnderDirectory(const QString &dirPath)
{
    QVector<QString> result;
    if (!QFileInfo(dirPath).isDir())
        return result;

    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        result.append(QDir::cleanPath(it.next()));
    return result;
}

void appendUniqueFileDecision(QVector<DeleteSystemFileDecision> &list, const QString &path, const QString &reason)
{
    const QString normalized = normalizedPath(path);
    for (auto &entry : list) {
        if (normalizedPath(entry.path) == normalized) {
            if (!reason.isEmpty() && !entry.reason.contains(reason, Qt::CaseInsensitive))
                entry.reason += QStringLiteral(" | %1").arg(reason);
            return;
        }
    }
    list.append({QDir::cleanPath(path), reason});
}

void appendUniqueSectionChange(QVector<DeleteSystemSectionChange> &list,
                               const QString &filePath,
                               const QString &sectionName,
                               const QString &identifier,
                               const QString &reason)
{
    for (const auto &entry : list) {
        if (normalizedPath(entry.filePath) == normalizedPath(filePath)
            && entry.sectionName.compare(sectionName, Qt::CaseInsensitive) == 0
            && entry.identifier.compare(identifier, Qt::CaseInsensitive) == 0)
            return;
    }
    list.append({QDir::cleanPath(filePath), sectionName, identifier, reason});
}

void appendUniqueWarning(QStringList &list, const QString &text)
{
    for (const QString &existing : list) {
        if (existing.compare(text, Qt::CaseInsensitive) == 0)
            return;
    }
    list.append(text);
}

void appendUniqueBlocker(QStringList &list, const QString &text)
{
    for (const QString &existing : list) {
        if (existing.compare(text, Qt::CaseInsensitive) == 0)
            return;
    }
    list.append(text);
}

QVector<QString> allSystemFilesFromUniverse(const QString &universeFilePath, const IniDocument &universeDoc)
{
    QVector<QString> result;
    for (const auto &section : universeDoc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) != 0)
            continue;
        const QString relativePath = section.value(QStringLiteral("file")).trimmed();
        if (relativePath.isEmpty())
            continue;
        result.append(resolveSystemFilePath(universeFilePath, relativePath));
    }
    return result;
}

QHash<QString, int> collectFieldReferenceCounts(const QVector<QString> &systemFiles,
                                                const QString &skipSystemFile)
{
    QHash<QString, int> counts;
    for (const QString &systemFile : systemFiles) {
        if (normalizedPath(systemFile) == normalizedPath(skipSystemFile))
            continue;
        const IniDocument doc = IniParser::parseFile(systemFile);
        for (const auto &section : doc) {
            const bool asteroid = section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0;
            const bool nebula = section.name.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0;
            if (!asteroid && !nebula)
                continue;
            const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
            if (relativeFile.isEmpty())
                continue;
            counts[normalizedRelativeKey(relativeFile)] += 1;
        }
    }
    return counts;
}

QHash<QString, int> collectRoomReferenceCounts(const IniDocument &universeDoc,
                                               const QString &universeFilePath,
                                               const QSet<QString> &skipBaseNicknames)
{
    QHash<QString, int> counts;
    for (const auto &section : universeDoc) {
        if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (skipBaseNicknames.contains(nickname.toLower()))
            continue;
        const QString baseFileRel = section.value(QStringLiteral("file")).trimmed();
        if (baseFileRel.isEmpty())
            continue;

        const QString baseIniPath = resolveDataFilePath(universeFilePath, baseFileRel);
        const IniDocument baseDoc = IniParser::parseFile(baseIniPath);
        for (const auto &baseSection : baseDoc) {
            if (baseSection.name.compare(QStringLiteral("Room"), Qt::CaseInsensitive) != 0)
                continue;
            const QString roomFileRel = baseSection.value(QStringLiteral("file")).trimmed();
            if (roomFileRel.isEmpty())
                continue;
            counts[normalizedRelativeKey(roomFileRel)] += 1;
        }
    }
    return counts;
}

DeleteSystemPrecheckReport buildPrecheckReport(const QString &universeFilePath,
                                               const UniverseData *data,
                                               const QString &systemNickname)
{
    DeleteSystemPrecheckReport report;
    report.precheckCompleted = true;
    report.universeFilePath = QDir::cleanPath(universeFilePath);
    report.systemNickname = systemNickname.trimmed();
    report.systemDisplayName = systemNickname.trimmed();

    if (report.systemNickname.isEmpty()) {
        appendUniqueBlocker(report.blockers, QObject::tr("Es wurde kein System-Nickname für den Löschvorgang übergeben."));
        return report;
    }

    if (data) {
        if (const auto *sys = data->findSystem(report.systemNickname))
            report.systemDisplayName = sys->displayName.trimmed().isEmpty() ? sys->nickname : sys->displayName;
    }

    const IniDocument universeDoc = IniParser::parseFile(universeFilePath);
    if (universeDoc.isEmpty()) {
        appendUniqueBlocker(report.blockers, QObject::tr("universe.ini konnte nicht gelesen werden."));
        return report;
    }

    QString systemFileRel;
    bool foundSystemSection = false;
    QSet<QString> baseNicknames;
    QVector<QString> baseFilePaths;

    for (const auto &section : universeDoc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.compare(report.systemNickname, Qt::CaseInsensitive) != 0)
                continue;
            foundSystemSection = true;
            systemFileRel = section.value(QStringLiteral("file")).trimmed();
            appendUniqueSectionChange(report.sectionChanges,
                                      universeFilePath,
                                      QStringLiteral("System"),
                                      nickname,
                                      QObject::tr("Systemeintrag wird aus universe.ini entfernt."));
        } else if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) == 0) {
            const QString baseSystem = section.value(QStringLiteral("system")).trimmed();
            const QString baseNickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString baseFileRel = section.value(QStringLiteral("file")).trimmed();
            if (baseSystem.compare(report.systemNickname, Qt::CaseInsensitive) != 0)
                continue;
            baseNicknames.insert(baseNickname.toLower());
            appendUniqueSectionChange(report.sectionChanges,
                                      universeFilePath,
                                      QStringLiteral("Base"),
                                      baseNickname,
                                      QObject::tr("Basiseintrag gehört zum gelöschten System."));
            if (!baseFileRel.isEmpty())
                baseFilePaths.append(resolveDataFilePath(universeFilePath, baseFileRel));
        }
    }

    if (!foundSystemSection) {
        appendUniqueBlocker(report.blockers,
                            QObject::tr("Für das System '%1' wurde kein [System]-Eintrag in universe.ini gefunden.")
                                .arg(report.systemNickname));
        return report;
    }

    if (systemFileRel.isEmpty()) {
        appendUniqueBlocker(report.blockers,
                            QObject::tr("Der [System]-Eintrag für '%1' enthält keinen file-Wert.")
                                .arg(report.systemNickname));
        return report;
    }

    report.systemFilePath = resolveSystemFilePath(universeFilePath, systemFileRel);
    report.systemFolderPath = QFileInfo(report.systemFilePath).absolutePath();

    if (!QFileInfo::exists(report.systemFilePath)) {
        appendUniqueBlocker(report.blockers,
                            QObject::tr("Die Systemdatei wurde nicht gefunden:\n%1").arg(report.systemFilePath));
        return report;
    }

    const IniDocument systemDoc = IniParser::parseFile(report.systemFilePath);
    if (systemDoc.isEmpty()) {
        appendUniqueBlocker(report.blockers,
                            QObject::tr("Die Systemdatei konnte nicht gelesen werden:\n%1").arg(report.systemFilePath));
        return report;
    }

    const QVector<QString> systemFiles = allSystemFilesFromUniverse(universeFilePath, universeDoc);
    const QHash<QString, int> fieldUsageCounts = collectFieldReferenceCounts(systemFiles, report.systemFilePath);
    const QHash<QString, int> roomUsageCounts = collectRoomReferenceCounts(universeDoc, universeFilePath, baseNicknames);

    QSet<QString> fieldFilesSeen;
    QSet<QString> jumpObjectsToRemove;
    QSet<QString> jumpZonesToRemove;

    for (const auto &section : systemDoc) {
        if (section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0
            || section.name.compare(QStringLiteral("Nebula"), Qt::CaseInsensitive) == 0) {
            const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
            if (relativeFile.isEmpty())
                continue;

            const QString key = normalizedRelativeKey(relativeFile);
            if (fieldFilesSeen.contains(key))
                continue;
            fieldFilesSeen.insert(key);

            const QString resolvedPath = resolveDataFilePath(universeFilePath, relativeFile);
            const bool asteroid = section.name.compare(QStringLiteral("Asteroids"), Qt::CaseInsensitive) == 0;
            const QString label = asteroid ? QObject::tr("Asteroidenfeld") : QObject::tr("Nebel");
            const QString normalizedResolved = normalizedPath(resolvedPath);
            if (!(normalizedResolved.contains(QStringLiteral("/solar/asteroids/"))
                  || normalizedResolved.contains(QStringLiteral("/solar/nebula/")))) {
                appendUniqueFileDecision(report.filesToKeep, resolvedPath,
                                         QObject::tr("%1-INI liegt außerhalb der sicheren SOLAR-Unterordner und wird nicht automatisch gelöscht.").arg(label));
                continue;
            }

            if (fieldUsageCounts.value(key) > 0) {
                appendUniqueFileDecision(report.filesToKeep, resolvedPath,
                                         QObject::tr("%1-INI wird noch von anderen Systemen verwendet.").arg(label));
            } else {
                appendUniqueFileDecision(report.filesToDelete, resolvedPath,
                                         QObject::tr("%1-INI wird nur von diesem System verwendet.").arg(label));
            }
        } else if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0) {
            const QString gotoValue = section.value(QStringLiteral("goto")).trimmed();
            if (gotoValue.isEmpty())
                continue;
            const QString targetSystem = gotoTargetSystem(gotoValue);
            if (targetSystem.compare(report.systemNickname, Qt::CaseInsensitive) != 0)
                continue;
            const QString objectNickname = section.value(QStringLiteral("nickname")).trimmed();
            if (!objectNickname.isEmpty()) {
                jumpObjectsToRemove.insert(objectNickname.toLower());
                jumpZonesToRemove.insert(QStringLiteral("zone_%1").arg(objectNickname).toLower());
            }
        }
    }

    for (const QString &baseFilePath : baseFilePaths) {
        if (!startsWithPath(baseFilePath, report.systemFolderPath)) {
            appendUniqueBlocker(report.blockers,
                                QObject::tr("Base-INI liegt außerhalb des Systemordners und kann nicht sicher mitgelöscht werden:\n%1")
                                    .arg(baseFilePath));
            continue;
        }

        const IniDocument baseDoc = IniParser::parseFile(baseFilePath);
        if (baseDoc.isEmpty()) {
            appendUniqueWarning(report.warnings,
                                QObject::tr("Base-INI konnte nicht gelesen werden:\n%1").arg(baseFilePath));
            continue;
        }

        for (const auto &baseSection : baseDoc) {
            if (baseSection.name.compare(QStringLiteral("Room"), Qt::CaseInsensitive) != 0)
                continue;
            const QString roomFileRel = baseSection.value(QStringLiteral("file")).trimmed();
            if (roomFileRel.isEmpty())
                continue;
            const QString roomPath = resolveDataFilePath(universeFilePath, roomFileRel);
            if (!startsWithPath(roomPath, report.systemFolderPath)) {
                appendUniqueBlocker(report.blockers,
                                    QObject::tr("Room-Datei liegt außerhalb des Systemordners und kann nicht sicher mitgelöscht werden:\n%1")
                                        .arg(roomPath));
                continue;
            }
            if (roomUsageCounts.value(normalizedRelativeKey(roomFileRel)) > 0) {
                appendUniqueBlocker(report.blockers,
                                    QObject::tr("Room-Datei wird noch von anderen Basen referenziert:\n%1").arg(roomPath));
            }
        }
    }

    for (const QString &filePath : filesUnderDirectory(report.systemFolderPath)) {
        appendUniqueFileDecision(report.filesToDelete, filePath,
                                 QObject::tr("Liegt im Systemordner und wird mit dem Systemordner entfernt."));
    }

    for (const QString &systemFile : systemFiles) {
        if (normalizedPath(systemFile) == normalizedPath(report.systemFilePath))
            continue;

        const IniDocument otherDoc = IniParser::parseFile(systemFile);
        if (otherDoc.isEmpty()) {
            appendUniqueWarning(report.warnings,
                                QObject::tr("Eine Nachbarsystem-Datei konnte für den Jump-Precheck nicht gelesen werden:\n%1").arg(systemFile));
            continue;
        }

        const QString otherNickname = QFileInfo(systemFile).completeBaseName();
        for (const auto &section : otherDoc) {
            if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
                continue;
            const QString gotoValue = section.value(QStringLiteral("goto")).trimmed();
            if (gotoTargetSystem(gotoValue).compare(report.systemNickname, Qt::CaseInsensitive) != 0)
                continue;

            DeleteSystemJumpCleanup cleanup;
            cleanup.systemNickname = otherNickname;
            cleanup.systemFilePath = systemFile;
            cleanup.objectNickname = section.value(QStringLiteral("nickname")).trimmed();
            cleanup.zoneNickname = cleanup.objectNickname.isEmpty()
                ? QString()
                : QStringLiteral("Zone_%1").arg(cleanup.objectNickname);
            report.jumpCleanups.append(cleanup);
            appendUniqueSectionChange(report.sectionChanges,
                                      systemFile,
                                      QStringLiteral("Object"),
                                      cleanup.objectNickname,
                                      QObject::tr("Dead jump counterpart points to the deleted system."));
            if (!cleanup.zoneNickname.isEmpty()) {
                appendUniqueSectionChange(report.sectionChanges,
                                          systemFile,
                                          QStringLiteral("Zone"),
                                          cleanup.zoneNickname,
                                          QObject::tr("Associated jump zone is removed together with the dead counterpart."));
            }
        }
    }

    report.canDelete = report.blockers.isEmpty();
    return report;
}

bool writeIniDocument(const QString &path, const IniDocument &doc, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Datei konnte nicht geschrieben werden:\n%1").arg(path);
        return false;
    }
    file.write(IniParser::serialize(doc).toUtf8());
    return true;
}

} // namespace

DeleteSystemPrecheckReport DeleteSystemService::precheck(const QString &universeFilePath,
                                                         const UniverseData *data,
                                                         const QString &systemNickname)
{
    return buildPrecheckReport(universeFilePath, data, systemNickname);
}

bool DeleteSystemService::execute(const DeleteSystemPrecheckReport &report,
                                  UniverseData *data,
                                  QString *errorMessage)
{
    if (!report.precheckCompleted) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Löschvorgang ist ohne erfolgreichen Precheck gesperrt.");
        return false;
    }
    if (!report.canDelete) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Löschvorgang ist wegen offener Blocker gesperrt.");
        return false;
    }

    const IniDocument universeDoc = IniParser::parseFile(report.universeFilePath);
    if (universeDoc.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("universe.ini konnte nicht erneut gelesen werden.");
        return false;
    }

    IniDocument newUniverseDoc;
    for (const auto &section : universeDoc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) == 0) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.compare(report.systemNickname, Qt::CaseInsensitive) == 0)
                continue;
        }
        if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) == 0) {
            const QString baseSystem = section.value(QStringLiteral("system")).trimmed();
            if (baseSystem.compare(report.systemNickname, Qt::CaseInsensitive) == 0)
                continue;
        }
        newUniverseDoc.append(section);
    }

    if (!writeIniDocument(report.universeFilePath, newUniverseDoc, errorMessage))
        return false;

    QHash<QString, QSet<QString>> objectsByFile;
    QHash<QString, QSet<QString>> zonesByFile;
    for (const auto &cleanup : report.jumpCleanups) {
        objectsByFile[normalizedPath(cleanup.systemFilePath)].insert(cleanup.objectNickname.toLower());
        if (!cleanup.zoneNickname.trimmed().isEmpty())
            zonesByFile[normalizedPath(cleanup.systemFilePath)].insert(cleanup.zoneNickname.toLower());
    }

    for (auto it = objectsByFile.constBegin(); it != objectsByFile.constEnd(); ++it) {
        const QString filePath = it.key();
        const IniDocument doc = IniParser::parseFile(filePath);
        if (doc.isEmpty()) {
            if (errorMessage)
                *errorMessage = QObject::tr("Jump-Gegenstück-Datei konnte nicht gelesen werden:\n%1").arg(filePath);
            return false;
        }

        IniDocument updated;
        const QSet<QString> objects = it.value();
        const QSet<QString> zones = zonesByFile.value(filePath);
        for (const auto &section : doc) {
            if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0) {
                const QString nickname = section.value(QStringLiteral("nickname")).trimmed().toLower();
                if (objects.contains(nickname))
                    continue;
            }
            if (section.name.compare(QStringLiteral("Zone"), Qt::CaseInsensitive) == 0) {
                const QString nickname = section.value(QStringLiteral("nickname")).trimmed().toLower();
                if (zones.contains(nickname))
                    continue;
            }
            updated.append(section);
        }

        if (!writeIniDocument(filePath, updated, errorMessage))
            return false;
    }

    for (const auto &fileDecision : report.filesToDelete) {
        const QFileInfo info(fileDecision.path);
        if (startsWithPath(fileDecision.path, report.systemFolderPath))
            continue;
        if (!info.exists())
            continue;
        if (!QFile::remove(fileDecision.path)) {
            if (errorMessage)
                *errorMessage = QObject::tr("Datei konnte nicht gelöscht werden:\n%1").arg(fileDecision.path);
            return false;
        }
    }

    if (QFileInfo(report.systemFolderPath).isDir()) {
        QDir dir(report.systemFolderPath);
        if (!dir.removeRecursively()) {
            if (errorMessage)
                *errorMessage = QObject::tr("Systemordner konnte nicht gelöscht werden:\n%1").arg(report.systemFolderPath);
            return false;
        }
    }

    if (data) {
        data->systems.erase(
            std::remove_if(data->systems.begin(), data->systems.end(),
                           [&](const SystemInfo &sys) {
                               return sys.nickname.compare(report.systemNickname, Qt::CaseInsensitive) == 0;
                           }),
            data->systems.end());

        data->connections.erase(
            std::remove_if(data->connections.begin(), data->connections.end(),
                           [&](const JumpConnection &conn) {
                               return conn.fromSystem.compare(report.systemNickname, Qt::CaseInsensitive) == 0
                                   || conn.toSystem.compare(report.systemNickname, Qt::CaseInsensitive) == 0;
                           }),
            data->connections.end());
    }

    return true;
}

} // namespace flatlas::editors
