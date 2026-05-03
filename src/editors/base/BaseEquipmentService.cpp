#include "BaseEquipmentService.h"

#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QSet>
#include <algorithm>

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniEntry;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

namespace flatlas::editors {
namespace {

QString normalized(const QString &value)
{
    return value.trimmed().toLower();
}

QString dataPathForSystemFile(const QString &systemFilePath)
{
    QDir dir(QFileInfo(systemFilePath).absoluteDir());
    while (!dir.path().isEmpty() && dir.exists()) {
        if (dir.dirName().compare(QStringLiteral("DATA"), Qt::CaseInsensitive) == 0)
            return dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    return {};
}

QString findFirstMarketFile(const QString &dataPath, const QString &preferredRelativePath, const QString &name)
{
    const QString preferred = flatlas::core::PathUtils::ciResolvePath(dataPath, preferredRelativePath);
    if (!preferred.isEmpty())
        return preferred;

    QDirIterator it(dataPath, {name}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext())
        return it.next();
    return QDir(dataPath).filePath(preferredRelativePath);
}

IdsStringTable loadIds(const QString &dataPath)
{
    IdsStringTable ids;
    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(QFileInfo(dataPath).absolutePath(), QStringLiteral("EXE"));
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);
    return ids;
}

QString prettyNickname(QString nickname)
{
    nickname = nickname.trimmed();
    const QStringList prefixes = {
        QStringLiteral("commodity_"),
        QStringLiteral("package_"),
    };
    for (const QString &prefix : prefixes) {
        if (nickname.startsWith(prefix, Qt::CaseInsensitive)) {
            nickname = nickname.mid(prefix.size());
            break;
        }
    }
    QStringList words;
    for (const QString &part : nickname.split(QLatin1Char('_'), Qt::SkipEmptyParts))
        words.append(part.left(1).toUpper() + part.mid(1));
    return words.isEmpty() ? nickname : words.join(QLatin1Char(' '));
}

QHash<QString, QString> loadShipDisplayNames(const QString &dataPath, const IdsStringTable &ids)
{
    QHash<QString, QString> names;
    const QString shiparchPath = flatlas::core::PathUtils::ciResolvePath(dataPath, QStringLiteral("SHIPS/shiparch.ini"));
    if (shiparchPath.isEmpty())
        return names;

    const IniDocument doc = IniParser::parseFile(shiparchPath);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Ship"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.isEmpty())
            continue;
        bool ok = false;
        const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
        const QString display = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
        names.insert(normalized(nickname), display.isEmpty() ? prettyNickname(nickname) : display);
    }
    return names;
}

QHash<QString, QString> loadHullShipNicknames(const QString &dataPath)
{
    QHash<QString, QString> hullShips;
    const QVector<QString> sources = {
        QStringLiteral("EQUIPMENT/goods.ini"),
        QStringLiteral("EQUIPMENT/engine_good.ini"),
        QStringLiteral("EQUIPMENT/misc_good.ini"),
        QStringLiteral("EQUIPMENT/st_good.ini"),
        QStringLiteral("EQUIPMENT/weapon_good.ini"),
    };

    for (const QString &source : sources) {
        const QString goodsPath = flatlas::core::PathUtils::ciResolvePath(dataPath, source);
        if (goodsPath.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(goodsPath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0)
                continue;
            const QString category = section.value(QStringLiteral("category")).trimmed();
            if (category.compare(QStringLiteral("shiphull"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString ship = section.value(QStringLiteral("ship")).trimmed();
            if (!nickname.isEmpty() && !ship.isEmpty())
                hullShips.insert(normalized(nickname), ship);
        }
    }
    return hullShips;
}

void loadGoodOptions(const QString &dataPath,
                     QVector<BaseEquipmentOption> *equipment,
                     QVector<BaseEquipmentOption> *commodities,
                     QVector<BaseEquipmentOption> *ships)
{
    equipment->clear();
    commodities->clear();
    ships->clear();

    const IdsStringTable ids = loadIds(dataPath);
    const QHash<QString, QString> shipNames = loadShipDisplayNames(dataPath, ids);
    const QHash<QString, QString> hullShips = loadHullShipNicknames(dataPath);
    const QVector<QPair<QString, QString>> sources = {
        {QStringLiteral("EQUIPMENT/weapon_good.ini"), QObject::tr("Weapons")},
        {QStringLiteral("EQUIPMENT/st_good.ini"), QObject::tr("Shields / Thrusters")},
        {QStringLiteral("EQUIPMENT/misc_good.ini"), QObject::tr("Misc")},
        {QStringLiteral("EQUIPMENT/goods.ini"), QObject::tr("General")},
        {QStringLiteral("EQUIPMENT/engine_good.ini"), QObject::tr("Engines")},
    };
    QSet<QString> seenEquipment;
    QSet<QString> seenCommodities;
    QSet<QString> seenShips;
    for (const auto &source : sources) {
        const QString goodsPath = flatlas::core::PathUtils::ciResolvePath(dataPath, source.first);
        if (goodsPath.isEmpty())
            continue;

        const IniDocument doc = IniParser::parseFile(goodsPath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Good"), Qt::CaseInsensitive) != 0)
                continue;

            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            const QString category = section.value(QStringLiteral("category")).trimmed();
            const QString shipNickname = section.value(QStringLiteral("ship")).trimmed();
            const QString hullNickname = section.value(QStringLiteral("hull")).trimmed();
            const bool isShipPackage = nickname.endsWith(QStringLiteral("_package"), Qt::CaseInsensitive)
                && category.compare(QStringLiteral("ship"), Qt::CaseInsensitive) == 0;

            bool ok = false;
            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
            QString ingameName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
            QString packageShipNickname = shipNickname;
            if (isShipPackage && packageShipNickname.isEmpty() && !hullNickname.isEmpty())
                packageShipNickname = hullShips.value(normalized(hullNickname));
            if (isShipPackage && !packageShipNickname.isEmpty()) {
                const QString shipDisplay = shipNames.value(normalized(packageShipNickname));
                if (!shipDisplay.isEmpty())
                    ingameName = shipDisplay;
            }
            if (ingameName.isEmpty())
                ingameName = prettyNickname(isShipPackage && !packageShipNickname.isEmpty() ? packageShipNickname : nickname);

            BaseEquipmentOption option;
            option.nickname = nickname;
            option.ingameName = ingameName;
            option.displayLabel = BaseEquipmentService::displayLabel(nickname, ingameName);
            option.groupLabel = source.second;
            bool priceOk = false;
            option.price = section.value(QStringLiteral("price")).trimmed().toInt(&priceOk);
            if (!priceOk)
                option.price = 0;

            const QString key = normalized(nickname);
            if (isShipPackage) {
                if (!seenShips.contains(key)) {
                    seenShips.insert(key);
                    ships->append(option);
                }
            } else if (category.compare(QStringLiteral("commodity"), Qt::CaseInsensitive) == 0 && !seenCommodities.contains(key)) {
                seenCommodities.insert(key);
                option.groupLabel = QObject::tr("General");
                commodities->append(option);
            } else if (category.compare(QStringLiteral("equipment"), Qt::CaseInsensitive) == 0 && !seenEquipment.contains(key)) {
                seenEquipment.insert(key);
                equipment->append(option);
            }
        }
    }

    auto sorter = [](const BaseEquipmentOption &left, const BaseEquipmentOption &right) {
        return left.displayLabel.toLower() < right.displayLabel.toLower();
    };
    std::sort(equipment->begin(), equipment->end(), sorter);
    std::sort(commodities->begin(), commodities->end(), sorter);
    std::sort(ships->begin(), ships->end(), sorter);
}

QSet<QString> optionNicknameSet(const QVector<BaseEquipmentOption> &options)
{
    QSet<QString> values;
    for (const BaseEquipmentOption &option : options)
        values.insert(normalized(option.nickname));
    return values;
}

QVector<QStringList> loadMarketRowsForBase(const QString &filePath,
                                           const QString &baseNickname,
                                           const QSet<QString> &allowedGoods)
{
    QVector<QStringList> rows;
    if (filePath.isEmpty() || !QFile::exists(filePath))
        return rows;

    const IniDocument doc = IniParser::parseFile(filePath);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0)
            continue;
        if (section.value(QStringLiteral("base")).trimmed().compare(baseNickname, Qt::CaseInsensitive) != 0)
            continue;

        for (const QString &line : section.values(QStringLiteral("MarketGood"))) {
            const QString nickname = line.section(QLatin1Char(','), 0, 0).trimmed();
            if (!allowedGoods.contains(normalized(nickname)))
                continue;
            QStringList values;
            for (const QString &part : line.split(QLatin1Char(',')))
                values.append(part.trimmed());
            bool duplicate = false;
            for (const QStringList &row : rows) {
                if (!row.isEmpty() && row.constFirst().compare(nickname, Qt::CaseInsensitive) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                rows.append(values);
        }
        break;
    }
    return rows;
}

QStringList marketNicknames(const QVector<QStringList> &rows)
{
    QStringList out;
    for (const QStringList &row : rows) {
        if (!row.isEmpty())
            out.append(row.constFirst().trimmed());
    }
    return out;
}

QStringList marketShipLevels(const QVector<QStringList> &rows)
{
    QStringList out;
    for (const QStringList &row : rows)
        out.append(row.value(1, QStringLiteral("1")).trimmed().isEmpty() ? QStringLiteral("1") : row.value(1).trimmed());
    return out;
}

QString defaultMarketGoodLine(const QString &nickname, const QString &level = QStringLiteral("0"))
{
    return QStringLiteral("%1, %2, -1, 1, 1, 0, 1, 1").arg(nickname.trimmed(), level.trimmed().isEmpty() ? QStringLiteral("0") : level.trimmed());
}

QString marketGoodLine(const QString &nickname, const QString &level, const QHash<QString, QStringList> &existingRows)
{
    QStringList values = existingRows.value(normalized(nickname));
    if (values.isEmpty())
        values = defaultMarketGoodLine(nickname, level).split(QLatin1Char(','));
    while (values.size() < 8)
        values.append(values.size() == 2 ? QStringLiteral("-1") : QStringLiteral("1"));
    values[0] = nickname.trimmed();
    values[1] = level.trimmed().isEmpty() ? QStringLiteral("0") : level.trimmed();
    for (QString &value : values)
        value = value.trimmed();
    return values.join(QStringLiteral(", "));
}

QString updatedMarketText(const QString &filePath,
                          const QString &baseNickname,
                          const QStringList &goods,
                          const QStringList &levels,
                          const QSet<QString> &managedGoods)
{
    IniDocument doc = QFile::exists(filePath) ? IniParser::parseFile(filePath) : IniDocument();
    IniDocument updated;
    bool wroteBase = false;
    QHash<QString, QStringList> existingRows;

    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) == 0
            && section.value(QStringLiteral("base")).trimmed().compare(baseNickname, Qt::CaseInsensitive) == 0) {
            for (const QString &line : section.values(QStringLiteral("MarketGood"))) {
                QStringList values;
                for (const QString &part : line.split(QLatin1Char(',')))
                    values.append(part.trimmed());
                if (!values.isEmpty())
                    existingRows.insert(normalized(values.constFirst()), values);
            }
        }
        if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0
            || section.value(QStringLiteral("base")).trimmed().compare(baseNickname, Qt::CaseInsensitive) != 0) {
            updated.append(section);
            continue;
        }

        IniSection out = section;
        QVector<IniEntry> preserved;
        for (const IniEntry &entry : out.entries) {
            if (entry.first.compare(QStringLiteral("MarketGood"), Qt::CaseInsensitive) != 0) {
                preserved.append(entry);
                continue;
            }
            const QString existingGood = entry.second.section(QLatin1Char(','), 0, 0).trimmed();
            if (!managedGoods.contains(normalized(existingGood)))
                preserved.append(entry);
        }
        out.entries = preserved;
        for (int index = 0; index < goods.size(); ++index)
            out.entries.append({QStringLiteral("MarketGood"), marketGoodLine(goods.at(index), levels.value(index), existingRows)});
        updated.append(out);
        wroteBase = true;
    }

    if (!wroteBase && !goods.isEmpty()) {
        IniSection section;
        section.name = QStringLiteral("BaseGood");
        section.entries.append({QStringLiteral("base"), baseNickname});
        for (int index = 0; index < goods.size(); ++index)
            section.entries.append({QStringLiteral("MarketGood"), marketGoodLine(goods.at(index), levels.value(index), existingRows)});
        updated.append(section);
    }

    return IniParser::serialize(updated);
}

QString updatedMarketTextFromRows(const QString &filePath,
                                  const QString &baseNickname,
                                  const QVector<QStringList> &rows,
                                  const QSet<QString> &managedGoods)
{
    QStringList goods;
    for (const QStringList &row : rows) {
        const QString nickname = row.value(0).trimmed();
        if (!nickname.isEmpty() && !goods.contains(nickname, Qt::CaseInsensitive))
            goods.append(nickname);
    }

    IniDocument doc = QFile::exists(filePath) ? IniParser::parseFile(filePath) : IniDocument();
    IniDocument updated;
    bool wroteBase = false;
    auto appendRows = [&](IniSection *section) {
        for (const QStringList &row : rows) {
            QStringList values = row;
            if (values.isEmpty() || values.first().trimmed().isEmpty())
                continue;
            while (values.size() < 7)
                values.append(values.size() == 2 ? QStringLiteral("-1") : QStringLiteral("0"));
            for (QString &value : values)
                value = value.trimmed();
            section->entries.append({QStringLiteral("MarketGood"), values.mid(0, 7).join(QStringLiteral(", "))});
        }
    };

    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0
            || section.value(QStringLiteral("base")).trimmed().compare(baseNickname, Qt::CaseInsensitive) != 0) {
            updated.append(section);
            continue;
        }

        IniSection out = section;
        QVector<IniEntry> preserved;
        for (const IniEntry &entry : out.entries) {
            if (entry.first.compare(QStringLiteral("MarketGood"), Qt::CaseInsensitive) != 0) {
                preserved.append(entry);
                continue;
            }
            const QString existingGood = entry.second.section(QLatin1Char(','), 0, 0).trimmed();
            if (!managedGoods.contains(normalized(existingGood)))
                preserved.append(entry);
        }
        out.entries = preserved;
        appendRows(&out);
        updated.append(out);
        wroteBase = true;
    }

    if (!wroteBase && !rows.isEmpty()) {
        IniSection section;
        section.name = QStringLiteral("BaseGood");
        section.entries.append({QStringLiteral("base"), baseNickname});
        appendRows(&section);
        updated.append(section);
    }
    return IniParser::serialize(updated);
}

bool writeMarketFile(const QString &filePath, const QString &content, QString *errorMessage)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = QObject::tr("Could not write %1").arg(filePath);
        return false;
    }
    file.write(content.toUtf8());
    return true;
}

QStringList uniqueTrimmed(const QStringList &values, int maxCount = -1)
{
    QStringList out;
    for (const QString &value : values) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty() || out.contains(trimmed, Qt::CaseInsensitive))
            continue;
        out.append(trimmed);
        if (maxCount > 0 && out.size() >= maxCount)
            break;
    }
    return out;
}

} // namespace

QString BaseEquipmentService::displayLabel(const QString &nickname, const QString &ingameName)
{
    const QString cleanNickname = nickname.trimmed();
    const QString cleanName = ingameName.trimmed().isEmpty() ? prettyNickname(cleanNickname) : ingameName.trimmed();
    return QStringLiteral("%1 - %2").arg(cleanNickname, cleanName);
}

BaseEquipmentState BaseEquipmentService::load(const QString &systemFilePath, const QString &baseNickname)
{
    BaseEquipmentState state;
    state.dataPath = dataPathForSystemFile(systemFilePath);
    if (state.dataPath.isEmpty()) {
        state.warningMessage = QObject::tr("Could not resolve Freelancer DATA path for equipment and ship markets.");
        return state;
    }

    state.equipmentMarketFilePath = findFirstMarketFile(state.dataPath,
                                                        QStringLiteral("EQUIPMENT/market_misc.ini"),
                                                        QStringLiteral("market_misc.ini"));
    state.commodityMarketFilePath = findFirstMarketFile(state.dataPath,
                                                        QStringLiteral("EQUIPMENT/market_commodities.ini"),
                                                        QStringLiteral("market_commodities.ini"));
    state.shipMarketFilePath = findFirstMarketFile(state.dataPath,
                                                   QStringLiteral("EQUIPMENT/market_ships.ini"),
                                                   QStringLiteral("market_ships.ini"));

    loadGoodOptions(state.dataPath, &state.equipmentOptions, &state.commodityOptions, &state.shipPackageOptions);

    const QSet<QString> equipmentSet = optionNicknameSet(state.equipmentOptions);
    const QSet<QString> commoditySet = optionNicknameSet(state.commodityOptions);
    const QSet<QString> shipSet = optionNicknameSet(state.shipPackageOptions);

    state.equipment = marketNicknames(loadMarketRowsForBase(state.equipmentMarketFilePath, baseNickname, equipmentSet));
    state.commodityMarketRows = loadMarketRowsForBase(state.commodityMarketFilePath, baseNickname, commoditySet);
    state.commodities = marketNicknames(state.commodityMarketRows);
    const QVector<QStringList> shipRows = loadMarketRowsForBase(state.shipMarketFilePath, baseNickname, shipSet);
    state.shipPackages = uniqueTrimmed(marketNicknames(shipRows), MaxShipsPerBase);
    state.shipPackageLevels = marketShipLevels(shipRows).mid(0, state.shipPackages.size());
    return state;
}

bool BaseEquipmentService::save(const QString &systemFilePath,
                                const QString &baseNickname,
                                const QStringList &equipment,
                                const QStringList &commodities,
                                const QStringList &shipPackages,
                                QString *errorMessage)
{
    return save(systemFilePath, baseNickname, equipment, commodities, shipPackages, {}, errorMessage);
}

bool BaseEquipmentService::save(const QString &systemFilePath,
                                const QString &baseNickname,
                                const QStringList &equipment,
                                const QStringList &commodities,
                                const QStringList &shipPackages,
                                const QStringList &shipPackageLevels,
                                QString *errorMessage)
{
    const QVector<BaseEquipmentStagedWrite> writes = stagedWrites(systemFilePath,
                                                                  baseNickname,
                                                                  equipment,
                                                                  commodities,
                                                                  shipPackages,
                                                                  shipPackageLevels,
                                                                  errorMessage);
    if (writes.isEmpty())
        return errorMessage ? errorMessage->trimmed().isEmpty() : true;

    for (const BaseEquipmentStagedWrite &write : writes) {
        if (!writeMarketFile(write.absolutePath, write.content, errorMessage))
            return false;
    }
    return true;
}

QVector<BaseEquipmentStagedWrite> BaseEquipmentService::stagedWrites(const QString &systemFilePath,
                                                                     const QString &baseNickname,
                                                                     const QStringList &equipment,
                                                                     const QStringList &commodities,
                                                                     const QStringList &shipPackages,
                                                                     QString *errorMessage)
{
    return stagedWrites(systemFilePath, baseNickname, equipment, commodities, shipPackages, {}, errorMessage);
}

QVector<BaseEquipmentStagedWrite> BaseEquipmentService::stagedWrites(const QString &systemFilePath,
                                                                     const QString &baseNickname,
                                                                     const QStringList &equipment,
                                                                     const QStringList &commodities,
                                                                     const QStringList &shipPackages,
                                                                     const QStringList &shipPackageLevels,
                                                                     QString *errorMessage)
{
    QVector<QStringList> commodityRows;
    for (const QString &commodity : commodities)
        commodityRows.append({commodity, QStringLiteral("0"), QStringLiteral("-1"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")});
    return stagedWrites(systemFilePath, baseNickname, equipment, commodityRows, shipPackages, shipPackageLevels, errorMessage);
}

QVector<BaseEquipmentStagedWrite> BaseEquipmentService::stagedWrites(const QString &systemFilePath,
                                                                     const QString &baseNickname,
                                                                     const QStringList &equipment,
                                                                     const QVector<QStringList> &commodityMarketRows,
                                                                     const QStringList &shipPackages,
                                                                     const QStringList &shipPackageLevels,
                                                                     QString *errorMessage)
{
    QVector<BaseEquipmentStagedWrite> writes;
    BaseEquipmentState state = load(systemFilePath, baseNickname);
    if (state.dataPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = state.warningMessage;
        return writes;
    }

    const QStringList cleanEquipment = uniqueTrimmed(equipment);
    QVector<QStringList> cleanCommodityRows;
    QSet<QString> seenCommodityRows;
    for (const QStringList &row : commodityMarketRows) {
        const QString nickname = row.value(0).trimmed();
        if (nickname.isEmpty() || seenCommodityRows.contains(normalized(nickname)))
            continue;
        seenCommodityRows.insert(normalized(nickname));
        QStringList values = row;
        while (values.size() < 7)
            values.append(values.size() == 2 ? QStringLiteral("-1") : QStringLiteral("0"));
        values[0] = nickname;
        if (values.value(6).trimmed().isEmpty())
            values[6] = QStringLiteral("1");
        cleanCommodityRows.append(values.mid(0, 7));
    }
    const QStringList cleanShips = uniqueTrimmed(shipPackages, MaxShipsPerBase);
    QStringList cleanShipLevels;
    for (int index = 0; index < cleanShips.size(); ++index) {
        bool ok = false;
        const int level = shipPackageLevels.value(index, QStringLiteral("1")).trimmed().toInt(&ok);
        cleanShipLevels.append(QString::number(ok ? std::clamp(level, 0, 100) : 1));
    }
    if (uniqueTrimmed(shipPackages).size() > MaxShipsPerBase) {
        if (errorMessage)
            *errorMessage = QObject::tr("Freelancer supports at most 3 ships per base.");
        return writes;
    }

    const QSet<QString> equipmentSet = optionNicknameSet(state.equipmentOptions);
    const QSet<QString> commoditySet = optionNicknameSet(state.commodityOptions);
    const QSet<QString> shipSet = optionNicknameSet(state.shipPackageOptions);

    writes.append({state.equipmentMarketFilePath,
                   updatedMarketText(state.equipmentMarketFilePath, baseNickname, cleanEquipment, {}, equipmentSet)});
    writes.append({state.commodityMarketFilePath,
                   updatedMarketTextFromRows(state.commodityMarketFilePath, baseNickname, cleanCommodityRows, commoditySet)});
    writes.append({state.shipMarketFilePath,
                   updatedMarketText(state.shipMarketFilePath, baseNickname, cleanShips, cleanShipLevels, shipSet)});
    return writes;
}

} // namespace flatlas::editors
