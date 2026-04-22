#include "TradeRouteDataService.h"

#include "core/PathUtils.h"
#include "editors/universe/UniverseSerializer.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>

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
            const QVector3D position = parsePos(section.value(QStringLiteral("pos")));

            if (!baseNickname.isEmpty()) {
                TradeBaseRecord base;
                base.nickname = baseNickname;
                base.displayName = objectNickname.isEmpty() ? baseNickname : objectNickname;
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
            jumps->append(jump);
        }
    }
}

QVector<TradeCommodityRecord> loadCommodities(const QString &goodsFilePath)
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
        commodity.displayName = TradeRouteDataService::fallbackCommodityDisplayName(nickname);
        commodity.basePrice = section.value(QStringLiteral("price")).toInt();
        commodity.volume = qMax(1, section.value(QStringLiteral("volume"), QStringLiteral("1")).toInt());
        commodity.idsName = section.value(QStringLiteral("ids_name")).toInt();
        commodity.idsInfo = section.value(QStringLiteral("ids_info")).toInt();
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
    workspace.preferredMarketFilePath = preferredMarketFile(dataPath);

    const QString universeIni = resolveUniverseIni(dataPath);
    if (!universeIni.isEmpty()) {
        auto loadedUniverse = UniverseSerializer::load(universeIni);
        if (loadedUniverse)
            workspace.universe = std::shared_ptr<UniverseData>(loadedUniverse.release());
    }
    if (!workspace.universe)
        workspace.universe = std::make_shared<UniverseData>();

    workspace.commodities = loadCommodities(workspace.goodsFilePath);

    QHash<QString, TradeBaseRecord> bases;
    scanSystemObjects(dataPath, workspace.universe, &bases, &workspace.jumps);
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
        updateOrAppendEntry(&updated.entries, QStringLiteral("nickname"), it->nickname);
        updateOrAppendEntry(&updated.entries, QStringLiteral("category"), QStringLiteral("commodity"));
        updateOrAppendEntry(&updated.entries, QStringLiteral("price"), QString::number(it->basePrice));
        updateOrAppendEntry(&updated.entries, QStringLiteral("volume"), QString::number(qMax(1, it->volume)));
        if (it->idsName > 0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("ids_name"), QString::number(it->idsName));
        if (it->idsInfo > 0)
            updateOrAppendEntry(&updated.entries, QStringLiteral("ids_info"), QString::number(it->idsInfo));
        updatedGoods.append(updated);
        writtenCommoditySections.insert(nickname);
    }

    for (const auto &commodity : workspace.commodities) {
        const QString nickname = normalizedNickname(commodity.nickname);
        if (writtenCommoditySections.contains(nickname))
            continue;

        IniSection section;
        section.name = QStringLiteral("Good");
        section.entries = {
            {QStringLiteral("nickname"), commodity.nickname},
            {QStringLiteral("category"), QStringLiteral("commodity")},
            {QStringLiteral("price"), QString::number(commodity.basePrice)},
            {QStringLiteral("combinable"), QStringLiteral("true")},
            {QStringLiteral("volume"), QString::number(qMax(1, commodity.volume))},
        };
        if (commodity.idsName > 0)
            section.entries.append({QStringLiteral("ids_name"), QString::number(commodity.idsName)});
        if (commodity.idsInfo > 0)
            section.entries.append({QStringLiteral("ids_info"), QString::number(commodity.idsInfo)});
        updatedGoods.append(section);
    }

    QFile goodsFile(workspace.goodsFilePath);
    if (!goodsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not write %1").arg(workspace.goodsFilePath);
        return false;
    }
    goodsFile.write(IniParser::serialize(updatedGoods).toUtf8());
    goodsFile.close();

    IniDocument marketDoc = IniParser::parseFile(workspace.preferredMarketFilePath);
    QHash<QString, QVector<TradePriceRecord>> explicitPricesByBase;
    for (const auto &price : workspace.prices) {
        if (price.implicit)
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