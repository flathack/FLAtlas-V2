#include "FactionRepository.h"

#include "core/PathUtils.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <algorithm>

namespace flatlas::infrastructure {

using namespace flatlas::domain;

namespace {

QStringList splitPair(const QString &value)
{
    QStringList parts;
    for (const QString &part : value.split(QLatin1Char(','), Qt::KeepEmptyParts))
        parts.append(part.trimmed());
    return parts;
}

double parseDouble(const QString &value, bool *ok = nullptr)
{
    return value.trimmed().toDouble(ok);
}

QString formatDouble(double value)
{
    QString text = QString::number(value, 'f', 6);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text.isEmpty() ? QStringLiteral("0") : text;
}

QString existingOrTargetPath(const QString &gameRoot, const QString &relativePath)
{
    const QString resolved = flatlas::core::PathUtils::ciResolvePath(gameRoot, relativePath);
    if (!resolved.isEmpty() && QFileInfo::exists(resolved))
        return resolved;
    return QDir(gameRoot).filePath(relativePath);
}

void parseInitialWorld(const IniDocument &doc, FactionWorld *world)
{
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("locked_gates"), Qt::CaseInsensitive) == 0) {
            world->lockedGateEntries = section.entries;
            continue;
        }
        if (section.name.compare(QStringLiteral("group"), Qt::CaseInsensitive) != 0)
            continue;

        Faction faction;
        for (const IniEntry &entry : section.entries) {
            const QString key = entry.first.trimmed().toLower();
            const QString value = entry.second.trimmed();
            if (key == QStringLiteral("nickname"))
                faction.nickname = value;
            else if (key == QStringLiteral("ids_name"))
                faction.idsName = value;
            else if (key == QStringLiteral("ids_info"))
                faction.idsInfo = value;
            else if (key == QStringLiteral("ids_short_name"))
                faction.idsShortName = value;
            else if (key == QStringLiteral("rep")) {
                const QStringList parts = splitPair(value);
                if (parts.size() >= 2) {
                    bool ok = false;
                    const double rep = parseDouble(parts.at(0), &ok);
                    if (ok)
                        faction.reputations.append({parts.at(1), rep});
                }
            }
        }
        if (faction.nickname.trimmed().isEmpty())
            continue;

        Faction merged = world->faction(faction.nickname) ? *world->faction(faction.nickname) : Faction{};
        merged.nickname = faction.nickname;
        merged.idsName = faction.idsName;
        merged.idsInfo = faction.idsInfo;
        merged.idsShortName = faction.idsShortName;
        merged.reputations = faction.reputations;
        merged.inInitialWorld = true;
        world->upsertFaction(merged);
    }
}

void parseEmpathy(const IniDocument &doc, FactionWorld *world)
{
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("repchangeeffects"), Qt::CaseInsensitive) != 0)
            continue;

        QString group;
        QList<EmpathyEvent> events;
        QList<EmpathyRate> rates;
        for (const IniEntry &entry : section.entries) {
            const QString key = entry.first.trimmed().toLower();
            const QString value = entry.second.trimmed();
            if (key == QStringLiteral("group")) {
                group = value;
            } else if (key == QStringLiteral("event")) {
                const QStringList parts = splitPair(value);
                if (parts.size() >= 2) {
                    bool ok = false;
                    const double eventValue = parseDouble(parts.at(1), &ok);
                    if (ok)
                        events.append({parts.at(0), eventValue});
                }
            } else if (key == QStringLiteral("empathy_rate")) {
                const QStringList parts = splitPair(value);
                if (parts.size() >= 2) {
                    bool ok = false;
                    const double rate = parseDouble(parts.at(1), &ok);
                    if (ok)
                        rates.append({parts.at(0), rate});
                }
            }
        }
        if (group.trimmed().isEmpty())
            continue;

        Faction merged = world->faction(group) ? *world->faction(group) : Faction{};
        if (merged.nickname.isEmpty())
            merged.nickname = group;
        merged.empathyEvents = events;
        merged.empathyRates = rates;
        merged.inEmpathy = true;
        world->upsertFaction(merged);
    }
}

void parseFactionProps(const IniDocument &doc, FactionWorld *world)
{
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("factionprops"), Qt::CaseInsensitive) != 0)
            continue;

        FactionPropData props;
        for (const IniEntry &entry : section.entries) {
            const QString key = entry.first.trimmed().toLower();
            const QString value = entry.second.trimmed();
            if (key == QStringLiteral("affiliation"))
                props.affiliation = value;
            else if (key == QStringLiteral("legality"))
                props.legality = value;
            else if (key == QStringLiteral("nickname_plurality"))
                props.nicknamePlurality = value;
            else if (key == QStringLiteral("msg_id_prefix"))
                props.msgIdPrefix = value;
            else if (key == QStringLiteral("jump_preference"))
                props.jumpPreference = value;
            else if (key == QStringLiteral("npc_ship"))
                props.npcShips.append(value);
            else if (key == QStringLiteral("voice"))
                props.voices.append(value);
            else if (key == QStringLiteral("mc_costume"))
                props.mcCostume = value;
            else if (key == QStringLiteral("space_costume"))
                props.spaceCostumes.append(value);
            else if (key == QStringLiteral("firstname_male"))
                props.firstnameMale = value;
            else if (key == QStringLiteral("firstname_female"))
                props.firstnameFemale = value;
            else if (key == QStringLiteral("lastname"))
                props.lastname = value;
            else if (key == QStringLiteral("rank_desig"))
                props.rankDesig = value;
            else if (key == QStringLiteral("formation_desig"))
                props.formationDesig = value;
            else if (key == QStringLiteral("large_ship_desig"))
                props.largeShipDesig = value;
            else if (key == QStringLiteral("large_ship_names"))
                props.largeShipNames = value;
            else if (key == QStringLiteral("scan_for_cargo"))
                props.scanForCargo.append(value);
            else if (key == QStringLiteral("scan_announce"))
                props.scanAnnounce = value;
            else if (key == QStringLiteral("scan_chance"))
                props.scanChance = value;
            else if (key == QStringLiteral("formation"))
                props.formations.append(value);
        }
        if (props.affiliation.trimmed().isEmpty())
            continue;

        Faction merged = world->faction(props.affiliation) ? *world->faction(props.affiliation) : Faction{};
        if (merged.nickname.isEmpty())
            merged.nickname = props.affiliation;
        merged.props = props;
        merged.inFactionProp = true;
        world->upsertFaction(merged);
    }
}

IniSection makeInitialWorldSection(const Faction &faction)
{
    IniSection section;
    section.name = QStringLiteral("Group");
    section.entries.append({QStringLiteral("nickname"), faction.nickname});
    if (!faction.idsName.isEmpty())
        section.entries.append({QStringLiteral("ids_name"), faction.idsName});
    if (!faction.idsInfo.isEmpty())
        section.entries.append({QStringLiteral("ids_info"), faction.idsInfo});
    if (!faction.idsShortName.isEmpty())
        section.entries.append({QStringLiteral("ids_short_name"), faction.idsShortName});
    for (const FactionRep &rep : faction.reputations)
        section.entries.append({QStringLiteral("rep"), QStringLiteral("%1, %2").arg(formatDouble(rep.value), rep.target)});
    return section;
}

IniSection makeEmpathySection(const Faction &faction)
{
    IniSection section;
    section.name = QStringLiteral("RepChangeEffects");
    section.entries.append({QStringLiteral("group"), faction.nickname});
    for (const EmpathyEvent &event : faction.empathyEvents)
        section.entries.append({QStringLiteral("event"), QStringLiteral("%1, %2").arg(event.eventType, formatDouble(event.value))});
    for (const EmpathyRate &rate : faction.empathyRates)
        section.entries.append({QStringLiteral("empathy_rate"), QStringLiteral("%1, %2").arg(rate.target, formatDouble(rate.rate))});
    return section;
}

void appendIfNotEmpty(IniSection *section, const QString &key, const QString &value)
{
    if (!value.trimmed().isEmpty())
        section->entries.append({key, value.trimmed()});
}

void appendList(IniSection *section, const QString &key, const QStringList &values)
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            section->entries.append({key, value.trimmed()});
    }
}

IniSection makeFactionPropsSection(const Faction &faction)
{
    IniSection section;
    section.name = QStringLiteral("FactionProps");
    const FactionPropData &p = faction.props;
    section.entries.append({QStringLiteral("affiliation"), p.affiliation.isEmpty() ? faction.nickname : p.affiliation});
    appendIfNotEmpty(&section, QStringLiteral("legality"), p.legality);
    appendIfNotEmpty(&section, QStringLiteral("nickname_plurality"), p.nicknamePlurality);
    appendIfNotEmpty(&section, QStringLiteral("msg_id_prefix"), p.msgIdPrefix);
    appendIfNotEmpty(&section, QStringLiteral("jump_preference"), p.jumpPreference);
    appendList(&section, QStringLiteral("npc_ship"), p.npcShips);
    appendList(&section, QStringLiteral("voice"), p.voices);
    appendIfNotEmpty(&section, QStringLiteral("mc_costume"), p.mcCostume);
    appendList(&section, QStringLiteral("space_costume"), p.spaceCostumes);
    appendIfNotEmpty(&section, QStringLiteral("firstname_male"), p.firstnameMale);
    appendIfNotEmpty(&section, QStringLiteral("firstname_female"), p.firstnameFemale);
    appendIfNotEmpty(&section, QStringLiteral("lastname"), p.lastname);
    appendIfNotEmpty(&section, QStringLiteral("rank_desig"), p.rankDesig);
    appendIfNotEmpty(&section, QStringLiteral("formation_desig"), p.formationDesig);
    appendIfNotEmpty(&section, QStringLiteral("large_ship_desig"), p.largeShipDesig);
    appendIfNotEmpty(&section, QStringLiteral("large_ship_names"), p.largeShipNames);
    appendList(&section, QStringLiteral("scan_for_cargo"), p.scanForCargo);
    appendIfNotEmpty(&section, QStringLiteral("scan_announce"), p.scanAnnounce);
    appendIfNotEmpty(&section, QStringLiteral("scan_chance"), p.scanChance);
    appendList(&section, QStringLiteral("formation"), p.formations);
    return section;
}

bool writeTextFile(const QString &path, const QString &text, QString *errorMessage)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

} // namespace

FactionRepositoryResult FactionRepository::load(const QString &gameRoot) const
{
    FactionRepositoryResult result;
    if (gameRoot.trimmed().isEmpty()) {
        result.warnings.append(QStringLiteral("No active game path"));
        return result;
    }

    const QString iw = initialWorldPath(gameRoot);
    const QString emp = empathyPath(gameRoot);
    const QString fp = factionPropPath(gameRoot);

    if (QFileInfo::exists(iw))
        parseInitialWorld(IniParser::parseFile(iw), &result.world);
    else
        result.warnings.append(QStringLiteral("initialworld.ini not found"));

    if (QFileInfo::exists(emp))
        parseEmpathy(IniParser::parseFile(emp), &result.world);
    else
        result.warnings.append(QStringLiteral("empathy.ini not found"));

    if (QFileInfo::exists(fp))
        parseFactionProps(IniParser::parseFile(fp), &result.world);
    else
        result.warnings.append(QStringLiteral("faction_prop.ini not found"));

    return result;
}

bool FactionRepository::save(const FactionWorld &world, const QString &gameRoot, QString *errorMessage) const
{
    IniDocument initialWorld;
    IniDocument empathy;
    IniDocument factionProps;

    for (const QString &nickname : world.sortedNicknames()) {
        const Faction *faction = world.faction(nickname);
        if (!faction)
            continue;
        if (faction->inInitialWorld)
            initialWorld.append(makeInitialWorldSection(*faction));
        if (faction->inEmpathy)
            empathy.append(makeEmpathySection(*faction));
        if (faction->inFactionProp)
            factionProps.append(makeFactionPropsSection(*faction));
    }

    if (!world.lockedGateEntries.isEmpty()) {
        IniSection locked;
        locked.name = QStringLiteral("locked_gates");
        locked.entries = world.lockedGateEntries;
        initialWorld.append(locked);
    }

    if (!writeTextFile(initialWorldPath(gameRoot), IniParser::serialize(initialWorld), errorMessage))
        return false;
    if (!writeTextFile(empathyPath(gameRoot), IniParser::serialize(empathy), errorMessage))
        return false;
    if (!writeTextFile(factionPropPath(gameRoot), IniParser::serialize(factionProps), errorMessage))
        return false;
    return true;
}

QString FactionRepository::initialWorldPath(const QString &gameRoot) const
{
    return existingOrTargetPath(gameRoot, QStringLiteral("DATA/initialworld.ini"));
}

QString FactionRepository::empathyPath(const QString &gameRoot) const
{
    return existingOrTargetPath(gameRoot, QStringLiteral("DATA/MISSIONS/empathy.ini"));
}

QString FactionRepository::factionPropPath(const QString &gameRoot) const
{
    return existingOrTargetPath(gameRoot, QStringLiteral("DATA/MISSIONS/faction_prop.ini"));
}

} // namespace flatlas::infrastructure
