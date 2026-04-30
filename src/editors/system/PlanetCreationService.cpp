#include "PlanetCreationService.h"

#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/parser/IniParser.h"
#include "infrastructure/parser/XmlInfocard.h"

#include <QDirIterator>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}

int radiusFromArchetypeSuffix(const QString &archetype)
{
    static const QRegularExpression suffixPattern(QStringLiteral("_(\\d+(?:\\.\\d+)?)\\s*$"));
    const QRegularExpressionMatch match = suffixPattern.match(archetype.trimmed());
    if (!match.hasMatch())
        return 0;

    bool ok = false;
    const int radius = qRound(match.captured(1).toDouble(&ok));
    return ok && radius > 0 ? radius : 0;
}

PlanetArchetypeOption makeOption(const IniSection &section)
{
    PlanetArchetypeOption option;
    option.archetype = section.value(QStringLiteral("nickname")).trimmed();

    bool ok = false;
    const int solarRadius = section.value(QStringLiteral("solar_radius")).trimmed().toInt(&ok);
    option.solarRadius = ok && solarRadius > 0 ? solarRadius : radiusFromArchetypeSuffix(option.archetype);
    return option;
}

QHash<int, QString> idsPlainTextByGlobalId(const QString &gameRoot)
{
    QHash<int, QString> values;
    const IdsDataset dataset = IdsDataService::loadFromGameRoot(gameRoot);
    for (const IdsEntryRecord &entry : dataset.entries) {
        if (!entry.hasHtmlValue || entry.globalId <= 0)
            continue;
        const QString text = entry.plainText.trimmed();
        if (!text.isEmpty())
            values.insert(entry.globalId, text);
    }
    return values;
}

void applySuggestion(PlanetArchetypeOption *option,
                     int idsInfo,
                     const QString &nickname,
                     const QHash<int, QString> &plainTextById)
{
    if (!option || !option->suggestedInfocardText.trimmed().isEmpty() || idsInfo <= 0)
        return;

    const QString text = plainTextById.value(idsInfo).trimmed();
    if (text.isEmpty())
        return;

    option->sourceIdsInfo = idsInfo;
    option->sourceObjectNickname = nickname.trimmed();
    option->suggestedInfocardText = text;
}

PlanetCreationCatalog buildBaseCatalog(const QString &gameRoot)
{
    PlanetCreationCatalog catalog;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString solarArchPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (solarArchPath.isEmpty())
        return catalog;

    QHash<QString, PlanetArchetypeOption> optionsByKey;
    const IniDocument solarArch = IniParser::parseFile(solarArchPath);
    for (const IniSection &section : solarArch) {
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;

        const QString type = section.value(QStringLiteral("type")).trimmed();
        if (type.compare(QStringLiteral("PLANET"), Qt::CaseInsensitive) != 0
            && !nickname.contains(QStringLiteral("planet"), Qt::CaseInsensitive)) {
            continue;
        }

        optionsByKey.insert(normalizedKey(nickname), makeOption(section));
    }

    const QHash<int, QString> plainTextById = idsPlainTextByGlobalId(gameRoot);
    const QString systemsDir =
        flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA/UNIVERSE/SYSTEMS"));
    if (!systemsDir.isEmpty()) {
        QDirIterator it(systemsDir, QStringList{QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const IniDocument systemDoc = IniParser::parseFile(it.next());
            for (const IniSection &section : systemDoc) {
                if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
                    continue;

                const QString archetype = section.value(QStringLiteral("archetype")).trimmed();
                auto optionIt = optionsByKey.find(normalizedKey(archetype));
                if (optionIt == optionsByKey.end())
                    continue;

                bool ok = false;
                const int idsInfo = section.value(QStringLiteral("ids_info")).trimmed().toInt(&ok);
                if (!ok || idsInfo <= 0)
                    continue;

                applySuggestion(&optionIt.value(),
                                idsInfo,
                                section.value(QStringLiteral("nickname")).trimmed(),
                                plainTextById);
            }
        }
    }

    catalog.options = optionsByKey.values().toVector();
    std::sort(catalog.options.begin(), catalog.options.end(), [](const PlanetArchetypeOption &lhs,
                                                                 const PlanetArchetypeOption &rhs) {
        return lhs.archetype.compare(rhs.archetype, Qt::CaseInsensitive) < 0;
    });
    return catalog;
}

const PlanetCreationCatalog &cachedBaseCatalog(const QString &gameRoot)
{
    static QString cachedKey;
    static PlanetCreationCatalog cachedCatalog;

    const QString key = normalizedKey(gameRoot);
    if (key == cachedKey)
        return cachedCatalog;

    cachedKey = key;
    cachedCatalog = buildBaseCatalog(gameRoot);
    return cachedCatalog;
}

} // namespace

QStringList PlanetCreationCatalog::archetypeNicknames() const
{
    QStringList values;
    values.reserve(options.size());
    for (const PlanetArchetypeOption &option : options)
        values.append(option.archetype);
    return values;
}

PlanetArchetypeOption PlanetCreationCatalog::optionForArchetype(const QString &archetype) const
{
    const QString key = normalizedKey(archetype);
    for (const PlanetArchetypeOption &option : options) {
        if (normalizedKey(option.archetype) == key)
            return option;
    }
    return {};
}

int PlanetCreationCatalog::solarRadiusForArchetype(const QString &archetype) const
{
    return optionForArchetype(archetype).solarRadius;
}

PlanetCreationCatalog PlanetCreationService::loadCatalog(const flatlas::domain::SystemDocument *document,
                                                         const QString &gameRoot)
{
    PlanetCreationCatalog catalog = cachedBaseCatalog(gameRoot);
    if (!document)
        return catalog;

    const QHash<int, QString> plainTextById = idsPlainTextByGlobalId(gameRoot);
    for (const std::shared_ptr<flatlas::domain::SolarObject> &object : document->objects()) {
        if (!object)
            continue;

        const QString archetype = object->archetype().trimmed();
        for (PlanetArchetypeOption &option : catalog.options) {
            if (normalizedKey(option.archetype) != normalizedKey(archetype))
                continue;
            applySuggestion(&option, object->idsInfo(), object->nickname(), plainTextById);
            break;
        }
    }
    return catalog;
}

int PlanetCreationService::derivePlanetRadius(const QString &archetype,
                                              const PlanetCreationCatalog &catalog)
{
    const int catalogRadius = catalog.solarRadiusForArchetype(archetype);
    return catalogRadius > 0 ? catalogRadius : radiusFromArchetypeSuffix(archetype);
}

int PlanetCreationService::defaultDeathZoneRadius(int solarRadius)
{
    return solarRadius > 0 ? solarRadius + 100 : 1100;
}

int PlanetCreationService::defaultAtmosphereRange(int solarRadius)
{
    return solarRadius > 0 ? solarRadius + 200 : 1200;
}

QString PlanetCreationService::wrapInfocardXml(const QString &plainText)
{
    return XmlInfocard::wrapAsInfocard(plainText.trimmed());
}

bool PlanetCreationService::isValidNickname(const QString &nickname)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_]+$"));
    return pattern.match(nickname.trimmed()).hasMatch();
}

} // namespace flatlas::editors