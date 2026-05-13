#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::domain {

struct FactionRep {
    QString target;
    double value = 0.0;
};

struct EmpathyEvent {
    QString eventType;
    double value = 0.0;
};

struct EmpathyRate {
    QString target;
    double rate = 0.0;
};

struct FactionPropData {
    QString affiliation;
    QString legality;
    QString nicknamePlurality;
    QString msgIdPrefix;
    QString jumpPreference;
    QStringList npcShips;
    QStringList voices;
    QString mcCostume;
    QStringList spaceCostumes;
    QString firstnameMale;
    QString firstnameFemale;
    QString lastname;
    QString rankDesig;
    QString formationDesig;
    QString largeShipDesig;
    QString largeShipNames;
    QStringList scanForCargo;
    QString scanAnnounce;
    QString scanChance;
    QStringList formations;
};

struct Faction {
    QString nickname;
    QString idsName;
    QString idsInfo;
    QString idsShortName;
    QList<FactionRep> reputations;
    QList<EmpathyEvent> empathyEvents;
    QList<EmpathyRate> empathyRates;
    FactionPropData props;
    bool inInitialWorld = false;
    bool inEmpathy = false;
    bool inFactionProp = false;
};

enum class FactionValidationSeverity {
    Critical,
    Warning,
    Info,
};

struct FactionValidationIssue {
    FactionValidationSeverity severity = FactionValidationSeverity::Info;
    QString faction;
    QString message;
};

class FactionWorld {
public:
    static QString keyForNickname(const QString &nickname);

    QStringList sortedNicknames() const;
    bool contains(const QString &nickname) const;
    Faction *faction(const QString &nickname);
    const Faction *faction(const QString &nickname) const;
    void upsertFaction(const Faction &faction);
    bool removeFaction(const QString &nickname);
    bool deactivateFaction(const QString &nickname);

    void addFaction(const QString &nickname);
    double reputation(const QString &source, const QString &target, double fallback = 0.0) const;
    void setReputation(const QString &source, const QString &target, double value);
    double empathyRate(const QString &source, const QString &target, double fallback = 0.0) const;
    void setEmpathyRate(const QString &source, const QString &target, double value);

    QList<FactionValidationIssue> validate() const;

    QHash<QString, Faction> factionsByKey;
    QStringList declaredNicknames;
    QStringList initialWorldRepOrder;
    QStringList initialWorldPrefixSections;
    QVector<QPair<QString, QString>> lockedGateEntries;
};

} // namespace flatlas::domain
