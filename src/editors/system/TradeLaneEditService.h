#pragma once

#include <QSet>
#include <QVector>
#include <QVector3D>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

namespace flatlas::domain {
class SolarObject;
class SystemDocument;
}

namespace flatlas::editors {

enum class TradeLaneChainIssue {
    None,
    NotTradeLane,
    MissingPrevRing,
    MissingNextRing,
    CyclicPrevLink,
    CyclicNextLink,
    TooFewRings,
};

struct TradeLaneChain {
    QVector<std::shared_ptr<flatlas::domain::SolarObject>> rings;
    QString systemNickname;
    int startNumber = 1;
    QString archetype = QStringLiteral("Trade_Lane_Ring");
    QString loadout;
    QString reputation;
    QString behavior = QStringLiteral("NOTHING");
    QString pilot = QStringLiteral("pilot_solar_easiest");
    int difficultyLevel = 1;
    int routeIdsName = 0;
    int startSpaceNameId = 0;
    int endSpaceNameId = 0;
    int idsInfo = 66170;
    QVector3D startPosition;
    QVector3D endPosition;
};

struct TradeLaneChainDetection {
    std::optional<TradeLaneChain> chain;
    TradeLaneChainIssue issue = TradeLaneChainIssue::None;
    QString referencedNickname;
    std::shared_ptr<flatlas::domain::SolarObject> boundaryRing;
    QString errorMessage;
};

struct TradeLaneEditRequest {
    QString systemNickname;
    int startNumber = 1;
    QVector3D startPosition;
    QVector3D endPosition;
    int ringCount = 2;
    QString archetype = QStringLiteral("Trade_Lane_Ring");
    QString loadout;
    QString reputation;
    QString behavior = QStringLiteral("NOTHING");
    QString pilot = QStringLiteral("pilot_solar_easiest");
    int difficultyLevel = 1;
    int routeIdsName = 0;
    int startSpaceNameId = 0;
    int endSpaceNameId = 0;
    int idsInfo = 66170;
};

class TradeLaneEditService {
public:
    static TradeLaneChainDetection inspectChain(const flatlas::domain::SystemDocument &document,
                                                const QString &selectedNickname);

    static std::optional<TradeLaneChain> detectChain(const flatlas::domain::SystemDocument &document,
                                                     const QString &selectedNickname,
                                                     QString *errorMessage = nullptr);

    static QStringList memberNicknames(const TradeLaneChain &chain);
    static double laneLengthMeters(const TradeLaneChain &chain);
    static double derivedSpacingMeters(const TradeLaneChain &chain);

    static bool buildReplacementObjects(const TradeLaneChain &existingChain,
                                        const TradeLaneEditRequest &request,
                                        const QSet<QString> &occupiedNicknames,
                                        QVector<std::shared_ptr<flatlas::domain::SolarObject>> *outObjects,
                                        QString *errorMessage = nullptr);
};

} // namespace flatlas::editors