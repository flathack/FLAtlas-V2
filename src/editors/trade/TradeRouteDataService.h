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
    QString idsInfoText;
    QString sourceFilePath;
    QString msgIdPrefix;
    QString equipment;
    bool combinable = true;
    double goodSellPrice = 0.0;
    double badBuyPrice = 0.0;
    double badSellPrice = 0.0;
    double goodBuyPrice = 0.0;
    QString shopArchetype;
    QString itemIcon;
    int jumpDist = 0;
    int unitsPerContainer = 30;
    QString podAppearance = QStringLiteral("cargopod_grey");
    QString lootAppearance = QStringLiteral("lootcrate_grey");
    double decayPerSecond = 0.0;
    int hitPts = 250;
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
    QString targetObjectNickname;
    QString kind;
    QVector3D position;
    QString objectDisplayName = {};
};

struct TradeLaneRecord {
    QString systemNickname;
    QStringList ringNicknames;
    QVector<QVector3D> ringPositions;
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
    QString selectEquipFilePath;
    QString preferredMarketFilePath;
    std::shared_ptr<flatlas::domain::UniverseData> universe;
    QVector<TradeCommodityRecord> commodities;
    QVector<TradeBaseRecord> bases;
    QVector<TradeJumpRecord> jumps;
    QVector<TradeLaneRecord> tradeLanes;
    QVector<TradePriceRecord> prices;
    double cruiseSpeed = 300.0;
};

class TradeRouteDataService
{
public:
    static TradeRouteWorkspaceData loadFromDataPath(const QString &dataPath);
    static bool saveWorkspace(const TradeRouteWorkspaceData &workspace, QString *errorMessage = nullptr);
    static QString fallbackCommodityDisplayName(const QString &nickname);
};

} // namespace flatlas::editors
