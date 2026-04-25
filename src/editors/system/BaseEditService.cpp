#include "BaseEditService.h"

#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/view2d/MapScene.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace flatlas::editors {
namespace {

using flatlas::domain::SolarObject;
using flatlas::domain::SystemDocument;
using flatlas::infrastructure::IdsDataService;
using flatlas::infrastructure::IdsDataset;
using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString normalizeKey(const QString &value)
{
    return value.trimmed().toLower();
}

QString rawEntryValue(const QVector<QPair<QString, QString>> &entries, const QString &key)
{
    for (const auto &entry : entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) == 0)
            return entry.second.trimmed();
    }
    return {};
}

int rawEntryIntValue(const QVector<QPair<QString, QString>> &entries, const QString &key)
{
    bool ok = false;
    const int value = rawEntryValue(entries, key).toInt(&ok);
    return ok ? value : 0;
}

QString nicknameFromDisplayValue(const QString &value)
{
    const QString trimmed = value.trimmed();
    const int separator = trimmed.indexOf(QStringLiteral(" - "));
    return separator >= 0 ? trimmed.left(separator).trimmed() : trimmed;
}

QPair<QString, QString> splitSpaceCostume(const QString &value)
{
    const QStringList parts = value.split(QLatin1Char(','), Qt::KeepEmptyParts);
    return {parts.value(0).trimmed(), parts.value(1).trimmed()};
}

QString normalizeRotateValue(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("0, 0, 0") : value.trimmed();
}

QString safeNickPart(const QString &value)
{
    QString out;
    const QString lowered = value.trimmed().toLower();
    for (const QChar ch : lowered) {
        out.append((ch.isLetterOrNumber() || ch == QLatin1Char('_')) ? ch : QLatin1Char('_'));
    }
    while (out.contains(QStringLiteral("__")))
        out.replace(QStringLiteral("__"), QStringLiteral("_"));
    return out.trimmed().trimmed().remove(QRegularExpression(QStringLiteral("^_+|_+$")));
}

QPair<int, int> mbaseBlockRange(const IniDocument &doc, int mbaseIndex);
QString extractScenePath(QString content);
QString generateRoomIniText(const QString &roomName, const QStringList &allRooms, const QString &startRoom);
QString textForPath(const QString &path, const QHash<QString, QString> &overrides);
QString resolvedDisplayName(const QString &gameRoot, int idsName);

struct ExistingBaseObjectData {
    QString objectNickname;
    QString baseNickname;
    QString loadout;
    int idsInfo = 0;
};

QStringList universeSystemFilePaths(const QString &gameRoot, const QHash<QString, QString> &textOverrides)
{
    QStringList systemFiles;
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString universeIni = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (universeIni.isEmpty())
        return systemFiles;

    const IniDocument universeDoc = IniParser::parseText(textForPath(universeIni, textOverrides));
    QSet<QString> seen;
    for (const IniSection &section : universeDoc) {
        if (section.name.compare(QStringLiteral("System"), Qt::CaseInsensitive) != 0)
            continue;
        const QString relativePath = section.value(QStringLiteral("file")).trimmed();
        const QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        const QString key = QDir::cleanPath(absolutePath).toLower();
        if (absolutePath.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        systemFiles.append(absolutePath);
    }

    std::sort(systemFiles.begin(), systemFiles.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    return systemFiles;
}

QVector<ExistingBaseObjectData> existingBaseObjectsForArchetype(const QString &archetype,
                                                                const QString &gameRoot,
                                                                const QHash<QString, QString> &textOverrides)
{
    QVector<ExistingBaseObjectData> matches;
    const QString targetArchetype = normalizeKey(archetype);
    if (targetArchetype.isEmpty())
        return matches;

    for (const QString &systemFile : universeSystemFilePaths(gameRoot, textOverrides)) {
        const IniDocument systemDoc = IniParser::parseText(textForPath(systemFile, textOverrides));
        for (const IniSection &section : systemDoc) {
            if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
                continue;
            if (normalizeKey(section.value(QStringLiteral("archetype"))) != targetArchetype)
                continue;

            const QString baseNickname = section.value(QStringLiteral("base")).trimmed().isEmpty()
                ? section.value(QStringLiteral("dock_with")).trimmed()
                : section.value(QStringLiteral("base")).trimmed();
            if (baseNickname.isEmpty())
                continue;

            ExistingBaseObjectData match;
            match.objectNickname = section.value(QStringLiteral("nickname")).trimmed();
            match.baseNickname = baseNickname;
            match.loadout = section.value(QStringLiteral("loadout")).trimmed();
            match.idsInfo = rawEntryIntValue(section.entries, QStringLiteral("ids_info"));
            matches.append(match);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const ExistingBaseObjectData &left, const ExistingBaseObjectData &right) {
        const int baseCompare = left.baseNickname.compare(right.baseNickname, Qt::CaseInsensitive);
        if (baseCompare != 0)
            return baseCompare < 0;
        return left.objectNickname.compare(right.objectNickname, Qt::CaseInsensitive) < 0;
    });
    return matches;
}

QString defaultLoadoutFromSolarArch(const QString &archetype, const QString &gameRoot)
{
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString solarArchPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (solarArchPath.isEmpty())
        return {};

    const IniDocument solarArchDoc = IniParser::parseFile(solarArchPath);
    for (const IniSection &section : solarArchDoc) {
        if (section.name.compare(QStringLiteral("Solar"), Qt::CaseInsensitive) != 0)
            continue;
        if (normalizeKey(section.value(QStringLiteral("nickname"))) != normalizeKey(archetype))
            continue;
        return section.value(QStringLiteral("loadout")).trimmed();
    }
    return {};
}

void setOrRemoveRawEntry(QVector<QPair<QString, QString>> *entries,
                         const QString &key,
                         const QString &value)
{
    if (!entries)
        return;

    const QString trimmed = value.trimmed();
    for (int index = entries->size() - 1; index >= 0; --index) {
        if (entries->at(index).first.compare(key, Qt::CaseInsensitive) == 0) {
            if (trimmed.isEmpty()) {
                entries->removeAt(index);
                continue;
            }
            (*entries)[index].second = trimmed;
            return;
        }
    }

    if (!trimmed.isEmpty())
        entries->append({key, trimmed});
}

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QByteArray raw = file.readAll();
    QString text = QString::fromUtf8(raw);
    if (text.isEmpty() && !raw.isEmpty())
        text = QString::fromLocal8Bit(raw);
    return text;
}

QString textForPath(const QString &path, const QHash<QString, QString> &overrides)
{
    const QString key = QDir::cleanPath(path).toLower();
    if (overrides.contains(key))
        return overrides.value(key);
    return readTextFile(path);
}

QString generatedBaseStem(const QString &baseNickname)
{
    QString stem = baseNickname.trimmed().toLower();
    if (stem.endsWith(QStringLiteral("_base")))
        stem.chop(5);
    return stem;
}

QString canonicalRoomName(const QString &roomName)
{
    const QString room = roomName.trimmed().toLower();
    if (room == QStringLiteral("shipdealer") || room == QStringLiteral("ship_dealer"))
        return QStringLiteral("ShipDealer");
    if (room == QStringLiteral("equipment") || room == QStringLiteral("equip"))
        return QStringLiteral("Equipment");
    if (room == QStringLiteral("deck"))
        return QStringLiteral("Deck");
    if (room == QStringLiteral("cityscape"))
        return QStringLiteral("Cityscape");
    if (room == QStringLiteral("bar"))
        return QStringLiteral("Bar");
    if (room == QStringLiteral("trader") || room == QStringLiteral("commoditytrader"))
        return QStringLiteral("Trader");
    return roomName.trimmed();
}

QString roomKey(const QString &roomName)
{
    const QString room = roomName.trimmed().toLower();
    if (room == QStringLiteral("shipdealer") || room == QStringLiteral("ship_dealer") || room == QStringLiteral("ship-dealer"))
        return QStringLiteral("shipdealer");
    if (room == QStringLiteral("equipment") || room == QStringLiteral("equip"))
        return QStringLiteral("equipment");
    if (room == QStringLiteral("trader") || room == QStringLiteral("commoditytrader"))
        return QStringLiteral("trader");
    return room;
}

QStringList allowedRolesForRoom(const QString &roomName)
{
    const QString room = roomKey(roomName);
    if (room == QStringLiteral("bar"))
        return {QStringLiteral("bartender"), QStringLiteral("BarFly"), QStringLiteral("NewsVendor")};
    if (room == QStringLiteral("trader"))
        return {QStringLiteral("trader")};
    if (room == QStringLiteral("equipment"))
        return {QStringLiteral("Equipment")};
    if (room == QStringLiteral("shipdealer"))
        return {QStringLiteral("ShipDealer")};
    if (room == QStringLiteral("deck"))
        return {QStringLiteral("ShipDealer"), QStringLiteral("trader"), QStringLiteral("Equipment"), QStringLiteral("bartender")};
    if (room == QStringLiteral("cityscape"))
        return {QStringLiteral("trader")};
    return {QStringLiteral("trader")};
}

QString normalizedRoleForRoom(const QString &role, const QString &roomName)
{
    const QStringList allowed = allowedRolesForRoom(roomName);
    const QString raw = role.trimmed();
    if (raw.isEmpty())
        return allowed.value(0);
    for (const QString &candidate : allowed) {
        if (candidate.compare(raw, Qt::CaseInsensitive) == 0)
            return candidate;
    }
    return allowed.value(0);
}

QPair<QString, QString> fixtureSceneForRole(const QString &role)
{
    const QString roleText = role.trimmed().toLower();
    if (roleText == QStringLiteral("shipdealer"))
        return {QStringLiteral("scripts\\vendors\\li_shipdealer_fidget.thn"), QStringLiteral("ShipDealer")};
    if (roleText == QStringLiteral("equipment"))
        return {QStringLiteral("scripts\\vendors\\li_equipdealer_fidget.thn"), QStringLiteral("Equipment")};
    if (roleText == QStringLiteral("bartender"))
        return {QStringLiteral("scripts\\vendors\\li_host_fidget.thn"), QStringLiteral("bartender")};
    if (roleText == QStringLiteral("newsvendor"))
        return {QStringLiteral("scripts\\vendors\\li_bartender_fidget.thn"), QStringLiteral("NewsVendor")};
    if (roleText == QStringLiteral("barfly"))
        return {QStringLiteral("scripts\\vendors\\li_bartender_fidget.thn"), QStringLiteral("BarFly")};
    return {QStringLiteral("scripts\\vendors\\li_commtrader_fidget.thn"), QStringLiteral("trader")};
}

BaseRoomNpcState npcFromFixtureEntry(const QString &fixtureValue, const QString &roomName)
{
    BaseRoomNpcState npc;
    const QStringList parts = fixtureValue.split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (!parts.isEmpty())
        npc.nickname = parts.first().trimmed();
    if (parts.size() >= 4)
        npc.role = normalizedRoleForRoom(parts.at(3).trimmed(), roomName);
    else
        npc.role = normalizedRoleForRoom(QString(), roomName);
    return npc;
}

BaseRoomNpcState templateNpcFromEntries(const QVector<QPair<QString, QString>> &entries,
                                        const QString &roomName,
                                        const QString &reputation,
                                        const QString &gameRoot)
{
    BaseRoomNpcState npc;
    npc.nickname = rawEntryValue(entries, QStringLiteral("nickname"));
    npc.nameText = resolvedDisplayName(gameRoot, rawEntryIntValue(entries, QStringLiteral("individual_name")));
    if (npc.nameText.isEmpty())
        npc.nameText = npc.nickname;
    npc.reputation = reputation;
    npc.affiliation = rawEntryValue(entries, QStringLiteral("affiliation"));
    if (npc.affiliation.isEmpty())
        npc.affiliation = reputation;
    npc.role = normalizedRoleForRoom(QString(), roomName);
    npc.body = rawEntryValue(entries, QStringLiteral("body"));
    npc.head = rawEntryValue(entries, QStringLiteral("head"));
    npc.leftHand = rawEntryValue(entries, QStringLiteral("lefthand"));
    npc.rightHand = rawEntryValue(entries, QStringLiteral("righthand"));
    npc.templateEntries = entries;
    return npc;
}

QString makeGeneratedNpcNickname(const QString &baseNickname,
                                 const QString &roomName,
                                 int index)
{
    return QStringLiteral("%1_%2_npc_%3")
        .arg(safeNickPart(baseNickname),
             safeNickPart(roomName),
             QStringLiteral("%1").arg(index, 2, 10, QLatin1Char('0')));
}

int roomDensity(const QString &roomName)
{
    const QString room = normalizeKey(roomName);
    if (room == QStringLiteral("bar"))
        return 7;
    if (room == QStringLiteral("shipdealer") || room == QStringLiteral("equipment"))
        return 2;
    return 3;
}

QStringList enabledRoomNames(const BaseEditState &state)
{
    QStringList rooms;
    for (const BaseRoomState &room : state.rooms) {
        if (room.enabled && !room.roomName.trimmed().isEmpty())
            rooms.append(canonicalRoomName(room.roomName));
    }
    rooms.removeDuplicates();
    return rooms;
}

QString chooseStartRoom(const QStringList &rooms, const QString &preferred)
{
    const QString target = preferred.trimmed();
    if (!target.isEmpty()) {
        for (const QString &room : rooms) {
            if (room.compare(target, Qt::CaseInsensitive) == 0)
                return room;
        }
    }
    for (const QString &candidate : {QStringLiteral("Deck"), QStringLiteral("Cityscape")}) {
        for (const QString &room : rooms) {
            if (room.compare(candidate, Qt::CaseInsensitive) == 0)
                return room;
        }
    }
    return rooms.value(0);
}

QVector<BaseRoomState> buildDefaultRoomsForArchetype(const QString &archetype)
{
    const bool planetLike = archetype.trimmed().toLower().contains(QStringLiteral("planet"));
    QVector<BaseRoomState> rooms;
    auto addRoom = [&rooms](const QString &name, bool enabled, const QString &scene) {
        BaseRoomState room;
        room.roomName = name;
        room.enabled = enabled;
        room.scenePath = scene;
        rooms.append(room);
    };

    if (planetLike) {
        addRoom(QStringLiteral("Cityscape"), true,
                QStringLiteral("Scripts\\Bases\\Li_01_cityscape_ambi_day_01.thn"));
    } else {
        addRoom(QStringLiteral("Deck"), true,
                QStringLiteral("Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn"));
    }
    addRoom(QStringLiteral("Bar"), true,
            QStringLiteral("Scripts\\Bases\\Li_09_bar_ambi_int_s020x.thn"));
    addRoom(QStringLiteral("Trader"), true,
            QStringLiteral("Scripts\\Bases\\Li_01_Trader_ambi_int_01.thn"));
    addRoom(QStringLiteral("Equipment"), false,
            QStringLiteral("Scripts\\Bases\\Li_01_equipment_ambi_int_01.thn"));
    addRoom(QStringLiteral("ShipDealer"), false,
            QStringLiteral("Scripts\\Bases\\Li_01_shipdealer_ambi_int_01.thn"));
    return rooms;
}

bool loadBaseFileState(BaseEditState *state,
                       const QString &gameRoot,
                       const QHash<QString, QString> &textOverrides,
                       QString *errorMessage)
{
    if (!state)
        return false;
    if (state->baseNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Template-Base-Nickname darf nicht leer sein.");
        return false;
    }

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    state->universeIniAbsolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    state->mbaseAbsolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS/mbases.ini"));

    const QString universeText = textForPath(state->universeIniAbsolutePath, textOverrides);
    const IniDocument universeDoc = IniParser::parseText(universeText);
    for (const IniSection &section : universeDoc) {
        if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
            continue;
        if (normalizeKey(section.value(QStringLiteral("nickname"))) != normalizeKey(state->baseNickname))
            continue;
        state->bgcsBaseRunBy = section.value(QStringLiteral("BGCS_base_run_by")).trimmed();
        const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
        if (!relativeFile.isEmpty()) {
            state->universeBaseFileRelativePath = relativeFile;
            state->baseIniAbsolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeFile);
        }
        break;
    }

    if (state->baseIniAbsolutePath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("Die Template-Base %1 wurde in universe.ini nicht gefunden.")
                                .arg(state->baseNickname);
        }
        return false;
    }

    state->roomsDirectoryAbsolutePath = QFileInfo(state->baseIniAbsolutePath).absolutePath() + QStringLiteral("/ROOMS");

    const QString baseText = textForPath(state->baseIniAbsolutePath, textOverrides);
    const IniDocument baseDoc = IniParser::parseText(baseText);
    QVector<QPair<QString, QString>> roomFiles;
    for (const IniSection &section : baseDoc) {
        if (section.name.compare(QStringLiteral("BaseInfo"), Qt::CaseInsensitive) == 0) {
            state->startRoom = section.value(QStringLiteral("start_room")).trimmed();
            bool ok = false;
            const double price = section.value(QStringLiteral("price_variance")).trimmed().toDouble(&ok);
            if (ok)
                state->priceVariance = price;
        } else if (section.name.compare(QStringLiteral("Room"), Qt::CaseInsensitive) == 0) {
            const QString name = canonicalRoomName(section.value(QStringLiteral("nickname")).trimmed());
            const QString file = section.value(QStringLiteral("file")).trimmed();
            if (!name.isEmpty() && !file.isEmpty())
                roomFiles.append({name, file});
        }
    }

    QVector<BaseRoomState> rooms;
    QHash<QString, QVector<BaseRoomNpcState>> roomNpcs;
    const QString mbasesText = textForPath(state->mbaseAbsolutePath, textOverrides);
    const IniDocument mbasesDoc = IniParser::parseText(mbasesText);
    int matchingMbaseIndex = -1;
    for (int index = 0; index < mbasesDoc.size(); ++index) {
        if (mbasesDoc.at(index).name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) != 0)
            continue;
        if (normalizeKey(mbasesDoc.at(index).value(QStringLiteral("nickname"))) == normalizeKey(state->baseNickname)) {
            matchingMbaseIndex = index;
            break;
        }
    }
    if (matchingMbaseIndex >= 0) {
        const auto range = mbaseBlockRange(mbasesDoc, matchingMbaseIndex);
        const QString localFaction = mbasesDoc.at(matchingMbaseIndex).value(QStringLiteral("local_faction")).trimmed();
        QHash<QString, QString> npcToFaction;
        QHash<QString, QPair<QString, QString>> fixtureMap;
        for (int index = range.first + 1; index < range.second; ++index) {
            const IniSection &section = mbasesDoc.at(index);
            if (section.name.compare(QStringLiteral("BaseFaction"), Qt::CaseInsensitive) == 0) {
                const QString faction = section.value(QStringLiteral("faction")).trimmed();
                for (const auto &entry : section.entries) {
                    if (entry.first.compare(QStringLiteral("npc"), Qt::CaseInsensitive) == 0) {
                        const QString npcNickname = entry.second.trimmed();
                        if (!npcNickname.isEmpty() && !npcToFaction.contains(normalizeKey(npcNickname)))
                            npcToFaction.insert(normalizeKey(npcNickname), faction);
                    }
                }
                continue;
            }
            if (section.name.compare(QStringLiteral("MRoom"), Qt::CaseInsensitive) != 0)
                continue;
            const QString roomName = canonicalRoomName(section.value(QStringLiteral("nickname")).trimmed());
            for (const auto &entry : section.entries) {
                if (entry.first.compare(QStringLiteral("fixture"), Qt::CaseInsensitive) == 0) {
                    const BaseRoomNpcState npc = npcFromFixtureEntry(entry.second, roomName);
                    if (!npc.nickname.trimmed().isEmpty()) {
                        const QString npcKey = normalizeKey(npc.nickname);
                        fixtureMap.insert(npcKey, {roomKey(roomName), npc.role});
                        if (!roomNpcs.contains(normalizeKey(roomName)))
                            roomNpcs.insert(normalizeKey(roomName), {});
                    }
                }
            }
        }

        for (int index = range.first + 1; index < range.second; ++index) {
            const IniSection &section = mbasesDoc.at(index);
            if (section.name.compare(QStringLiteral("GF_NPC"), Qt::CaseInsensitive) != 0)
                continue;
            const QString npcNickname = section.value(QStringLiteral("nickname")).trimmed();
            if (npcNickname.isEmpty())
                continue;
            const QString npcKey = normalizeKey(npcNickname);
            const auto fixture = fixtureMap.value(npcKey);
            if (!fixtureMap.isEmpty() && fixture.first.isEmpty())
                continue;

            const QString roomName = canonicalRoomName(fixture.first.isEmpty()
                                                           ? roomKey(section.value(QStringLiteral("room")).trimmed())
                                                           : fixture.first);
            if (roomName.isEmpty())
                continue;

            BaseRoomNpcState npc = templateNpcFromEntries(section.entries,
                                                          roomName,
                                                          npcToFaction.value(npcKey, localFaction),
                                                          gameRoot);
            if (!fixture.second.isEmpty())
                npc.role = fixture.second;
            if (!npc.nickname.isEmpty())
                roomNpcs[normalizeKey(roomName)].append(npc);
        }

        for (auto it = fixtureMap.constBegin(); it != fixtureMap.constEnd(); ++it) {
            const QString roomName = canonicalRoomName(it.value().first);
            if (roomName.isEmpty())
                continue;
            const QVector<BaseRoomNpcState> existing = roomNpcs.value(normalizeKey(roomName));
            const bool alreadyPresent = std::any_of(existing.begin(), existing.end(), [&](const BaseRoomNpcState &npc) {
                return normalizeKey(npc.nickname) == it.key();
            });
            if (alreadyPresent)
                continue;
            BaseRoomNpcState npc;
            npc.nickname = it.key();
            npc.nameText = it.key();
            npc.reputation = localFaction;
            npc.affiliation = localFaction;
            npc.role = it.value().second;
            roomNpcs[normalizeKey(roomName)].append(npc);
        }
    }

    if (roomFiles.isEmpty()) {
        rooms = buildDefaultRoomsForArchetype(state->archetype);
    } else {
        for (const auto &roomFile : roomFiles) {
            BaseRoomState room;
            room.roomName = canonicalRoomName(roomFile.first);
            room.enabled = true;
            const QString absoluteRoomPath = flatlas::core::PathUtils::ciResolvePath(dataDir, roomFile.second);
            const QString roomText = textForPath(absoluteRoomPath, textOverrides);
            room.templateContent = roomText;
            room.scenePath = extractScenePath(roomText);
            if (room.scenePath.isEmpty())
                room.scenePath = extractScenePath(generateRoomIniText(room.roomName, QStringList{room.roomName}, room.roomName));
            room.npcs = roomNpcs.value(normalizeKey(room.roomName));
            rooms.append(room);
        }
    }

    if (rooms.isEmpty())
        rooms = buildDefaultRoomsForArchetype(state->archetype);
    state->rooms = rooms;
    if (state->rotate.trimmed().isEmpty())
        state->rotate = QStringLiteral("0, 0, 0");
    if (state->startRoom.isEmpty())
        state->startRoom = chooseStartRoom(enabledRoomNames(*state), QStringLiteral("Deck"));
    return true;
}

int nextObjectNumber(const SystemDocument &document, const QString &systemNickname)
{
    const QString prefix = systemNickname.trimmed().toLower() + QLatin1Char('_');
    int maxNumber = 0;
    for (const auto &object : document.objects()) {
        if (!object)
            continue;
        const QString nickname = object->nickname().trimmed().toLower();
        if (!nickname.startsWith(prefix))
            continue;
        const QString suffix = nickname.mid(prefix.size());
        const int underscore = suffix.indexOf(QLatin1Char('_'));
        const QString numberText = underscore >= 0 ? suffix.left(underscore) : suffix;
        bool ok = false;
        const int number = numberText.toInt(&ok);
        if (ok)
            maxNumber = std::max(maxNumber, number);
    }
    return maxNumber + 1;
}

QString relativeToDataDir(const QString &dataDir, const QString &absolutePath)
{
    return QDir(dataDir).relativeFilePath(absolutePath).replace(QLatin1Char('/'), QLatin1Char('\\'));
}

QString normalizeGeneratedText(QString content)
{
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList rawLines = content.split(QLatin1Char('\n'));
    QStringList cleaned;
    bool blankPending = false;
    for (const QString &rawLine : rawLines) {
        const QString line = rawLine.trimmed().isEmpty() ? QString() : rawLine.trimmed().isEmpty() ? QString() : rawLine;
        const QString rightTrimmed = rawLine;
        const QString finalLine = rightTrimmed.trimmed().isEmpty() ? QString() : QString(rightTrimmed).replace(QRegularExpression(QStringLiteral("[ \t]+$")), QString());
        if (finalLine.trimmed().isEmpty()) {
            if (!cleaned.isEmpty())
                blankPending = true;
            continue;
        }
        if (blankPending && !cleaned.isEmpty()) {
            cleaned.append(QString());
            blankPending = false;
        }
        cleaned.append(finalLine);
    }
    while (!cleaned.isEmpty() && cleaned.last().trimmed().isEmpty())
        cleaned.removeLast();
    return cleaned.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QVector<QPair<QString, QString>> navHotspots(const QStringList &rooms, const QString &startRoom)
{
    QVector<QPair<QString, QString>> result;
    result.append({QStringLiteral("IDS_HOTSPOT_EXIT"), startRoom});
    for (const QString &room : rooms) {
        if (room.compare(startRoom, Qt::CaseInsensitive) == 0)
            continue;
        const QString key = normalizeKey(room);
        QString hotspot = QStringLiteral("IDS_HOTSPOT_%1").arg(room.toUpper());
        if (key == QStringLiteral("bar"))
            hotspot = QStringLiteral("IDS_HOTSPOT_BAR");
        else if (key == QStringLiteral("trader"))
            hotspot = QStringLiteral("IDS_HOTSPOT_COMMODITYTRADER_ROOM");
        else if (key == QStringLiteral("equipment"))
            hotspot = QStringLiteral("IDS_HOTSPOT_EQUIPMENTDEALER_ROOM");
        else if (key == QStringLiteral("shipdealer"))
            hotspot = QStringLiteral("IDS_HOTSPOT_SHIPDEALER_ROOM");
        else if (key == QStringLiteral("cityscape"))
            hotspot = QStringLiteral("IDS_HOTSPOT_CITYSCAPE");
        else if (key == QStringLiteral("deck"))
            hotspot = QStringLiteral("IDS_HOTSPOT_DECK");
        result.append({hotspot, room});
    }
    return result;
}

QString canonicalNavRoom(const QString &roomName)
{
    const QString canonical = canonicalRoomName(roomName);
    const QString key = normalizeKey(canonical);
    if (key == QStringLiteral("deck")
        || key == QStringLiteral("bar")
        || key == QStringLiteral("trader")
        || key == QStringLiteral("equipment")
        || key == QStringLiteral("shipdealer")
        || key == QStringLiteral("cityscape")) {
        return canonical;
    }
    return {};
}

QString hotspotNameForRoom(const QString &roomName)
{
    const QString key = normalizeKey(roomName);
    if (key == QStringLiteral("bar"))
        return QStringLiteral("IDS_HOTSPOT_BAR");
    if (key == QStringLiteral("trader"))
        return QStringLiteral("IDS_HOTSPOT_COMMODITYTRADER_ROOM");
    if (key == QStringLiteral("equipment"))
        return QStringLiteral("IDS_HOTSPOT_EQUIPMENTDEALER_ROOM");
    if (key == QStringLiteral("shipdealer"))
        return QStringLiteral("IDS_HOTSPOT_SHIPDEALER_ROOM");
    if (key == QStringLiteral("cityscape"))
        return QStringLiteral("IDS_HOTSPOT_CITYSCAPE");
    if (key == QStringLiteral("deck"))
        return QStringLiteral("IDS_HOTSPOT_DECK");
    return QStringLiteral("IDS_HOTSPOT_EXIT");
}

QStringList orderedNavigationTargets(const QString &roomName,
                                     const QStringList &allRooms,
                                     const QString &startRoom,
                                     const QHash<QString, QStringList> &existingBlocksByTarget)
{
    QStringList targets;
    auto appendTarget = [&targets](const QString &target) {
        const QString canonical = canonicalNavRoom(target);
        if (!canonical.isEmpty() && !targets.contains(canonical, Qt::CaseInsensitive))
            targets.append(canonical);
    };

    const QString canonicalRoom = canonicalNavRoom(roomName);
    const QString launchTarget = normalizeKey(canonicalRoom) == QStringLiteral("deck")
            || normalizeKey(canonicalRoom) == QStringLiteral("cityscape")
        ? canonicalRoom
        : canonicalNavRoom(startRoom);
    appendTarget(launchTarget);
    appendTarget(QStringLiteral("Bar"));
    appendTarget(QStringLiteral("Trader"));
    appendTarget(QStringLiteral("Equipment"));
    appendTarget(QStringLiteral("ShipDealer"));
    for (const QString &room : allRooms)
        appendTarget(room);
    for (auto it = existingBlocksByTarget.constBegin(); it != existingBlocksByTarget.constEnd(); ++it)
        appendTarget(it.key());
    return targets;
}

QString normalizeRoomNavigation(const QString &content,
                                const QString &roomName,
                                const QStringList &allRooms,
                                const QString &startRoom)
{
    QString normalizedContent = content;
    normalizedContent.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalizedContent.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList lines = normalizedContent.split(QLatin1Char('\n'));
    QStringList result;
    int insertIndex = -1;
    QHash<QString, QStringList> existingBlocksByTarget;
    int index = 0;
    while (index < lines.size()) {
        if (lines.at(index).trimmed().compare(QStringLiteral("[Hotspot]"), Qt::CaseInsensitive) == 0) {
            QStringList block;
            block.append(lines.at(index++));
            while (index < lines.size() && !lines.at(index).trimmed().startsWith(QLatin1Char('[')))
                block.append(lines.at(index++));

            QString behavior;
            QString roomSwitch;
            QString virtualRoom;
            for (const QString &line : block) {
                const QString trimmed = line.trimmed();
                const int eq = trimmed.indexOf(QLatin1Char('='));
                if (eq < 0)
                    continue;
                const QString key = trimmed.left(eq).trimmed().toLower();
                const QString value = trimmed.mid(eq + 1).trimmed();
                if (key == QStringLiteral("behavior"))
                    behavior = value;
                else if (key == QStringLiteral("room_switch"))
                    roomSwitch = value;
                else if (key == QStringLiteral("set_virtual_room") || key == QStringLiteral("virtual_room"))
                    virtualRoom = value;
            }

            const bool navigationBehavior = behavior.compare(QStringLiteral("ExitDoor"), Qt::CaseInsensitive) == 0
                || behavior.compare(QStringLiteral("VirtualRoom"), Qt::CaseInsensitive) == 0;
            const QString navTarget = canonicalNavRoom(!virtualRoom.trimmed().isEmpty() ? virtualRoom : roomSwitch);
            if (navigationBehavior && !navTarget.isEmpty()) {
                insertIndex = result.size();
                if (!existingBlocksByTarget.contains(navTarget))
                    existingBlocksByTarget.insert(navTarget, block);
                continue;
            }
            result.append(block);
            continue;
        }
        result.append(lines.at(index++));
    }

    if (insertIndex < 0) {
        while (!result.isEmpty() && result.last().trimmed().isEmpty())
            result.removeLast();
        result.append(QString());
        insertIndex = result.size();
    }

    QStringList navLines;
    const QStringList orderedTargets = orderedNavigationTargets(roomName, allRooms, startRoom, existingBlocksByTarget);
    const QString canonicalRoom = canonicalNavRoom(roomName);
    const QString launchTarget = normalizeKey(canonicalRoom) == QStringLiteral("deck")
            || normalizeKey(canonicalRoom) == QStringLiteral("cityscape")
        ? canonicalRoom
        : canonicalNavRoom(startRoom);
    for (const QString &target : orderedTargets) {
        if (existingBlocksByTarget.contains(target)) {
            navLines += existingBlocksByTarget.value(target);
            navLines << QString();
            continue;
        }

        const QString hotspotName = target.compare(launchTarget, Qt::CaseInsensitive) == 0
                && target.compare(canonicalRoom, Qt::CaseInsensitive) != 0
            ? QStringLiteral("IDS_HOTSPOT_EXIT")
            : hotspotNameForRoom(target);
        navLines << QStringLiteral("[Hotspot]")
                 << QStringLiteral("name = %1").arg(hotspotName)
                 << QStringLiteral("behavior = ExitDoor")
                 << QStringLiteral("room_switch = %1").arg(target)
                 << QString();
    }

    for (int navIndex = navLines.size() - 1; navIndex >= 0; --navIndex)
        result.insert(insertIndex, navLines.at(navIndex));
    return normalizeGeneratedText(result.join(QLatin1Char('\n')));
}

QString adaptTemplateRoom(QString content, const QStringList &rooms)
{
    const QStringList lines = content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
                                  .replace(QLatin1Char('\r'), QLatin1Char('\n'))
                                  .split(QLatin1Char('\n'));
    QSet<QString> roomsLower;
    for (const QString &room : rooms)
        roomsLower.insert(room.toLower());
    QStringList output;

    int index = 0;
    while (index < lines.size()) {
        const QString line = lines.at(index);
        if (line.trimmed().compare(QStringLiteral("[Hotspot]"), Qt::CaseInsensitive) != 0) {
            output.append(line);
            ++index;
            continue;
        }

        QStringList block;
        block.append(line);
        ++index;
        while (index < lines.size()) {
            const QString nextLine = lines.at(index);
            const QString trimmed = nextLine.trimmed();
            if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']')))
                break;
            block.append(nextLine);
            ++index;
        }

        bool keep = true;
        QString behavior;
        bool hasVirtualTarget = false;
        QString directTarget;
        for (int blockIndex = 1; blockIndex < block.size(); ++blockIndex) {
            const QString trimmed = block.at(blockIndex).trimmed();
            const int eq = trimmed.indexOf(QLatin1Char('='));
            if (eq < 0)
                continue;
            const QString key = trimmed.left(eq).trimmed().toLower();
            const QString value = trimmed.mid(eq + 1).trimmed();
            if (key == QStringLiteral("behavior")) {
                behavior = value.toLower();
            } else if (key == QStringLiteral("room_switch")) {
                if (!value.isEmpty() && !roomsLower.contains(value.toLower()))
                    keep = false;
            } else if (key == QStringLiteral("virtual_room") || key == QStringLiteral("set_virtual_room")) {
                hasVirtualTarget = hasVirtualTarget || !value.isEmpty();
                if (roomsLower.contains(value.toLower()))
                    directTarget = value;
            }
        }

        if (!keep && (behavior == QStringLiteral("virtualroom") || hasVirtualTarget))
            keep = true;
        if (!keep)
            continue;

        if (!directTarget.isEmpty()) {
            QStringList rewritten;
            bool roomSwitchWritten = false;
            for (const QString &blockLine : block) {
                const QString trimmed = blockLine.trimmed();
                const int eq = trimmed.indexOf(QLatin1Char('='));
                if (eq < 0) {
                    rewritten.append(blockLine);
                    continue;
                }
                const QString key = trimmed.left(eq).trimmed().toLower();
                if (key == QStringLiteral("behavior")) {
                    rewritten.append(QStringLiteral("behavior = ExitDoor"));
                } else if (key == QStringLiteral("room_switch")) {
                    rewritten.append(QStringLiteral("room_switch = %1").arg(directTarget));
                    roomSwitchWritten = true;
                } else if (key == QStringLiteral("virtual_room") || key == QStringLiteral("set_virtual_room")) {
                    continue;
                } else {
                    rewritten.append(blockLine);
                }
            }
            if (!roomSwitchWritten) {
                int insertAt = rewritten.size();
                for (int rewriteIndex = 0; rewriteIndex < rewritten.size(); ++rewriteIndex) {
                    if (rewritten.at(rewriteIndex).trimmed().toLower().startsWith(QStringLiteral("behavior ="))) {
                        insertAt = rewriteIndex + 1;
                        break;
                    }
                }
                rewritten.insert(insertAt, QStringLiteral("room_switch = %1").arg(directTarget));
            }
            output += rewritten;
            continue;
        }

        output += block;
    }

    return normalizeGeneratedText(output.join(QLatin1Char('\n')));
}

QVector<QPair<QString, QString>> buildOrderedObjectEntries(const BaseEditState &state,
                                                           const QString &posValue,
                                                           const QString &rotateValue,
                                                           const QVector<QPair<QString, QString>> &existingEntries)
{
    QVector<QPair<QString, QString>> ordered = {
        {QStringLiteral("nickname"), state.objectNickname.trimmed()},
        {QStringLiteral("pos"), posValue.trimmed()},
        {QStringLiteral("rotate"), normalizeRotateValue(rotateValue)},
        {QStringLiteral("ids_name"), state.idsName > 0 ? QString::number(state.idsName) : QString()},
        {QStringLiteral("ids_info"), state.idsInfo > 0 ? QString::number(state.idsInfo) : QString()},
        {QStringLiteral("Archetype"), state.archetype.trimmed()},
        {QStringLiteral("dock_with"), state.baseNickname.trimmed()},
        {QStringLiteral("base"), state.baseNickname.trimmed()},
        {QStringLiteral("behavior"), QStringLiteral("NOTHING")},
        {QStringLiteral("difficulty_level"), QStringLiteral("1")},
        {QStringLiteral("loadout"), state.loadout.trimmed()},
        {QStringLiteral("pilot"), state.pilot.trimmed()},
        {QStringLiteral("reputation"), nicknameFromDisplayValue(state.reputation)},
        {QStringLiteral("voice"), state.voice.trimmed()},
    };

    const QString head = state.head.trimmed();
    const QString body = state.body.trimmed();
    const QString costume = head.isEmpty() ? body : (body.isEmpty() ? head : QStringLiteral("%1, %2").arg(head, body));
    ordered.append({QStringLiteral("space_costume"), costume});

    QSet<QString> controlledKeys;
    for (const auto &entry : ordered)
        controlledKeys.insert(normalizeKey(entry.first));
    controlledKeys.insert(QStringLiteral("archetype"));

    QVector<QPair<QString, QString>> result;
    for (const auto &entry : ordered) {
        if (!entry.second.trimmed().isEmpty())
            result.append(entry);
    }
    for (const auto &entry : existingEntries) {
        if (!controlledKeys.contains(normalizeKey(entry.first)) && !entry.second.trimmed().isEmpty())
            result.append({entry.first, entry.second.trimmed()});
    }
    return result;
}

QString overrideRoomScene(QString content, const QString &scenePath)
{
    QStringList lines = content.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n')).split(QLatin1Char('\n'));
    bool inRoomInfo = false;
    bool updated = false;
    for (int index = 0; index < lines.size(); ++index) {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.startsWith(QLatin1Char('[')))
            inRoomInfo = trimmed.compare(QStringLiteral("[Room_Info]"), Qt::CaseInsensitive) == 0;
        if (!inRoomInfo)
            continue;
        const int eq = trimmed.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = trimmed.left(eq).trimmed();
        if (key.compare(QStringLiteral("scene"), Qt::CaseInsensitive) != 0)
            continue;
        lines[index] = QStringLiteral("scene = all, ambient, %1").arg(scenePath.trimmed());
        updated = true;
        break;
    }
    if (!updated) {
        int insertIndex = 0;
        for (int index = 0; index < lines.size(); ++index) {
            const QString trimmed = lines.at(index).trimmed();
            if (trimmed.compare(QStringLiteral("[Room_Info]"), Qt::CaseInsensitive) == 0) {
                insertIndex = index + 1;
                break;
            }
        }
        lines.insert(insertIndex, QStringLiteral("scene = all, ambient, %1").arg(scenePath.trimmed()));
    }
    return normalizeGeneratedText(lines.join(QLatin1Char('\n')));
}

QString extractScenePath(QString content)
{
    const QStringList lines = content.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n')).split(QLatin1Char('\n'));
    bool inRoomInfo = false;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')))
            inRoomInfo = trimmed.compare(QStringLiteral("[Room_Info]"), Qt::CaseInsensitive) == 0;
        if (!inRoomInfo)
            continue;
        const int eq = trimmed.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = trimmed.left(eq).trimmed();
        if (key.compare(QStringLiteral("scene"), Qt::CaseInsensitive) != 0)
            continue;
        const QStringList parts = trimmed.mid(eq + 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
        return parts.isEmpty() ? QString() : parts.last().trimmed();
    }
    return {};
}

QString generateRoomIniText(const QString &roomName, const QStringList &allRooms, const QString &startRoom)
{
    const QString room = normalizeKey(roomName);
    QStringList lines;

    if (room == QStringLiteral("deck")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_08_Deck_hardpoint_01.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn")
              << QStringLiteral("animation = Sc_loop");
    } else if (room == QStringLiteral("bar")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_09_bar_hardpoint_s020x.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_09_bar_ambi_int_s020x.thn");
    } else if (room == QStringLiteral("trader")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_01_Trader_hardpoint_01.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_01_Trader_ambi_int_01.thn");
    } else if (room == QStringLiteral("equipment")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_01_equipment_hardpoint_01.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_01_equipment_ambi_int_01.thn");
    } else if (room == QStringLiteral("shipdealer")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_01_shipdealer_hardpoint_01.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_01_shipdealer_ambi_int_01.thn");
    } else if (room == QStringLiteral("cityscape")) {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_01_cityscape_hardpoint_01.thn")
              << QStringLiteral("animation = Sc_loop")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_01_cityscape_ambi_day_01.thn");
    } else {
        lines << QStringLiteral("[Room_Info]")
              << QStringLiteral("set_script = Scripts\\Bases\\Li_08_Deck_hardpoint_01.thn")
              << QStringLiteral("scene = all, ambient, Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn");
    }

    lines << QString();

    if (room == QStringLiteral("trader"))
        lines << QStringLiteral("[Spiels]") << QStringLiteral("CommodityDealer = manhattan_commodity_spiel") << QString();
    else if (room == QStringLiteral("equipment"))
        lines << QStringLiteral("[Spiels]") << QStringLiteral("EquipmentDealer = manhattan_equipment_spiel") << QString();
    else if (room == QStringLiteral("shipdealer"))
        lines << QStringLiteral("[Spiels]") << QStringLiteral("ShipDealer = manhattan_ship_spiel") << QString();

    if (room == QStringLiteral("equipment"))
        lines << QStringLiteral("[Room_Sound]") << QStringLiteral("ambient = ambience_equip_ground_larger") << QString();
    else if (room == QStringLiteral("shipdealer"))
        lines << QStringLiteral("[Room_Sound]") << QStringLiteral("ambient = ambience_shipbuy") << QString();
    else if (room == QStringLiteral("trader"))
        lines << QStringLiteral("[Room_Sound]") << QStringLiteral("ambient = ambience_comm") << QString();
    else
        lines << QStringLiteral("[Room_Sound]") << QStringLiteral("ambient = ambience_deck_space_smaller") << QString();

    lines << QStringLiteral("[Camera]") << QStringLiteral("name = Camera_0") << QString();

    if (room == QStringLiteral("bar") || room == QStringLiteral("trader")
        || room == QStringLiteral("equipment") || room == QStringLiteral("shipdealer")) {
        lines << QStringLiteral("[CharacterPlacement]") << QStringLiteral("name = Zg/PC/Player/01/A/Stand") << QString();
    }
    if (room == QStringLiteral("deck") || room == QStringLiteral("cityscape") || room == QStringLiteral("equipment"))
        lines << QStringLiteral("[PlayerShipPlacement]") << QStringLiteral("name = X/Shipcentre/01") << QString();
    if (room == QStringLiteral("shipdealer")) {
        lines << QStringLiteral("[ForSaleShipPlacement]") << QStringLiteral("name = X/Shipcentre/01") << QString()
              << QStringLiteral("[ForSaleShipPlacement]") << QStringLiteral("name = X/Shipcentre/02") << QString()
              << QStringLiteral("[ForSaleShipPlacement]") << QStringLiteral("name = X/Shipcentre/03") << QString();
    }

    for (const auto &entry : navHotspots(allRooms, startRoom)) {
        lines << QStringLiteral("[Hotspot]")
              << QStringLiteral("name = %1").arg(entry.first)
              << QStringLiteral("behavior = ExitDoor")
              << QStringLiteral("room_switch = %1").arg(entry.second)
              << QString();
    }

    return normalizeGeneratedText(lines.join(QLatin1Char('\n')));
}

QString defaultRoomFilePathForBase(const BaseEditState &state, const QString &roomName)
{
    const QString roomFileName = QStringLiteral("%1_%2.ini")
        .arg(generatedBaseStem(state.baseNickname), normalizeKey(roomName));

    const QString relativeBaseIniPath = QDir::fromNativeSeparators(state.universeBaseFileRelativePath.trimmed());
    if (!relativeBaseIniPath.isEmpty()) {
        const QString baseDir = QFileInfo(relativeBaseIniPath).path();
        if (!baseDir.trimmed().isEmpty()) {
            return QDir(baseDir)
                .filePath(QStringLiteral("ROOMS/%1").arg(roomFileName))
                .replace(QLatin1Char('/'), QLatin1Char('\\'));
        }
    }

    return QStringLiteral("Universe\\Systems\\%1\\Bases\\ROOMS\\%2")
        .arg(state.systemNickname.trimmed(), roomFileName);
}

QString buildUpdatedBaseIniText(const QString &existingText, const BaseEditState &state)
{
    const QStringList rooms = enabledRoomNames(state);
    const IniDocument existingDoc = existingText.trimmed().isEmpty()
        ? IniDocument{}
        : IniParser::parseText(existingText);

    IniSection baseInfoSection;
    bool hasBaseInfo = false;
    int baseInfoInsertIndex = -1;
    QHash<QString, QString> existingRoomFiles;
    IniDocument updatedDoc;

    for (const IniSection &section : existingDoc) {
        if (section.name.compare(QStringLiteral("BaseInfo"), Qt::CaseInsensitive) == 0) {
            if (!hasBaseInfo) {
                baseInfoSection = section;
                hasBaseInfo = true;
                baseInfoInsertIndex = updatedDoc.size();
            }
            continue;
        }

        if (section.name.compare(QStringLiteral("Room"), Qt::CaseInsensitive) == 0) {
            const QString roomName = canonicalRoomName(section.value(QStringLiteral("nickname")).trimmed());
            const QString relativeFile = section.value(QStringLiteral("file")).trimmed();
            if (!roomName.isEmpty() && !relativeFile.isEmpty())
                existingRoomFiles.insert(normalizeKey(roomName), relativeFile);
            continue;
        }

        updatedDoc.append(section);
    }

    if (!hasBaseInfo) {
        baseInfoSection.name = QStringLiteral("BaseInfo");
        baseInfoInsertIndex = 0;
    }
    setOrRemoveRawEntry(&baseInfoSection.entries, QStringLiteral("nickname"), state.baseNickname.trimmed());
    setOrRemoveRawEntry(&baseInfoSection.entries, QStringLiteral("start_room"), chooseStartRoom(rooms, state.startRoom));
    setOrRemoveRawEntry(&baseInfoSection.entries,
                        QStringLiteral("price_variance"),
                        QStringLiteral("%1").arg(state.priceVariance, 0, 'f', 2));

    baseInfoInsertIndex = std::clamp(baseInfoInsertIndex, 0, static_cast<int>(updatedDoc.size()));
    updatedDoc.insert(baseInfoInsertIndex, baseInfoSection);

    int roomInsertIndex = baseInfoInsertIndex + 1;
    for (const QString &room : rooms) {
        IniSection roomSection;
        roomSection.name = QStringLiteral("Room");
        roomSection.entries = {
            {QStringLiteral("nickname"), room},
            {QStringLiteral("file"), existingRoomFiles.value(normalizeKey(room), defaultRoomFilePathForBase(state, room))},
        };
        updatedDoc.insert(roomInsertIndex++, roomSection);
    }

    return normalizeGeneratedText(IniParser::serialize(updatedDoc));
}

QString resolvedDisplayName(const QString &gameRoot, int idsName)
{
    if (idsName <= 0)
        return {};
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    if (exeDir.isEmpty())
        return {};
    IdsStringTable ids;
    ids.loadFromFreelancerDir(exeDir);
    return ids.getString(idsName).trimmed();
}

QString resolvedInfocardXml(const QString &gameRoot, int idsInfo)
{
    if (idsInfo <= 0)
        return {};
    const IdsDataset dataset = IdsDataService::loadFromGameRoot(gameRoot);
    for (const auto &entry : dataset.entries) {
        if (entry.globalId == idsInfo && entry.hasHtmlValue)
            return entry.htmlValue.trimmed();
    }
    return {};
}

QString objectBaseNickname(const SolarObject &object)
{
    if (!object.base().trimmed().isEmpty())
        return object.base().trimmed();
    if (!object.dockWith().trimmed().isEmpty())
        return object.dockWith().trimmed();
    const QString rawBase = rawEntryValue(object.rawEntries(), QStringLiteral("base"));
    if (!rawBase.isEmpty())
        return rawBase;
    return rawEntryValue(object.rawEntries(), QStringLiteral("dock_with"));
}

bool updateUniverseBaseSection(IniDocument *doc,
                               const BaseEditState &state,
                               const QString &dataDir)
{
    if (!doc)
        return false;
    const QString targetNick = normalizeKey(state.baseNickname);
    IniSection updated;
    updated.name = QStringLiteral("Base");
    updated.entries = {
        {QStringLiteral("nickname"), state.baseNickname.trimmed()},
        {QStringLiteral("system"), state.systemNickname.trimmed()},
        {QStringLiteral("strid_name"), QString::number(state.idsName)},
        {QStringLiteral("file"), relativeToDataDir(dataDir, state.baseIniAbsolutePath)},
    };
    if (!state.bgcsBaseRunBy.trimmed().isEmpty())
        updated.entries.append({QStringLiteral("BGCS_base_run_by"), state.bgcsBaseRunBy.trimmed()});

    for (int index = 0; index < doc->size(); ++index) {
        IniSection &section = (*doc)[index];
        if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
            continue;
        if (normalizeKey(section.value(QStringLiteral("nickname"))) != targetNick)
            continue;
        (*doc)[index] = updated;
        return true;
    }

    doc->append(updated);
    return true;
}

QPair<int, int> mbaseBlockRange(const IniDocument &doc, int startIndex)
{
    int end = doc.size();
    for (int index = startIndex + 1; index < doc.size(); ++index) {
        if (doc.at(index).name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) == 0) {
            end = index;
            break;
        }
    }
    return {startIndex, end};
}

int ensureMbaseSection(IniDocument *doc, const BaseEditState &state)
{
    const QString target = normalizeKey(state.baseNickname);
    const QString reputationNickname = nicknameFromDisplayValue(state.reputation);
    for (int index = 0; index < doc->size(); ++index) {
        IniSection &section = (*doc)[index];
        if (section.name.compare(QStringLiteral("MBase"), Qt::CaseInsensitive) != 0)
            continue;
        if (normalizeKey(section.value(QStringLiteral("nickname"))) == target) {
            setOrRemoveRawEntry(&section.entries, QStringLiteral("nickname"), state.baseNickname.trimmed());
            setOrRemoveRawEntry(&section.entries, QStringLiteral("local_faction"), reputationNickname);
            setOrRemoveRawEntry(&section.entries, QStringLiteral("diff"), QStringLiteral("1"));
            setOrRemoveRawEntry(&section.entries,
                                QStringLiteral("msg_id_prefix"),
                                QStringLiteral("gcs_refer_base_%1").arg(state.baseNickname.trimmed()));
            const auto range = mbaseBlockRange(*doc, index);
            bool vendorFound = false;
            for (int inner = index + 1; inner < range.second; ++inner) {
                IniSection &innerSection = (*doc)[inner];
                if (innerSection.name.compare(QStringLiteral("MVendor"), Qt::CaseInsensitive) != 0)
                    continue;
                setOrRemoveRawEntry(&innerSection.entries, QStringLiteral("num_offers"), QStringLiteral("0, 0"));
                vendorFound = true;
                break;
            }
            if (!vendorFound) {
                IniSection vendor;
                vendor.name = QStringLiteral("MVendor");
                vendor.entries = {{QStringLiteral("num_offers"), QStringLiteral("0, 0")}};
                doc->insert(index + 1, vendor);
            }
            return index;
        }
    }

    IniSection mbase;
    mbase.name = QStringLiteral("MBase");
    mbase.entries = {
        {QStringLiteral("nickname"), state.baseNickname.trimmed()},
        {QStringLiteral("local_faction"), reputationNickname},
        {QStringLiteral("diff"), QStringLiteral("1")},
        {QStringLiteral("msg_id_prefix"), QStringLiteral("gcs_refer_base_%1").arg(state.baseNickname.trimmed())},
    };
    doc->append(mbase);

    IniSection vendor;
    vendor.name = QStringLiteral("MVendor");
    vendor.entries = {{QStringLiteral("num_offers"), QStringLiteral("0, 0")}};
    doc->append(vendor);
    return doc->size() - 2;
}

void syncMbaseRooms(IniDocument *doc, const BaseEditState &state)
{
    if (!doc)
        return;
    const int mbaseIndex = ensureMbaseSection(doc, state);
    const auto range = mbaseBlockRange(*doc, mbaseIndex);
    const QStringList rooms = enabledRoomNames(state);
    QSet<QString> targetRooms;
    for (const BaseRoomState &room : state.rooms) {
        if (room.enabled && !room.roomName.trimmed().isEmpty())
            targetRooms.insert(normalizeKey(canonicalRoomName(room.roomName)));
    }

    for (int index = range.second - 1; index > range.first; --index) {
        const IniSection &section = doc->at(index);
        if (section.name.compare(QStringLiteral("MRoom"), Qt::CaseInsensitive) != 0)
            continue;
        if (targetRooms.contains(normalizeKey(section.value(QStringLiteral("nickname")))))
            doc->removeAt(index);
    }

    const auto refreshedRange = mbaseBlockRange(*doc, mbaseIndex);
    int insertAt = refreshedRange.first + 1;
    for (int index = refreshedRange.first + 1; index < refreshedRange.second; ++index) {
        const QString sectionName = normalizeKey(doc->at(index).name);
        if (sectionName == QStringLiteral("gf_npc")
            || sectionName == QStringLiteral("mvendor")) {
            insertAt = index + 1;
        }
    }

    for (const BaseRoomState &roomState : state.rooms) {
        if (!roomState.enabled || roomState.roomName.trimmed().isEmpty())
            continue;
        const QString room = canonicalRoomName(roomState.roomName);
        IniSection section;
        section.name = QStringLiteral("MRoom");
        section.entries = {
            {QStringLiteral("nickname"), canonicalRoomName(room)},
            {QStringLiteral("character_density"), QString::number(roomDensity(room))},
        };
        QSet<QString> seenNpcNames;
        for (const BaseRoomNpcState &npc : roomState.npcs) {
            const QString nickname = npc.nickname.trimmed();
            if (nickname.isEmpty())
                continue;
            const QString lowered = nickname.toLower();
            if (seenNpcNames.contains(lowered))
                continue;
            seenNpcNames.insert(lowered);

            const QString role = normalizedRoleForRoom(npc.role, room);
            const auto fixture = fixtureSceneForRole(role);
            const QString poseRole = fixture.second.compare(QStringLiteral("bartender"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("Bartender")
                : fixture.second;
            section.entries.append({QStringLiteral("fixture"),
                                    QStringLiteral("%1, Zs/NPC/%2/01/A/Stand, %3, %4")
                                        .arg(nickname, poseRole, fixture.first, fixture.second)});
        }
        doc->insert(insertAt++, section);
    }
}

void populateCreateMbaseBlock(IniDocument *doc, const BaseEditState &state)
{
    if (!doc)
        return;

    const int mbaseIndex = ensureMbaseSection(doc, state);
    auto range = mbaseBlockRange(*doc, mbaseIndex);
    for (int index = range.second - 1; index > range.first; --index) {
        const QString sectionName = normalizeKey(doc->at(index).name);
        if (sectionName == QStringLiteral("basefaction")
            || sectionName == QStringLiteral("gf_npc")
            || sectionName == QStringLiteral("mroom")) {
            doc->removeAt(index);
        }
    }

    range = mbaseBlockRange(*doc, mbaseIndex);
    int vendorIndex = mbaseIndex + 1;
    for (int index = mbaseIndex + 1; index < range.second; ++index) {
        if (normalizeKey(doc->at(index).name) == QStringLiteral("mvendor")) {
            vendorIndex = index;
            break;
        }
    }

    QVector<BaseRoomNpcState> desiredNpcs;
    QHash<QString, QVector<QString>> npcNamesByFaction;
    QHash<QString, QString> originalFactionCase;
    for (const BaseRoomState &room : state.rooms) {
        if (!room.enabled)
            continue;
        for (const BaseRoomNpcState &npc : room.npcs) {
            if (npc.nickname.trimmed().isEmpty())
                continue;
            desiredNpcs.append(npc);
            const QString faction = nicknameFromDisplayValue(npc.reputation.trimmed().isEmpty() ? state.reputation : npc.reputation).trimmed();
            if (faction.isEmpty())
                continue;
            const QString factionKey = normalizeKey(faction);
            if (!originalFactionCase.contains(factionKey))
                originalFactionCase.insert(factionKey, faction);
            npcNamesByFaction[factionKey].append(npc.nickname.trimmed());
        }
    }

    int insertAt = vendorIndex + 1;
    for (auto it = npcNamesByFaction.constBegin(); it != npcNamesByFaction.constEnd(); ++it) {
        IniSection factionSection;
        factionSection.name = QStringLiteral("BaseFaction");
        factionSection.entries = {
            {QStringLiteral("faction"), originalFactionCase.value(it.key())},
            {QStringLiteral("weight"), QStringLiteral("10")},
        };
        QSet<QString> seenNpcNames;
        for (const QString &npcName : it.value()) {
            const QString npcKey = normalizeKey(npcName);
            if (seenNpcNames.contains(npcKey))
                continue;
            seenNpcNames.insert(npcKey);
            factionSection.entries.append({QStringLiteral("npc"), npcName});
        }
        doc->insert(insertAt++, factionSection);
    }

    for (const BaseRoomNpcState &npc : desiredNpcs) {
        IniSection npcSection;
        npcSection.name = QStringLiteral("GF_NPC");
        npcSection.entries = npc.templateEntries;
        if (npcSection.entries.isEmpty()) {
            npcSection.entries = {
                {QStringLiteral("nickname"), npc.nickname.trimmed()},
                {QStringLiteral("body"), npc.body.trimmed().isEmpty() ? QStringLiteral("benchmark_male_body") : npc.body.trimmed()},
                {QStringLiteral("head"), npc.head.trimmed().isEmpty() ? QStringLiteral("benchmark_male_head") : npc.head.trimmed()},
                {QStringLiteral("lefthand"), npc.leftHand.trimmed().isEmpty() ? QStringLiteral("benchmark_male_hand_left") : npc.leftHand.trimmed()},
                {QStringLiteral("righthand"), npc.rightHand.trimmed().isEmpty() ? QStringLiteral("benchmark_male_hand_right") : npc.rightHand.trimmed()},
                {QStringLiteral("affiliation"), nicknameFromDisplayValue(npc.affiliation.trimmed().isEmpty() ? npc.reputation : npc.affiliation)},
                {QStringLiteral("voice"), QStringLiteral("mc_leg_m01")},
            };
        }
        setOrRemoveRawEntry(&npcSection.entries, QStringLiteral("nickname"), npc.nickname.trimmed());
        setOrRemoveRawEntry(&npcSection.entries,
                            QStringLiteral("body"),
                            npc.body.trimmed().isEmpty() ? QStringLiteral("benchmark_male_body") : npc.body.trimmed());
        setOrRemoveRawEntry(&npcSection.entries,
                            QStringLiteral("head"),
                            npc.head.trimmed().isEmpty() ? QStringLiteral("benchmark_male_head") : npc.head.trimmed());
        setOrRemoveRawEntry(&npcSection.entries,
                            QStringLiteral("lefthand"),
                            npc.leftHand.trimmed().isEmpty() ? QStringLiteral("benchmark_male_hand_left") : npc.leftHand.trimmed());
        setOrRemoveRawEntry(&npcSection.entries,
                            QStringLiteral("righthand"),
                            npc.rightHand.trimmed().isEmpty() ? QStringLiteral("benchmark_male_hand_right") : npc.rightHand.trimmed());
        setOrRemoveRawEntry(&npcSection.entries,
                            QStringLiteral("affiliation"),
                            nicknameFromDisplayValue(npc.affiliation.trimmed().isEmpty() ? npc.reputation : npc.affiliation));
        setOrRemoveRawEntry(&npcSection.entries, QStringLiteral("voice"), rawEntryValue(npcSection.entries, QStringLiteral("voice")).trimmed().isEmpty() ? QStringLiteral("mc_leg_m01") : rawEntryValue(npcSection.entries, QStringLiteral("voice")));
        setOrRemoveRawEntry(&npcSection.entries, QStringLiteral("room"), QString());
        doc->insert(insertAt++, npcSection);
    }

    QHash<QString, QVector<QPair<QString, QString>>> fixturesByRoom;
    for (const BaseRoomState &room : state.rooms) {
        if (!room.enabled || room.roomName.trimmed().isEmpty())
            continue;
        QVector<QPair<QString, QString>> fixtures;
        QSet<QString> seenNpcNames;
        for (const BaseRoomNpcState &npc : room.npcs) {
            const QString npcName = npc.nickname.trimmed();
            if (npcName.isEmpty() || seenNpcNames.contains(normalizeKey(npcName)))
                continue;
            seenNpcNames.insert(normalizeKey(npcName));
            fixtures.append({npcName, normalizedRoleForRoom(npc.role, room.roomName)});
        }
        fixturesByRoom.insert(roomKey(room.roomName), fixtures);
    }

    const QHash<QString, int> order = {
        {QStringLiteral("deck"), 1},
        {QStringLiteral("bar"), 2},
        {QStringLiteral("trader"), 3},
        {QStringLiteral("equipment"), 4},
        {QStringLiteral("shipdealer"), 5},
        {QStringLiteral("cityscape"), 6},
    };
    QStringList roomNames = fixturesByRoom.keys();
    std::sort(roomNames.begin(), roomNames.end(), [&](const QString &left, const QString &right) {
        const int leftOrder = order.value(left, 99);
        const int rightOrder = order.value(right, 99);
        if (leftOrder != rightOrder)
            return leftOrder < rightOrder;
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    for (const QString &roomName : roomNames) {
        const QVector<QPair<QString, QString>> fixtures = fixturesByRoom.value(roomName);
        if (fixtures.isEmpty())
            continue;
        IniSection roomSection;
        roomSection.name = QStringLiteral("MRoom");
        roomSection.entries = {
            {QStringLiteral("nickname"), canonicalRoomName(roomName)},
            {QStringLiteral("character_density"), QString::number(roomDensity(roomName))},
        };
        for (const auto &fixtureNpc : fixtures) {
            const auto fixture = fixtureSceneForRole(fixtureNpc.second);
            const QString poseRole = fixture.second.compare(QStringLiteral("bartender"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("Bartender")
                : fixture.second;
            roomSection.entries.append({QStringLiteral("fixture"),
                                        QStringLiteral("%1, Zs/NPC/%2/01/A/Stand, %3, %4")
                                            .arg(fixtureNpc.first, poseRole, fixture.first, fixture.second)});
        }
        doc->insert(insertAt++, roomSection);
    }
}

bool prepareIds(const QString &gameRoot,
                int currentIdsName,
                const QString &displayName,
                int currentIdsInfo,
                const QString &infocardXml,
                int *outIdsName,
                int *outIdsInfo,
                QString *errorMessage)
{
    int newIdsName = currentIdsName;
    if (displayName.trimmed().isEmpty()) {
        newIdsName = 0;
    }

    int newIdsInfo = currentIdsInfo;
    if (infocardXml.trimmed().isEmpty()) {
        newIdsInfo = 0;
    }

    if (displayName.trimmed().isEmpty() && infocardXml.trimmed().isEmpty()) {
        if (outIdsName)
            *outIdsName = newIdsName;
        if (outIdsInfo)
            *outIdsInfo = newIdsInfo;
        return true;
    }

    const IdsDataset dataset = IdsDataService::loadFromGameRoot(gameRoot);
    const QString dllName = IdsDataService::defaultCreationDllName(dataset);

    if (!displayName.trimmed().isEmpty()) {
        QString idsError;
        if (!IdsDataService::writeStringEntry(dataset, dllName, currentIdsName, displayName.trimmed(), &newIdsName, &idsError)) {
            if (errorMessage)
                *errorMessage = idsError.trimmed().isEmpty() ? QObject::tr("Der IDS-Name konnte nicht geschrieben werden.") : idsError;
            return false;
        }
    }

    if (!infocardXml.trimmed().isEmpty()) {
        QString infoError;
        if (!IdsDataService::writeInfocardEntry(dataset, dllName, currentIdsInfo, infocardXml.trimmed(), &newIdsInfo, &infoError)) {
            if (errorMessage)
                *errorMessage = infoError.trimmed().isEmpty() ? QObject::tr("Die Infocard konnte nicht geschrieben werden.") : infoError;
            return false;
        }
    }

    if (outIdsName)
        *outIdsName = newIdsName;
    if (outIdsInfo)
        *outIdsInfo = newIdsInfo;
    return true;
}

void buildObjectForState(std::shared_ptr<SolarObject> object,
                         const BaseEditState &state,
                         const QPointF *scenePos)
{
    if (!object)
        return;

    if (!state.objectNickname.trimmed().isEmpty())
        object->setNickname(state.objectNickname.trimmed());
    object->setArchetype(state.archetype.trimmed());
    object->setType(SolarObject::Station);
    object->setBase(state.baseNickname.trimmed());
    object->setDockWith(state.baseNickname.trimmed());
    object->setLoadout(state.loadout.trimmed());

    QVector<QPair<QString, QString>> rawEntries = object->rawEntries();
    QString posValue = rawEntryValue(rawEntries, QStringLiteral("pos"));
    QString rotateValue = state.rotate.trimmed().isEmpty()
        ? rawEntryValue(rawEntries, QStringLiteral("rotate"))
        : state.rotate.trimmed();

    if (scenePos) {
        const QPointF world = flatlas::rendering::MapScene::qtToFl(scenePos->x(), scenePos->y());
        object->setPosition(QVector3D(static_cast<float>(world.x()), 0.0f, static_cast<float>(world.y())));
        posValue = QStringLiteral("%1, 0, %2").arg(world.x(), 0, 'f', 0).arg(world.y(), 0, 'f', 0);
        rotateValue = normalizeRotateValue(rotateValue);
    }

    rawEntries = buildOrderedObjectEntries(state, posValue, rotateValue, rawEntries);
    object->setRawEntries(rawEntries);
}

bool validateState(const BaseEditState &state, QString *errorMessage)
{
    if (state.objectNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Objekt-Nickname darf nicht leer sein.");
        return false;
    }
    if (state.baseNickname.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Der Base-Nickname darf nicht leer sein.");
        return false;
    }
    if (state.archetype.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das Archetype-Feld darf nicht leer sein.");
        return false;
    }
    const QStringList rooms = enabledRoomNames(state);
    if (rooms.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Mindestens ein aktiver Raum ist erforderlich.");
        return false;
    }
    return true;
}

} // namespace

bool BaseEditService::objectHasBase(const SolarObject &object)
{
    return !objectBaseNickname(object).isEmpty();
}

QVector<BaseRoomState> BaseEditService::defaultRoomsForArchetype(const QString &archetype)
{
    return buildDefaultRoomsForArchetype(archetype);
}

QVector<BaseRoomState> BaseEditService::applyTemplateRoomsForCreate(const BaseEditState &targetState,
                                                                    const BaseEditState &templateState,
                                                                    bool copyNpcs)
{
    QVector<BaseRoomState> mergedRooms = buildDefaultRoomsForArchetype(targetState.archetype);
    QHash<QString, int> indexByRoom;
    for (int index = 0; index < mergedRooms.size(); ++index)
        indexByRoom.insert(normalizeKey(mergedRooms.at(index).roomName), index);

    const QString baseFaction = nicknameFromDisplayValue(targetState.reputation);
    QSet<QString> templateRooms;
    for (int roomIndex = 0; roomIndex < templateState.rooms.size(); ++roomIndex) {
        BaseRoomState room = templateState.rooms.at(roomIndex);
        room.roomName = room.roomName.trimmed();
        if (room.roomName.isEmpty())
            continue;
        room.enabled = true;

        if (copyNpcs) {
            QVector<BaseRoomNpcState> copiedNpcs;
            for (int npcIndex = 0; npcIndex < room.npcs.size(); ++npcIndex) {
                BaseRoomNpcState npc = room.npcs.at(npcIndex);
                npc.nickname = makeGeneratedNpcNickname(targetState.baseNickname, room.roomName, npcIndex + 1);
                if (npc.nameText.trimmed().isEmpty())
                    npc.nameText = npc.nickname;
                if (npc.reputation.trimmed().isEmpty())
                    npc.reputation = baseFaction;
                if (npc.affiliation.trimmed().isEmpty())
                    npc.affiliation = npc.reputation;
                copiedNpcs.append(npc);
            }
            room.npcs = copiedNpcs;
        } else {
            room.npcs.clear();
        }

        const QString roomLookupKey = normalizeKey(room.roomName);
        templateRooms.insert(roomLookupKey);
        if (indexByRoom.contains(roomLookupKey))
            mergedRooms[indexByRoom.value(roomLookupKey)] = room;
        else
            mergedRooms.append(room);
    }

    for (BaseRoomState &room : mergedRooms) {
        if (!templateRooms.contains(normalizeKey(room.roomName))) {
            room.enabled = false;
            room.npcs.clear();
            room.templateContent.clear();
        }
    }
    return mergedRooms;
}

BaseArchetypeDefaults BaseEditService::archetypeDefaults(const QString &archetype,
                                                         const QString &gameRoot,
                                                         const QHash<QString, QString> &textOverrides)
{
    BaseArchetypeDefaults defaults;
    defaults.archetype = archetype.trimmed();

    const QVector<ExistingBaseObjectData> matches = existingBaseObjectsForArchetype(archetype, gameRoot, textOverrides);
    for (const ExistingBaseObjectData &match : matches) {
        if (defaults.sourceBaseNickname.isEmpty()) {
            defaults.sourceBaseNickname = match.baseNickname;
            defaults.sourceObjectNickname = match.objectNickname;
        }
        if (defaults.loadout.isEmpty() && !match.loadout.isEmpty())
            defaults.loadout = match.loadout;
        if (defaults.infocardXml.isEmpty() && match.idsInfo > 0)
            defaults.infocardXml = resolvedInfocardXml(gameRoot, match.idsInfo);
        if (!defaults.loadout.isEmpty() && !defaults.infocardXml.isEmpty())
            break;
    }

    if (defaults.loadout.isEmpty())
        defaults.loadout = defaultLoadoutFromSolarArch(archetype, gameRoot);
    return defaults;
}

BaseEditState BaseEditService::makeCreateState(const SystemDocument &document,
                                               const QString &gameRoot,
                                               QString *errorMessage)
{
    BaseEditState state;
    state.editMode = false;
    state.systemNickname = document.name().trimmed();
    if (state.systemNickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das System besitzt keinen Nickname.");
        return state;
    }

    const int nextNumber = nextObjectNumber(document, state.systemNickname);
    const QString numberText = QStringLiteral("%1").arg(nextNumber, 2, 10, QLatin1Char('0'));
    state.objectNickname = QStringLiteral("%1_%2").arg(state.systemNickname, numberText);
    state.baseNickname = state.objectNickname + QStringLiteral("_Base");
    state.archetype = QStringLiteral("station");
    state.rotate = QStringLiteral("0, 0, 0");
    const BaseArchetypeDefaults defaults = archetypeDefaults(state.archetype, gameRoot);
    state.loadout = defaults.loadout;
    state.infocardXml = defaults.infocardXml;
    state.rooms = buildDefaultRoomsForArchetype(state.archetype);
    state.startRoom = chooseStartRoom(enabledRoomNames(state), QStringLiteral("Deck"));

    const QString systemFile = document.filePath();
    const QString gameData = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    const QString universeIni = flatlas::core::PathUtils::ciResolvePath(gameData, QStringLiteral("UNIVERSE/universe.ini"));
    const QString mbasePath = flatlas::core::PathUtils::ciResolvePath(gameData, QStringLiteral("MISSIONS/mbases.ini"));
    const QString baseDir = QFileInfo(systemFile).absolutePath() + QStringLiteral("/BASES");
    const QString roomsDir = baseDir + QStringLiteral("/ROOMS");
    state.universeIniAbsolutePath = universeIni;
    state.mbaseAbsolutePath = mbasePath;
    state.baseIniAbsolutePath = QDir(baseDir).absoluteFilePath(state.baseNickname.toLower() + QStringLiteral(".ini"));
    state.roomsDirectoryAbsolutePath = roomsDir;
    state.universeBaseFileRelativePath = gameData.isEmpty() ? QString() : relativeToDataDir(gameData, state.baseIniAbsolutePath);
    return state;
}

bool BaseEditService::loadState(const SystemDocument &document,
                                const SolarObject &object,
                                const QString &gameRoot,
                                const QHash<QString, QString> &textOverrides,
                                BaseEditState *outState,
                                QString *errorMessage)
{
    if (!outState)
        return false;

    BaseEditState state;
    state.editMode = true;
    state.objectNickname = object.nickname().trimmed();
    state.baseNickname = objectBaseNickname(object);
    state.systemNickname = document.name().trimmed();
    state.archetype = object.archetype().trimmed();
    state.loadout = object.loadout().trimmed();
    state.reputation = rawEntryValue(object.rawEntries(), QStringLiteral("reputation"));
    state.pilot = rawEntryValue(object.rawEntries(), QStringLiteral("pilot"));
    state.voice = rawEntryValue(object.rawEntries(), QStringLiteral("voice"));
    const auto costume = splitSpaceCostume(rawEntryValue(object.rawEntries(), QStringLiteral("space_costume")));
    state.head = costume.first;
    state.body = costume.second;
    state.rotate = normalizeRotateValue(rawEntryValue(object.rawEntries(), QStringLiteral("rotate")));
    state.idsName = object.idsName();
    state.idsInfo = object.idsInfo();
    state.displayName = resolvedDisplayName(gameRoot, state.idsName);
    state.infocardXml = resolvedInfocardXml(gameRoot, state.idsInfo);

    if (state.baseNickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("Das ausgewaehlte Objekt besitzt keine verknuepfte Base.");
        return false;
    }

    if (!loadBaseFileState(&state, gameRoot, textOverrides, errorMessage)) {
        const QString baseDir = QFileInfo(document.filePath()).absolutePath() + QStringLiteral("/BASES");
        state.baseIniAbsolutePath = QDir(baseDir).absoluteFilePath(state.baseNickname.toLower() + QStringLiteral(".ini"));
        const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
        state.universeBaseFileRelativePath = relativeToDataDir(dataDir, state.baseIniAbsolutePath);
        state.roomsDirectoryAbsolutePath = QFileInfo(state.baseIniAbsolutePath).absolutePath() + QStringLiteral("/ROOMS");
        if (!loadBaseFileState(&state, gameRoot, textOverrides, errorMessage))
            return false;
    }

    *outState = state;
    return true;
}

bool BaseEditService::loadTemplateState(const QString &baseNickname,
                                        const QString &gameRoot,
                                        const QHash<QString, QString> &textOverrides,
                                        BaseEditState *outState,
                                        QString *errorMessage)
{
    if (!outState)
        return false;

    BaseEditState state;
    state.baseNickname = baseNickname.trimmed();
    if (!loadBaseFileState(&state, gameRoot, textOverrides, errorMessage))
        return false;
    *outState = state;
    return true;
}

bool BaseEditService::applyCreate(const BaseEditState &state,
                                  const QPointF &scenePos,
                                  const QString &gameRoot,
                                  const QHash<QString, QString> &textOverrides,
                                  BaseApplyResult *outResult,
                                  QString *errorMessage)
{
    if (!outResult)
        return false;
    if (!validateState(state, errorMessage))
        return false;

    BaseEditState working = state;
    if (!prepareIds(gameRoot,
                    working.idsName,
                    working.displayName,
                    working.idsInfo,
                    working.infocardXml,
                    &working.idsName,
                    &working.idsInfo,
                    errorMessage)) {
        return false;
    }

    auto object = std::make_shared<SolarObject>();
    buildObjectForState(object, working, &scenePos);
    object->setIdsName(working.idsName);
    object->setIdsInfo(working.idsInfo);

    IniDocument universeDoc = IniParser::parseText(textForPath(working.universeIniAbsolutePath, textOverrides));
    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    updateUniverseBaseSection(&universeDoc, working, dataDir);

    IniDocument mbasesDoc = IniParser::parseText(textForPath(working.mbaseAbsolutePath, textOverrides));
    populateCreateMbaseBlock(&mbasesDoc, working);

    outResult->createdObject = object;
    outResult->stagedWrites = {
        {working.universeIniAbsolutePath, normalizeGeneratedText(IniParser::serialize(universeDoc))},
        {working.baseIniAbsolutePath, buildUpdatedBaseIniText(QString(), working)},
        {working.mbaseAbsolutePath, normalizeGeneratedText(IniParser::serialize(mbasesDoc))},
    };

    const QStringList rooms = enabledRoomNames(working);
    for (const BaseRoomState &room : working.rooms) {
        if (!room.enabled || room.roomName.trimmed().isEmpty())
            continue;
        const QString canonical = canonicalRoomName(room.roomName);
        const bool fromTemplate = !room.templateContent.trimmed().isEmpty();
        QString roomText = !fromTemplate
            ? generateRoomIniText(canonical, rooms, chooseStartRoom(rooms, working.startRoom))
            : adaptTemplateRoom(room.templateContent, rooms);
        if (!room.scenePath.trimmed().isEmpty())
            roomText = overrideRoomScene(roomText, room.scenePath);
        roomText = normalizeRoomNavigation(roomText,
                                          canonical,
                                          rooms,
                                          chooseStartRoom(rooms, working.startRoom));
        const QString roomPath = QDir(working.roomsDirectoryAbsolutePath)
            .absoluteFilePath(QStringLiteral("%1_%2.ini").arg(generatedBaseStem(working.baseNickname), normalizeKey(canonical)));
        outResult->stagedWrites.append({roomPath, roomText});
    }
    return true;
}

bool BaseEditService::applyEdit(SolarObject &object,
                                const BaseEditState &state,
                                const QString &gameRoot,
                                const QHash<QString, QString> &textOverrides,
                                BaseApplyResult *outResult,
                                QString *errorMessage)
{
    if (!outResult)
        return false;
    if (!validateState(state, errorMessage))
        return false;

    BaseEditState working = state;
    if (!prepareIds(gameRoot,
                    working.idsName,
                    working.displayName,
                    working.idsInfo,
                    working.infocardXml,
                    &working.idsName,
                    &working.idsInfo,
                    errorMessage)) {
        return false;
    }

    buildObjectForState(std::shared_ptr<SolarObject>(&object, [](SolarObject *) {}), working, nullptr);
    object.setIdsName(working.idsName);
    object.setIdsInfo(working.idsInfo);

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("DATA"));
    IniDocument universeDoc = IniParser::parseText(textForPath(working.universeIniAbsolutePath, textOverrides));
    updateUniverseBaseSection(&universeDoc, working, dataDir);

    IniDocument mbasesDoc = IniParser::parseText(textForPath(working.mbaseAbsolutePath, textOverrides));
    syncMbaseRooms(&mbasesDoc, working);
    const QString existingBaseIniText = textForPath(working.baseIniAbsolutePath, textOverrides);

    outResult->stagedWrites = {
        {working.universeIniAbsolutePath, normalizeGeneratedText(IniParser::serialize(universeDoc))},
        {working.baseIniAbsolutePath, buildUpdatedBaseIniText(existingBaseIniText, working)},
        {working.mbaseAbsolutePath, normalizeGeneratedText(IniParser::serialize(mbasesDoc))},
    };

    const QStringList rooms = enabledRoomNames(working);
    const QString startRoom = chooseStartRoom(rooms, working.startRoom);
    for (const BaseRoomState &room : working.rooms) {
        if (!room.enabled || room.roomName.trimmed().isEmpty())
            continue;
        const QString canonical = canonicalRoomName(room.roomName);
        const QString roomPath = QDir(working.roomsDirectoryAbsolutePath)
            .absoluteFilePath(QStringLiteral("%1_%2.ini").arg(generatedBaseStem(working.baseNickname), normalizeKey(canonical)));
        QString roomText = textForPath(roomPath, textOverrides);
        const bool hadExistingText = !roomText.trimmed().isEmpty();
        if (!hadExistingText)
            roomText = generateRoomIniText(canonical, rooms, startRoom);
        if (!room.scenePath.trimmed().isEmpty())
            roomText = overrideRoomScene(roomText, room.scenePath);
        roomText = normalizeRoomNavigation(roomText, canonical, rooms, startRoom);
        outResult->stagedWrites.append({roomPath, roomText});
    }
    return true;
}

} // namespace flatlas::editors