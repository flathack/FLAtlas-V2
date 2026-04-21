// infrastructure/freelancer/ModelAssetScanner.cpp - context-aware Freelancer 3D model discovery

#include "ModelAssetScanner.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace flatlas::infrastructure {

namespace {

QString normalizeModelKey(const QString &value)
{
    QString normalized = QDir::fromNativeSeparators(value).trimmed();
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}

QString categoryLabelForRelativePath(const QString &relativePath)
{
    const QString normalized = normalizeModelKey(relativePath);
    if (normalized.startsWith(QStringLiteral("solar/")))
        return QObject::tr("Solar");
    if (normalized.startsWith(QStringLiteral("ships/")))
        return QObject::tr("Ships");
    if (normalized.startsWith(QStringLiteral("equipment/")))
        return QObject::tr("Equipment");
    return QObject::tr("Models");
}

void upsertReferencedEntry(QVector<ModelAssetEntry> &entries,
                           QSet<QString> &seenKeys,
                           const QString &categoryLabel,
                           const QString &dataDir,
                           const QString &sourceIniPath,
                           const IniSection &section,
                           const IdsStringTable &ids)
{
    const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
    const QString archetypePath = section.value(QStringLiteral("DA_archetype")).trimmed();
    if (nickname.isEmpty() || archetypePath.isEmpty())
        return;

    const QString resolvedPath = flatlas::core::PathUtils::ciResolvePath(dataDir, archetypePath);
    if (resolvedPath.isEmpty())
        return;

    const QFileInfo modelInfo(resolvedPath);
    if (!modelInfo.exists())
        return;

    const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt();
    const QString displayName = idsName > 0 ? ids.getString(idsName).trimmed() : QString();
    const QString uniqueKey = QStringLiteral("%1|%2")
        .arg(nickname.trimmed().toLower(), normalizeModelKey(resolvedPath));
    if (seenKeys.contains(uniqueKey))
        return;

    seenKeys.insert(uniqueKey);

    ModelAssetEntry entry;
    entry.categoryKey = categoryLabel.toLower();
    entry.categoryLabel = categoryLabel;
    entry.nickname = nickname;
    entry.displayName = displayName;
    entry.archetype = nickname;
    entry.modelPath = modelInfo.absoluteFilePath();
    entry.relativeModelPath = QDir(dataDir).relativeFilePath(modelInfo.absoluteFilePath());
    entry.sourceIniPath = sourceIniPath;
    entry.idsName = idsName;
    entry.typeValue = section.value(QStringLiteral("type")).trimmed();
    entry.renderKind = QStringLiteral("Freelancer Geometry");
    entries.append(entry);
}

void collectReferencedModels(const QString &dataDir,
                             const QString &exeDir,
                             QVector<ModelAssetEntry> &entries,
                             QSet<QString> &seenKeys)
{
    IdsStringTable ids;
    if (QDir(exeDir).exists())
        ids.loadFromFreelancerDir(exeDir);

    const struct SourceSpec {
        QString relativePath;
        QString categoryLabel;
    } sources[] = {
        {QStringLiteral("SOLAR/solararch.ini"), QObject::tr("Solar")},
        {QStringLiteral("SHIPS/shiparch.ini"), QObject::tr("Ships")},
        {QStringLiteral("EQUIPMENT/st_equip.ini"), QObject::tr("Equipment")},
    };

    for (const auto &source : sources) {
        const QString iniPath = flatlas::core::PathUtils::ciResolvePath(dataDir, source.relativePath);
        if (iniPath.isEmpty())
            continue;

        const IniDocument doc = IniParser::parseFile(iniPath);
        for (const auto &section : doc)
            upsertReferencedEntry(entries, seenKeys, source.categoryLabel, dataDir, iniPath, section, ids);
    }
}

void collectLooseModels(const QString &dataDir,
                        QVector<ModelAssetEntry> &entries,
                        QSet<QString> &seenPaths)
{
    QDirIterator it(dataDir,
                    QStringList() << QStringLiteral("*.cmp") << QStringLiteral("*.3db"),
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString absolutePath = it.next();
        const QString normalizedPath = normalizeModelKey(absolutePath);
        if (seenPaths.contains(normalizedPath))
            continue;

        seenPaths.insert(normalizedPath);
        QFileInfo info(absolutePath);
        ModelAssetEntry entry;
        entry.modelPath = info.absoluteFilePath();
        entry.relativeModelPath = QDir(dataDir).relativeFilePath(info.absoluteFilePath());
        entry.categoryLabel = categoryLabelForRelativePath(entry.relativeModelPath);
        entry.categoryKey = entry.categoryLabel.toLower();
        entry.nickname = info.completeBaseName();
        entry.archetype = info.completeBaseName();
        entry.renderKind = QStringLiteral("Freelancer Geometry");
        entries.append(entry);
    }
}

} // namespace

QString ModelAssetEntry::titleText() const
{
    return displayName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, displayName);
}

QString ModelAssetEntry::searchBlob() const
{
    return QStringLiteral("%1 %2 %3 %4 %5 %6")
        .arg(categoryLabel, nickname, displayName, archetype, relativeModelPath, sourceIniPath)
        .toLower();
}

QVector<ModelAssetEntry> ModelAssetScanner::scanCurrentContext()
{
    return scanInstallation(flatlas::core::EditingContext::instance().primaryGamePath());
}

QVector<ModelAssetEntry> ModelAssetScanner::scanInstallation(const QString &gamePath)
{
    QVector<ModelAssetEntry> entries;
    if (gamePath.isEmpty())
        return entries;

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
    if (dataDir.isEmpty())
        return entries;

    QSet<QString> seenReferencedKeys;
    collectReferencedModels(dataDir, exeDir, entries, seenReferencedKeys);

    QSet<QString> seenPaths;
    for (const auto &entry : entries)
        seenPaths.insert(normalizeModelKey(entry.modelPath));
    collectLooseModels(dataDir, entries, seenPaths);

    std::sort(entries.begin(), entries.end(), [](const ModelAssetEntry &lhs, const ModelAssetEntry &rhs) {
        if (lhs.categoryLabel.compare(rhs.categoryLabel, Qt::CaseInsensitive) != 0)
            return lhs.categoryLabel.compare(rhs.categoryLabel, Qt::CaseInsensitive) < 0;
        if (lhs.titleText().compare(rhs.titleText(), Qt::CaseInsensitive) != 0)
            return lhs.titleText().compare(rhs.titleText(), Qt::CaseInsensitive) < 0;
        return lhs.relativeModelPath.compare(rhs.relativeModelPath, Qt::CaseInsensitive) < 0;
    });

    return entries;
}

} // namespace flatlas::infrastructure
