#pragma once

#include <QHash>
#include <QString>
#include <QVector>
#include <QVector3D>

#include <memory>

#include "domain/UniverseData.h"

namespace flatlas::editors {

struct TradeCommodityRecord {
    QString nickname;
    QString displayName;
    int basePrice = 0;
    int volume = 1;
    int idsName = 0;
    int idsInfo = 0;
    QString sourceFilePath;
};

struct TradeBaseRecord {
    QString nickname;
    QString displayName;
    QString systemNickname;
    QString systemDisplayName;
    QVector3D position;
};

struct TradeJumpRecord {
    QString objectNickname;
    QString systemNickname;
    QString targetSystemNickname;
    QString kind;
    QVector3D position;
};

struct TradePriceRecord {
    QString baseNickname;
    QString baseDisplayName;
    QString systemNickname;
    QString commodityNickname;
    int price = 0;
    double multiplier = 1.0;
    bool isSource = false;
    bool implicit = false;
    QString sourceFilePath;
};

struct TradeRouteWorkspaceData {
    QString dataPath;
    QString goodsFilePath;
    QString preferredMarketFilePath;
    std::shared_ptr<flatlas::domain::UniverseData> universe;
    QVector<TradeCommodityRecord> commodities;
    QVector<TradeBaseRecord> bases;
    QVector<TradeJumpRecord> jumps;
    QVector<TradePriceRecord> prices;
};

class TradeRouteDataService
{
public:
    static TradeRouteWorkspaceData loadFromDataPath(const QString &dataPath);
    static bool saveWorkspace(const TradeRouteWorkspaceData &workspace, QString *errorMessage = nullptr);
    static QString fallbackCommodityDisplayName(const QString &nickname);
};

} // namespace flatlas::editors