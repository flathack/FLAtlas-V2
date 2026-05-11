#include "TradeRouteDataService.h"

#include "core/PathUtils.h"
#include "editors/universe/UniverseSerializer.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStringDecoder>

using namespace flatlas::domain;
using namespace flatlas::infrastructure;

namespace flatlas::editors {

namespace {

QString normalizedNickname(const QString &value)
{
    return value.trimmed().toLower();
}

QVector3D parsePos(const QString &value)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (flatlas::core::PathUtils::parsePosition(value, x, y, z))
        return {x, y, z};

    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() >= 2)
        return {parts.at(0).trimmed().toFloat(), 0.0f, parts.at(1).trimmed().toFloat()};
    return {};
}

QString resolveUniverseIni(const QString &dataPath)
{
    return flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("UNIVERSE/universe.ini"));
}

QString resolveGoodsIni(const QString &dataPath)
{
    return flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("EQUIPMENT/goods.ini"));
}

QString resolveSelectEquipIni(const QString &dataPath)
{
    return flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("EQUIPMENT/select_equip.ini"));
}

QString preferredMarketFile(const QString &dataPath)
{
    const QString preferred = flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("EQUIPMENT/market_commodities.ini"));
    if (!preferred.isEmpty())
        return preferred;

    QDirIterator it(dataPath,
                    {QStringLiteral("market_*.ini"), QStringLiteral("*_market.ini")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QString freelancerExeDirForDataPath(const QString &dataPath)
{
    const QFileInfo dataInfo(dataPath);
    const QString gameRoot = dataInfo.fileName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0
        ? dataInfo.absolutePath()
        : dataPath;
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gameRoot, QStringLiteral("EXE"));
    return exeDir.isEmpty() ? gameRoot : exeDir;
}

QString resolvedIdsDisplayName(const IdsStringTable &ids, int idsName, const QString &fallback)
{
    const QString displayName = idsName > 0 ? ids.getString(idsName).trimmed() : QString();
    return displayName.isEmpty() ? fallback.trimmed() : displayName;
}

void applyUniverseDisplayNames(UniverseData *universe, const IdsStringTable &ids)
{
    if (!universe)
        return;

    for (auto &system : universe->systems) {
        QString displayName = resolvedIdsDisplayName(ids, system.idsName, QString());
        if (displayName.isEmpty())
            displayName = resolvedIdsDisplayName(ids, system.stridName, QString());
        if (!displayName.isEmpty())
            system.displayName = displayName;
        else if (system.displayName.trimmed().isEmpty())
            system.displayName = system.nickname;
    }
}

QHash<QString, IniSection> selectCommoditySections(const QString &selectEquipPath);

QStringList marketFiles(const QString &dataPath)
{
    QStringList files;
    QDirIterator it(dataPath,
                    {QStringLiteral("market_*.ini"), QStringLiteral("*_market.ini")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        files.append(it.next());
    files.removeDuplicates();
    return files;
}

QString systemFileAbsolutePath(const QString &dataPath, const SystemInfo &system)
{
    const QString universeIni = resolveUniverseIni(dataPath);
    const QString universeDir = QFileInfo(universeIni).absolutePath();
    QString absolute = flatlas::core::PathUtils::ciResolvePath(universeDir, system.filePath);
    if (absolute.isEmpty())
        absolute = QDir(universeDir).filePath(system.filePath);
    return absolute;
}

void scanSystemObjects(const QString &dataPath,
                       const std::shared_ptr<UniverseData> &universe,
                       const IdsStringTable &ids,
                       QHash<QString, TradeBaseRecord> *bases,
                       QVector<TradeJumpRecord> *jumps)
{
    if (!universe)
        return;

    for (const auto &system : universe->systems) {
        const QString filePath = systemFileAbsolutePath(dataPath, system);
        if (filePath.isEmpty() || !QFile::exists(filePath))
            continue;

        const IniDocument doc = IniParser::parseFile(filePath);
        for (const auto &section : doc) {
            if (section.name.compare(QStringLiteral("Object"), Qt::CaseInsensitive) != 0)
                continue;

            const QString baseNickname = section.value(QStringLiteral("base")).trimmed();
            const QString objectNickname = section.value(QStringLiteral("nickname")).trimmed();
            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt();
            const QString objectDisplayName =
                resolvedIdsDisplayName(ids, idsName, objectNickname.isEmpty() ? baseNickname : objectNickname);
            const QVector3D position = parsePos(section.value(QStringLiteral("pos")));

            if (!baseNickname.isEmpty()) {
                TradeBaseRecord base;
                base.nickname = baseNickname;
                base.displayName = objectDisplayName.isEmpty() ? baseNickname : objectDisplayName;
                base.systemNickname = system.nickname;
                base.systemDisplayName = system.displayName.isEmpty() ? system.nickname : system.displayName;
                base.position = position;
                bases->insert(normalizedNickname(base.nickname), base);
            }

            const QString gotoValue = section.value(QStringLiteral("goto")).trimmed();
            if (gotoValue.isEmpty())
                continue;

            const QStringList parts = gotoValue.split(QLatin1Char(','));
            if (parts.isEmpty())
                continue;

            const QString targetSystem = parts.at(0).trimmed();
            if (targetSystem.isEmpty() || targetSystem.compare(system.nickname, Qt::CaseInsensitive) == 0)
                continue;

            TradeJumpRecord jump;
            jump.objectNickname = objectNickname;
            jump.systemNickname = system.nickname;
            jump.targetSystemNickname = targetSystem;
            jump.kind = section.value(QStringLiteral("archetype")).contains(QStringLiteral("jumphole"), Qt::CaseInsensitive)
                ? QStringLiteral("hole")
                : QStringLiteral("gate");
            jump.position = position;
            jump.objectDisplayName = objectDisplayName;
            jumps->append(jump);
        }
    }
}

QVector<TradeCommodityRecord> loadCommodities(const QString &goodsFilePath,
                                              const QHash<QString, IniSection> &selectCommodities,
                                              const IdsStringTable &ids)
{
    QVector<TradeCommodityRecord> commodities;
    if (goodsFilePath.isEmpty() || !QFile::exists(goodsFilePath))
        return commodities;

    const IniDocument doc = IniParser::parseFile(goodsFilePath);
    for (const auto &section : doc) {
        if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0)
            continue;

        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (!nickname.startsWith(QStringLiteral("commodity_"), Qt::CaseInsensitive))
            continue;

        TradeCommodityRecord commodity;
        commodity.nickname = nickname;
        commodity.msgIdPrefix = section.value(QStringLiteral("msg_id_prefix")).trimmed();
        commodity.equipment = section.value(QStringLiteral("equipment")).trimmed();
        const QString equipmentNickname = commodity.equipment.trimmed().isEmpty() ? commodity.nickname : commodity.equipment.trimmed();
        const IniSection selectSection = selectCommodities.value(normalizedNickname(equipmentNickname));
        commodity.basePrice = section.value(QStringLiteral("price")).toInt();
        commodity.volume = qMax(1, section.value(QStringLiteral("volume"), QStringLiteral("1")).toInt());
        commodity.idsName = selectSection.value(QStringLiteral("ids_name")).toInt();
        commodity.idsInfo = selectSection.value(QStringLiteral("ids_info")).toInt();
        commodity.idsInfoText = commodity.idsInfo > 0 ? ids.getString(commodity.idsInfo).trimmed() : QString();
        commodity.combinable = section.value(QStringLiteral("combinable"), QStringLiteral("true")).trimmed()
                                   .compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        commodity.goodSellPrice = section.value(QStringLiteral("good_sell_price")).toDouble();
        commodity.badBuyPrice = section.value(QStringLiteral("bad_buy_price")).toDouble();
        commodity.badSellPrice = section.value(QStringLiteral("bad_sell_price")).toDouble();
        commodity.goodBuyPrice = section.value(QStringLiteral("good_buy_price")).toDouble();
        commodity.shopArchetype = section.value(QStringLiteral("shop_archetype")).trimmed();
        commodity.itemIcon = section.value(QStringLiteral("item_icon")).trimmed();
        commodity.jumpDist = section.value(QStringLiteral("jump_dist")).toInt();
        commodity.unitsPerContainer = qMax(1, selectSection.value(QStringLiteral("units_per_container"), QStringLiteral("30")).toInt());
        commodity.podAppearance = selectSection.value(QStringLiteral("pod_appearance"), QStringLiteral("cargopod_grey")).trimmed();
        commodity.lootAppearance = selectSection.value(QStringLiteral("loot_appearance"), QStringLiteral("lootcrate_grey")).trimmed();
        commodity.decayPerSecond = selectSection.value(QStringLiteral("decay_per_second")).toDouble();
        commodity.hitPts = qMax(1, selectSection.value(QStringLiteral("hit_pts"), QStringLiteral("250")).toInt());
        commodity.displayName = resolvedIdsDisplayName(
            ids, commodity.idsName, TradeRouteDataService::fallbackCommodityDisplayName(nickname));
        commodity.sourceFilePath = goodsFilePath;
        commodities.append(commodity);
    }

    std::sort(commodities.begin(), commodities.end(), [](const TradeCommodityRecord &left, const TradeCommodityRecord &right) {
        return left.nickname.toLower() < right.nickname.toLower();
    });
    return commodities;
}

QHash<QString, TradeCommodityRecord> commodityMap(const QVector<TradeCommodityRecord> &commodities)
{
    QHash<QString, TradeCommodityRecord> byNickname;
    for (const auto &commodity : commodities)
        byNickname.insert(normalizedNickname(commodity.nickname), commodity);
    return byNickname;
}

bool shouldPersistMarketPrice(const TradePriceRecord &price)
{
    if (price.implicit)
        return false;
    if (price.isSource)
        return true;
    return !qFuzzyCompare(price.multiplier, 1.0);
}

QVector<TradePriceRecord> loadPrices(const QString &dataPath,
                                     const QVector<TradeCommodityRecord> &commodities,
                                     const QHash<QString, TradeBaseRecord> &bases)
{
    QVector<TradePriceRecord> prices;
    const QHash<QString, TradeCommodityRecord> commoditiesByNickname = commodityMap(commodities);
    QHash<QString, QSet<QString>> explicitBasesByCommodity;

    for (const auto &filePath : marketFiles(dataPath)) {
        const IniDocument doc = IniParser::parseFile(filePath);
        for (const auto &section : doc) {
            if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0)
                continue;

            const QString baseNickname = section.value(QStringLiteral("base")).trimmed();
            const QString normalizedBase = normalizedNickname(baseNickname);
            if (baseNickname.isEmpty() || !bases.contains(normalizedBase))
                continue;

            for (const QString &marketLine : section.values(QStringLiteral("MarketGood"))) {
                const QStringList fields = marketLine.split(QLatin1Char(','));
                if (fields.size() < 7)
                    continue;

                const QString commodityNickname = fields.at(0).trimmed();
                const QString normalizedCommodity = normalizedNickname(commodityNickname);
                if (!commoditiesByNickname.contains(normalizedCommodity))
                    continue;

                bool okRelation = false;
                bool okMultiplier = false;
                const int relationFlag = static_cast<int>(fields.at(5).trimmed().toDouble(&okRelation));
                const double multiplier = fields.at(6).trimmed().toDouble(&okMultiplier);
                if (!okRelation || !okMultiplier || multiplier <= 0.0)
                    continue;

                const auto commodity = commoditiesByNickname.value(normalizedCommodity);
                if (commodity.basePrice <= 0)
                    continue;

                TradePriceRecord price;
                price.baseNickname = baseNickname;
                price.baseDisplayName = bases.value(normalizedBase).displayName;
                price.systemNickname = bases.value(normalizedBase).systemNickname;
                price.commodityNickname = commodityNickname;
                price.multiplier = multiplier;
                price.price = qRound(static_cast<double>(commodity.basePrice) * multiplier);
                price.isSource = (relationFlag == 0);
                price.implicit = false;
                price.sourceFilePath = filePath;
                prices.append(price);
                explicitBasesByCommodity[normalizedCommodity].insert(normalizedBase);
            }
        }
    }

    for (const auto &commodity : commodities) {
        if (commodity.basePrice <= 0)
            continue;
        const QString normalizedCommodity = normalizedNickname(commodity.nickname);
        for (auto it = bases.constBegin(); it != bases.constEnd(); ++it) {
            if (explicitBasesByCommodity.value(normalizedCommodity).contains(it.key()))
                continue;

            TradePriceRecord price;
            price.baseNickname = it.value().nickname;
            price.baseDisplayName = it.value().displayName;
            price.systemNickname = it.value().systemNickname;
            price.commodityNickname = commodity.nickname;
            price.multiplier = 1.0;
            price.price = commodity.basePrice;
            price.isSource = false;
            price.implicit = true;
            prices.append(price);
        }
    }

    return prices;
}

void updateOrAppendEntry(QVector<IniEntry> *entries, const QString &key, const QString &value)
{
    for (auto &entry : *entries) {
        if (entry.first.compare(key, Qt::CaseInsensitive) == 0) {
            entry.second = value;
            return;
        }
    }
    entries->append({key, value});
}

QString selectNicknameForCommodity(const TradeCommodityRecord &commodity)
{
    const QString equipment = commodity.equipment.trimmed();
    return equipment.isEmpty() ? commodity.nickname.trimmed() : equipment;
}

QVector<IniEntry> selectCommodityEntries(const TradeCommodityRecord &commodity)
{
    QVector<IniEntry> entries;
    entries.append({QStringLiteral("nickname"), selectNicknameForCommodity(commodity)});
    if (commodity.idsName > 0)
        entries.append({QStringLiteral("ids_name"), QString::number(commodity.idsName)});
    if (commodity.idsInfo > 0)
        entries.append({QStringLiteral("ids_info"), QString::number(commodity.idsInfo)});
    entries.append({QStringLiteral("units_per_container"), QString::number(qMax(1, commodity.unitsPerContainer))});
    entries.append({QStringLiteral("pod_appearance"),
                    commodity.podAppearance.trimmed().isEmpty() ? QStringLiteral("cargopod_grey") : commodity.podAppearance.trimmed()});
    entries.append({QStringLiteral("loot_appearance"),
                    commodity.lootAppearance.trimmed().isEmpty() ? QStringLiteral("lootcrate_grey") : commodity.lootAppearance.trimmed()});
    entries.append({QStringLiteral("decay_per_second"), QString::number(commodity.decayPerSecond, 'f', 6)});
    entries.append({QStringLiteral("volume"), QString::number(qMax(1, commodity.volume))});
    entries.append({QStringLiteral("hit_pts"), QString::number(qMax(1, commodity.hitPts))});
    return entries;
}

void removeEntries(QVector<IniEntry> *entries, const QStringList &keys)
{
    entries->erase(std::remove_if(entries->begin(), entries->end(), [&keys](const IniEntry &entry) {
        for (const QString &key : keys) {
            if (entry.first.compare(key, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    }), entries->end());
}

QHash<QString, IniSection> selectCommoditySections(const QString &selectEquipPath)
{
    QHash<QString, IniSection> sections;
    if (selectEquipPath.isEmpty() || !QFile::exists(selectEquipPath))
        return sections;

    const IniDocument doc = IniParser::parseFile(selectEquipPath);
    for (const auto &section : doc) {
        if (section.name.compare(QStringLiteral("Commodity"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = normalizedNickname(section.value(QStringLiteral("nickname")));
        if (!nickname.isEmpty())
            sections.insert(nickname, section);
    }
    return sections;
}

QString iniTextForFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray raw = file.readAll();
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = utf8(raw);
    if (!utf8.hasError())
        return text;
    return QString::fromLatin1(raw);
}

QString sectionNameForHeader(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char('[')) || !trimmed.endsWith(QLatin1Char(']')))
        return {};
    return trimmed.mid(1, trimmed.length() - 2).trimmed();
}

QString entryKeyForLine(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1String("//")))
        return {};

    const int eqPos = line.indexOf(QLatin1Char('='));
    if (eqPos <= 0)
        return {};
    return line.left(eqPos).trimmed();
}

QString entryValueForLine(const QString &line)
{
    const int eqPos = line.indexOf(QLatin1Char('='));
    if (eqPos <= 0)
        return {};

    QString value = line.mid(eqPos + 1).trimmed();
    const int semicolon = value.indexOf(QLatin1Char(';'));
    if (semicolon >= 0)
        value = value.left(semicolon).trimmed();
    return value;
}

QString sectionNicknameFromLines(const QStringList &lines, int headerIndex, int endIndex)
{
    for (int i = headerIndex + 1; i < endIndex; ++i) {
        if (entryKeyForLine(lines.at(i)).compare(QStringLiteral("nickname"), Qt::CaseInsensitive) == 0)
            return entryValueForLine(lines.at(i));
    }
    return {};
}

QString updatedEntryLine(const QString &line, const QString &key, const QString &value)
{
    const int keyStart = line.indexOf(key, 0, Qt::CaseInsensitive);
    const QString indent = keyStart > 0 ? line.left(keyStart) : QString();
    QString suffix;
    const int eqPos = line.indexOf(QLatin1Char('='));
    const int semicolon = eqPos >= 0 ? line.indexOf(QLatin1Char(';'), eqPos + 1) : -1;
    if (semicolon >= 0)
        suffix = QLatin1Char(' ') + line.mid(semicolon).trimmed();
    return indent + key + QStringLiteral(" = ") + value + suffix;
}

QString serializeNewSelectCommoditySection(const TradeCommodityRecord &commodity)
{
    QString text = QStringLiteral("[Commodity]\n");
    for (const auto &entry : selectCommodityEntries(commodity))
        text += entry.first + QStringLiteral(" = ") + entry.second + QLatin1Char('\n');
    return text;
}

QString updateSelectCommodityBlock(const QStringList &lines, int headerIndex, int endIndex, const TradeCommodityRecord &commodity)
{
    const QVector<IniEntry> entries = selectCommodityEntries(commodity);
    QSet<QString> writtenKeys;
    QStringList out;
    out.reserve(endIndex - headerIndex + entries.size());
    out.append(lines.at(headerIndex));

    for (int i = headerIndex + 1; i < endIndex; ++i) {
        const QString line = lines.at(i);
        const QString key = entryKeyForLine(line);
        if (key.isEmpty()) {
            out.append(line);
            continue;
        }

        const auto it = std::find_if(entries.begin(), entries.end(), [&key](const IniEntry &entry) {
            return entry.first.compare(key, Qt::CaseInsensitive) == 0;
        });
        if (it == entries.end()) {
            out.append(line);
            continue;
        }
        if (writtenKeys.contains(key.toLower()))
            continue;

        out.append(updatedEntryLine(line, it->first, it->second));
        writtenKeys.insert(key.toLower());
    }

    for (const auto &entry : entries) {
        if (!writtenKeys.contains(entry.first.toLower()))
            out.append(entry.first + QStringLiteral(" = ") + entry.second);
    }
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

bool writeSelectEquipPreservingComments(const QString &selectEquipPath,
                                        const QVector<TradeCommodityRecord> &commodities,
                                        QString *errorMessage)
{
    QHash<QString, TradeCommodityRecord> commoditiesBySelectNickname;
    for (const auto &commodity : commodities) {
        const QString nickname = normalizedNickname(selectNicknameForCommodity(commodity));
        if (!nickname.isEmpty())
            commoditiesBySelectNickname.insert(nickname, commodity);
    }

    QSet<QString> writtenSelectCommoditySections;
    QString out;
    if (QFile::exists(selectEquipPath)) {
        const QString text = iniTextForFile(selectEquipPath);
        const QStringList lines = text.split(QLatin1Char('\n'));
        int sectionStart = -1;
        int i = 0;
        while (i < lines.size()) {
            const QString sectionName = sectionNameForHeader(lines.at(i));
            if (sectionName.isEmpty()) {
                if (sectionStart < 0)
                    out += lines.at(i) + QLatin1Char('\n');
                ++i;
                continue;
            }

            sectionStart = i;
            int sectionEnd = i + 1;
            while (sectionEnd < lines.size() && sectionNameForHeader(lines.at(sectionEnd)).isEmpty())
                ++sectionEnd;

            if (sectionName.compare(QStringLiteral("Commodity"), Qt::CaseInsensitive) != 0) {
                for (int lineIndex = sectionStart; lineIndex < sectionEnd; ++lineIndex)
                    out += lines.at(lineIndex) + QLatin1Char('\n');
                i = sectionEnd;
                continue;
            }

            const QString nickname = normalizedNickname(sectionNicknameFromLines(lines, sectionStart, sectionEnd));
            if (!nickname.startsWith(QStringLiteral("commodity_"))) {
                for (int lineIndex = sectionStart; lineIndex < sectionEnd; ++lineIndex)
                    out += lines.at(lineIndex) + QLatin1Char('\n');
                i = sectionEnd;
                continue;
            }

            const auto it = commoditiesBySelectNickname.constFind(nickname);
            if (it != commoditiesBySelectNickname.constEnd()) {
                out += updateSelectCommodityBlock(lines, sectionStart, sectionEnd, it.value());
                writtenSelectCommoditySections.insert(nickname);
            }
            i = sectionEnd;
        }
    }

    for (const auto &commodity : commodities) {
        const QString nickname = normalizedNickname(selectNicknameForCommodity(commodity));
        if (nickname.isEmpty() || writtenSelectCommoditySections.contains(nickname))
            continue;
        if (!out.isEmpty() && !out.endsWith(QStringLiteral("\n\n")))
            out += QLatin1Char('\n');
        out += serializeNewSelectCommoditySection(commodity);
    }

    QFile selectFile(selectEquipPath);
    if (!selectFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not write %1").arg(selectEquipPath);
        return false;
    }
    selectFile.write(out.toUtf8());
    return true;
}

} // namespace

QString TradeRouteDataService::fallbackCommodityDisplayName(const QString &nickname)
{
    QString raw = nickname.trimmed();
    if (raw.startsWith(QStringLiteral("commodity_"), Qt::CaseInsensitive))
        raw = raw.mid(QStringLiteral("commodity_").size());
    const QStringList parts = raw.split(QLatin1Char('_'), Qt::SkipEmptyParts);
    QStringList pretty;
    for (const auto &part : parts)
        pretty.append(part.left(1).toUpper() + part.mid(1));
    return pretty.join(QLatin1Char(' '));
}

TradeRouteWorkspaceData TradeRouteDataService::loadFromDataPath(const QString &dataPath)
{
    TradeRouteWorkspaceData workspace;
    workspace.dataPath = dataPath;
    workspace.goodsFilePath = resolveGoodsIni(dataPath);
    workspace.selectEquipFilePath = resolveSelectEquipIni(dataPath);
    workspace.preferredMarketFilePath = preferredMarketFile(dataPath);

    IdsStringTable ids;
    if (!dataPath.isEmpty())
        ids.loadFromFreelancerDir(freelancerExeDirForDataPath(dataPath));

    const QString universeIni = resolveUniverseIni(dataPath);
    if (!universeIni.isEmpty()) {
        auto loadedUniverse = UniverseSerializer::load(universeIni);
        if (loadedUniverse) {
            applyUniverseDisplayNames(loadedUniverse.get(), ids);
            workspace.universe = std::shared_ptr<UniverseData>(loadedUniverse.release());
        }
    }
    if (!workspace.universe)
        workspace.universe = std::make_shared<UniverseData>();

    workspace.commodities = loadCommodities(workspace.goodsFilePath,
                                            selectCommoditySections(workspace.selectEquipFilePath),
                                            ids);

    QHash<QString, TradeBaseRecord> bases;
    scanSystemObjects(dataPath, workspace.universe, ids, &bases, &workspace.jumps);
    workspace.bases = bases.values().toVector();
    std::sort(workspace.bases.begin(), workspace.bases.end(), [](const TradeBaseRecord &left, const TradeBaseRecord &right) {
        return left.nickname.toLower() < right.nickname.toLower();
    });

    workspace.prices = loadPrices(dataPath, workspace.commodities, bases);
    return workspace;
}

bool TradeRouteDataService::saveWorkspace(const TradeRouteWorkspaceData &workspace, QString *errorMessage)
{
    if (workspace.goodsFilePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("goods.ini was not found for the current Freelancer data path.");
        return false;
    }
    if (workspace.preferredMarketFilePath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("No writable market file was found for the current Freelancer data path.");
        return false;
    }

    IniDocument goodsDoc = IniParser::parseFile(workspace.goodsFilePath);
    IniDocument updatedGoods;
    QSet<QString> writtenCommoditySections;

    for (const auto &section : goodsDoc) {
        if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0) {
            updatedGoods.append(section);
            continue;
        }

        const QString nickname = normalizedNickname(section.value(QStringLiteral("nickname")));
        if (!nickname.startsWith(QStringLiteral("commodity_"))) {
            updatedGoods.append(section);
            continue;
        }

        const auto it = std::find_if(workspace.commodities.begin(), workspace.commodities.end(), [&nickname](const TradeCommodityRecord &commodity) {
            return normalizedNickname(commodity.nickname) == nickname;
        });
        if (it == workspace.commodities.end())
            continue;

        IniSection updated = section;
        removeEntries(&updated.entries, {QStringLiteral("ids_name"), QStringLiteral("ids_info")});
        updateOrAppendEntry(&updated.entries, QStringLiteral("nickname"), it->nickname);
        if (!it->msgIdPrefix.trimmed().isEmpty())
            updateOrAppendEntry(&updated.entries, QStringLiteral("msg_id_prefix"), it->msgIdPrefix.trimmed());
        if (!it->equipment.trimmed().isEmpty())
            updateOrAppendEntry(&updated.entries, QStringLiteral("equipment"), it->equipment.trimmed());
        updateOrAppendEntry(&updated.entries, QStringLiteral("category"), QStringLiteral("commodity"));
        updateOrAppendEntry(&updated.entries, QStringLiteral("price"), QString::number(it->basePrice));
        updateOrAppendEntry(&updated.entries,
                            QStringLiteral("combinable"),
                            it->combinable ? QStringLiteral("true") : QStringLiteral("false"));
        if (it->goodSellPrice > 0.0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("good_sell_price"), QString::number(it->goodSellPrice, 'f', 6));
        if (it->badBuyPrice > 0.0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("bad_buy_price"), QString::number(it->badBuyPrice, 'f', 6));
        if (it->badSellPrice > 0.0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("bad_sell_price"), QString::number(it->badSellPrice, 'f', 6));
        if (it->goodBuyPrice > 0.0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("good_buy_price"), QString::number(it->goodBuyPrice, 'f', 6));
        if (!it->shopArchetype.trimmed().isEmpty())
            updateOrAppendEntry(&updated.entries, QStringLiteral("shop_archetype"), it->shopArchetype.trimmed());
        if (!it->itemIcon.trimmed().isEmpty())
            updateOrAppendEntry(&updated.entries, QStringLiteral("item_icon"), it->itemIcon.trimmed());
        if (it->jumpDist > 0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("jump_dist"), QString::number(it->jumpDist));
        updateOrAppendEntry(&updated.entries, QStringLiteral("volume"), QString::number(qMax(1, it->volume)));
        updatedGoods.append(updated);
        writtenCommoditySections.insert(nickname);
    }

    IniDocument newGoodSections;
    for (const auto &commodity : workspace.commodities) {
        const QString nickname = normalizedNickname(commodity.nickname);
        if (writtenCommoditySections.contains(nickname))
            continue;

        IniSection section;
        section.name = QStringLiteral("Good");
        section.entries = {
            {QStringLiteral("nickname"), commodity.nickname},
            {QStringLiteral("msg_id_prefix"), commodity.msgIdPrefix.trimmed().isEmpty()
                 ? QStringLiteral("gcs_gen_%1").arg(commodity.nickname)
                 : commodity.msgIdPrefix.trimmed()},
            {QStringLiteral("equipment"), commodity.equipment.trimmed().isEmpty()
                 ? commodity.nickname
                 : commodity.equipment.trimmed()},
            {QStringLiteral("category"), QStringLiteral("commodity")},
            {QStringLiteral("price"), QString::number(commodity.basePrice)},
            {QStringLiteral("combinable"), commodity.combinable ? QStringLiteral("true") : QStringLiteral("false")},
            {QStringLiteral("good_sell_price"), QString::number(commodity.goodSellPrice, 'f', 6)},
            {QStringLiteral("bad_buy_price"), QString::number(commodity.badBuyPrice, 'f', 6)},
            {QStringLiteral("bad_sell_price"), QString::number(commodity.badSellPrice, 'f', 6)},
            {QStringLiteral("good_buy_price"), QString::number(commodity.goodBuyPrice, 'f', 6)},
        };
        if (!commodity.shopArchetype.trimmed().isEmpty())
            section.entries.append({QStringLiteral("shop_archetype"), commodity.shopArchetype.trimmed()});
        if (!commodity.itemIcon.trimmed().isEmpty())
            section.entries.append({QStringLiteral("item_icon"), commodity.itemIcon.trimmed()});
        if (commodity.jumpDist > 0)
            section.entries.append({QStringLiteral("jump_dist"), QString::number(commodity.jumpDist)});
        section.entries.append({QStringLiteral("volume"), QString::number(qMax(1, commodity.volume))});
        newGoodSections.append(section);
    }

    if (!newGoodSections.isEmpty()) {
        int insertIndex = 0;
        for (int i = 0; i < updatedGoods.size(); ++i) {
            const auto &section = updatedGoods.at(i);
            if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0)
                continue;
            if (normalizedNickname(section.value(QStringLiteral("nickname"))).startsWith(QStringLiteral("commodity_")))
                insertIndex = i + 1;
        }

        for (int i = 0; i < newGoodSections.size(); ++i)
            updatedGoods.insert(insertIndex + i, newGoodSections.at(i));
    }

    QFile goodsFile(workspace.goodsFilePath);
    if (!goodsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not write %1").arg(workspace.goodsFilePath);
        return false;
    }
    goodsFile.write(IniParser::serialize(updatedGoods).toUtf8());
    goodsFile.close();

    QString selectEquipPath = workspace.selectEquipFilePath;
    if (selectEquipPath.isEmpty() && !workspace.dataPath.isEmpty()) {
        QDir equipmentDir(QDir(workspace.dataPath).filePath(QStringLiteral("EQUIPMENT")));
        if (!equipmentDir.exists())
            QDir().mkpath(equipmentDir.absolutePath());
        selectEquipPath = equipmentDir.filePath(QStringLiteral("select_equip.ini"));
    }
    if (selectEquipPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr("select_equip.ini was not found for the current Freelancer data path.");
        return false;
    }

    if (!writeSelectEquipPreservingComments(selectEquipPath, workspace.commodities, errorMessage))
        return false;

    IniDocument marketDoc = IniParser::parseFile(workspace.preferredMarketFilePath);
    QHash<QString, QVector<TradePriceRecord>> explicitPricesByBase;
    for (const auto &price : workspace.prices) {
        if (!shouldPersistMarketPrice(price))
            continue;
        explicitPricesByBase[normalizedNickname(price.baseNickname)].append(price);
    }

    IniDocument updatedMarket;
    QSet<QString> writtenBases;
    for (const auto &section : marketDoc) {
        if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0) {
            updatedMarket.append(section);
            continue;
        }

        const QString normalizedBase = normalizedNickname(section.value(QStringLiteral("base")));
        IniSection updated = section;
        QVector<IniEntry> preserved;
        for (const auto &entry : updated.entries) {
            if (entry.first.compare(QStringLiteral("MarketGood"), Qt::CaseInsensitive) != 0) {
                preserved.append(entry);
                continue;
            }
            const QString commodityNickname = entry.second.section(QLatin1Char(','), 0, 0).trimmed();
            if (!commodityNickname.startsWith(QStringLiteral("commodity_"), Qt::CaseInsensitive))
                preserved.append(entry);
        }
        updated.entries = preserved;

        const auto prices = explicitPricesByBase.value(normalizedBase);
        for (const auto &price : prices) {
            const int relationFlag = price.isSource ? 0 : 1;
            updated.entries.append({QStringLiteral("MarketGood"),
                                    QStringLiteral("%1, 0, 0, 0, 0, %2, %3")
                                        .arg(price.commodityNickname)
                                        .arg(relationFlag)
                                        .arg(QString::number(price.multiplier, 'f', 6))});
        }
        updatedMarket.append(updated);
        writtenBases.insert(normalizedBase);
    }

    for (auto it = explicitPricesByBase.constBegin(); it != explicitPricesByBase.constEnd(); ++it) {
        if (writtenBases.contains(it.key()))
            continue;
        IniSection section;
        section.name = QStringLiteral("BaseGood");
        section.entries.append({QStringLiteral("base"), it.value().isEmpty() ? it.key() : it.value().first().baseNickname});
        for (const auto &price : it.value()) {
            const int relationFlag = price.isSource ? 0 : 1;
            section.entries.append({QStringLiteral("MarketGood"),
                                    QStringLiteral("%1, 0, 0, 0, 0, %2, %3")
                                        .arg(price.commodityNickname)
                                        .arg(relationFlag)
                                        .arg(QString::number(price.multiplier, 'f', 6))});
        }
        updatedMarket.append(section);
    }

    QFile marketFile(workspace.preferredMarketFilePath);
    if (!marketFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not write %1").arg(workspace.preferredMarketFilePath);
        return false;
    }
    marketFile.write(IniParser::serialize(updatedMarket).toUtf8());
    marketFile.close();
    return true;
}

} // namespace flatlas::editors
