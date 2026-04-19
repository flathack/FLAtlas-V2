#pragma once
// editors/trade/MarketScanner.h – Market-Daten aus INIs extrahieren (Phase 11)

#include <QString>
#include <QVector>
#include "domain/TradeRoute.h"

namespace flatlas::editors {

class MarketScanner {
public:
    /// Scan a single market_*.ini file and extract buy/sell entries.
    static QVector<flatlas::domain::BaseMarketEntry> scanFile(const QString &filePath);

    /// Scan all market_*.ini files in a directory tree (recursive).
    static QVector<flatlas::domain::BaseMarketEntry> scanDirectory(const QString &dirPath);

    /// Scan from a Freelancer DATA directory (looks for equipment/market_*.ini etc.).
    static QVector<flatlas::domain::BaseMarketEntry> scanFreelancerData(const QString &dataPath);
};

} // namespace flatlas::editors
