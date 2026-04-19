// editors/trade/MarketScanner.cpp – Market-Daten aus INIs extrahieren (Phase 11)

#include "MarketScanner.h"
#include "infrastructure/parser/IniParser.h"

#include <QDir>
#include <QDirIterator>

using namespace flatlas::domain;
using namespace flatlas::infrastructure;

namespace flatlas::editors {

QVector<BaseMarketEntry> MarketScanner::scanFile(const QString &filePath)
{
    QVector<BaseMarketEntry> result;

    auto doc = IniParser::parseFile(filePath);
    if (doc.isEmpty())
        return result;

    // Market INIs have [BaseGood] sections with:
    //   base = <base_nickname>
    //   MarketGood = <commodity>, <rank_level>, <min_level>, <rep_required>,
    //                <price_modifier>, <sell_price_modifier>, <quantity>
    // Or simpler: MarketGood = <commodity>, <price>, ...
    for (const auto &sec : doc) {
        if (sec.name.compare(QStringLiteral("BaseGood"), Qt::CaseInsensitive) != 0)
            continue;

        QString base = sec.value(QStringLiteral("base"));
        if (base.isEmpty()) continue;

        for (const auto &mg : sec.values(QStringLiteral("MarketGood"))) {
            auto parts = mg.split(',');
            if (parts.size() < 2) continue;

            BaseMarketEntry entry;
            entry.base = base;
            entry.commodity = parts[0].trimmed();

            // Parse price from second field (rank/level or direct price)
            // In Freelancer format: commodity, rank_level, min_level, rep, price_mod, ...
            // The actual price depends on the good definition, but for scanning
            // we capture the price modifier or direct price
            if (parts.size() >= 5) {
                // Standard Freelancer format: good, rank, min_rank, rep, price_modifier
                float priceMod = parts[4].trimmed().toFloat();
                entry.price = static_cast<int>(priceMod);
                // sells = true if last field > 0
                int lastVal = parts.last().trimmed().toInt();
                entry.sells = (lastVal > 0);
            } else {
                // Simplified format: good, price
                entry.price = parts[1].trimmed().toInt();
                entry.sells = true;
            }

            result.append(entry);
        }
    }

    return result;
}

QVector<BaseMarketEntry> MarketScanner::scanDirectory(const QString &dirPath)
{
    QVector<BaseMarketEntry> result;

    QDirIterator it(dirPath, {QStringLiteral("market_*.ini")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(scanFile(it.next()));
    }

    return result;
}

QVector<BaseMarketEntry> MarketScanner::scanFreelancerData(const QString &dataPath)
{
    QVector<BaseMarketEntry> result;

    // Freelancer stores market data in various subdirectories
    QStringList searchDirs = {
        dataPath,
        dataPath + QStringLiteral("/equipment"),
        dataPath + QStringLiteral("/ships"),
    };

    for (const auto &dir : searchDirs) {
        if (QDir(dir).exists()) {
            QDirIterator it(dir, {QStringLiteral("market_*.ini"),
                                   QStringLiteral("*_market.ini")},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                result.append(scanFile(it.next()));
            }
        }
    }

    return result;
}

} // namespace flatlas::editors
