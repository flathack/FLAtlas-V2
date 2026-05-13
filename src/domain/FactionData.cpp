#include "FactionData.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <QSet>

namespace flatlas::domain {

namespace {

double clampRep(double value)
{
    return std::clamp(value, -1.0, 1.0);
}

bool hasDuplicateTarget(const auto &items)
{
    QHash<QString, int> counts;
    for (const auto &item : items)
        ++counts[FactionWorld::keyForNickname(item.target)];
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (!it.key().isEmpty() && it.value() > 1)
            return true;
    }
    return false;
}

bool containsNicknameKey(const QStringList &nicknames, const QString &nickname)
{
    const QString key = FactionWorld::keyForNickname(nickname);
    return std::any_of(nicknames.cbegin(), nicknames.cend(), [&](const QString &candidate) {
        return FactionWorld::keyForNickname(candidate) == key;
    });
}

QStringList reputationTargetKeys(const QList<FactionRep> &reputations)
{
    QStringList keys;
    keys.reserve(reputations.size());
    for (const FactionRep &rep : reputations)
        keys.append(FactionWorld::keyForNickname(rep.target));
    return keys;
}

QStringList expectedReputationTargetKeys(const QList<FactionRep> &reputations,
                                         const QStringList &initialWorldRepOrder)
{
    QStringList expected;
    QSet<QString> presentKeys;
    for (const FactionRep &rep : reputations)
        presentKeys.insert(FactionWorld::keyForNickname(rep.target));

    QSet<QString> emittedKeys;
    for (const QString &target : initialWorldRepOrder) {
        const QString key = FactionWorld::keyForNickname(target);
        if (presentKeys.contains(key) && !emittedKeys.contains(key)) {
            expected.append(key);
            emittedKeys.insert(key);
        }
    }
    for (const FactionRep &rep : reputations) {
        const QString key = FactionWorld::keyForNickname(rep.target);
        if (!emittedKeys.contains(key)) {
            expected.append(key);
            emittedKeys.insert(key);
        }
    }
    return expected;
}

} // namespace

QString FactionWorld::keyForNickname(const QString &nickname)
{
    return nickname.trimmed().toLower();
}

QStringList FactionWorld::sortedNicknames() const
{
    QStringList values;
    values.reserve(factionsByKey.size());
    for (const Faction &faction : factionsByKey)
        values.append(faction.nickname);
    values.sort(Qt::CaseInsensitive);
    return values;
}

bool FactionWorld::contains(const QString &nickname) const
{
    return factionsByKey.contains(keyForNickname(nickname));
}

Faction *FactionWorld::faction(const QString &nickname)
{
    auto it = factionsByKey.find(keyForNickname(nickname));
    return it == factionsByKey.end() ? nullptr : &it.value();
}

const Faction *FactionWorld::faction(const QString &nickname) const
{
    auto it = factionsByKey.constFind(keyForNickname(nickname));
    return it == factionsByKey.cend() ? nullptr : &it.value();
}

void FactionWorld::upsertFaction(const Faction &faction)
{
    if (faction.nickname.trimmed().isEmpty())
        return;
    if (!declaredNicknames.contains(faction.nickname, Qt::CaseSensitive))
        declaredNicknames.append(faction.nickname);
    factionsByKey.insert(keyForNickname(faction.nickname), faction);
}

bool FactionWorld::removeFaction(const QString &nickname)
{
    const QString key = keyForNickname(nickname);
    if (!factionsByKey.remove(key))
        return false;
    declaredNicknames.removeIf([&](const QString &declared) {
        return keyForNickname(declared) == key;
    });
    for (Faction &faction : factionsByKey) {
        faction.reputations.erase(std::remove_if(faction.reputations.begin(), faction.reputations.end(),
            [&](const FactionRep &rep) { return keyForNickname(rep.target) == key; }), faction.reputations.end());
        faction.empathyRates.erase(std::remove_if(faction.empathyRates.begin(), faction.empathyRates.end(),
            [&](const EmpathyRate &rate) { return keyForNickname(rate.target) == key; }), faction.empathyRates.end());
    }
    return true;
}

bool FactionWorld::deactivateFaction(const QString &nickname)
{
    Faction *item = faction(nickname);
    if (!item)
        return false;
    item->inInitialWorld = false;
    item->inEmpathy = false;
    item->inFactionProp = false;
    const QString key = keyForNickname(nickname);
    for (Faction &other : factionsByKey) {
        if (keyForNickname(other.nickname) == key)
            continue;
        other.reputations.erase(std::remove_if(other.reputations.begin(), other.reputations.end(),
            [&](const FactionRep &rep) { return keyForNickname(rep.target) == key; }), other.reputations.end());
        other.empathyRates.erase(std::remove_if(other.empathyRates.begin(), other.empathyRates.end(),
            [&](const EmpathyRate &rate) { return keyForNickname(rate.target) == key; }), other.empathyRates.end());
    }
    return true;
}

void FactionWorld::addFaction(const QString &nickname)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty() || contains(trimmed))
        return;

    Faction faction;
    faction.nickname = trimmed;
    faction.inInitialWorld = true;
    faction.inEmpathy = true;
    faction.inFactionProp = true;
    faction.props.affiliation = trimmed;
    faction.props.legality = QStringLiteral("lawful");
    faction.empathyEvents = {
        {QStringLiteral("object_destruction"), 0.0},
        {QStringLiteral("random_mission_success"), 0.0},
        {QStringLiteral("random_mission_failure"), 0.0},
        {QStringLiteral("random_mission_abortion"), 0.0},
    };
    faction.reputations.append({trimmed, 0.91});
    for (const QString &other : sortedNicknames()) {
        faction.reputations.append({other, 0.0});
        faction.empathyRates.append({other, 0.0});
        setReputation(other, trimmed, 0.0);
        setEmpathyRate(other, trimmed, 0.0);
    }
    upsertFaction(faction);
    if (!containsNicknameKey(initialWorldRepOrder, trimmed))
        initialWorldRepOrder.append(trimmed);
}

double FactionWorld::reputation(const QString &source, const QString &target, double fallback) const
{
    const Faction *item = faction(source);
    if (!item)
        return fallback;
    const QString targetKey = keyForNickname(target);
    for (const FactionRep &rep : item->reputations) {
        if (keyForNickname(rep.target) == targetKey)
            return rep.value;
    }
    return fallback;
}

void FactionWorld::setReputation(const QString &source, const QString &target, double value)
{
    Faction *item = faction(source);
    if (!item || target.trimmed().isEmpty())
        return;
    const QString targetKey = keyForNickname(target);
    for (FactionRep &rep : item->reputations) {
        if (keyForNickname(rep.target) == targetKey) {
            rep.value = clampRep(value);
            return;
        }
    }
    const QString trimmedTarget = target.trimmed();
    item->reputations.append({trimmedTarget, clampRep(value)});
    if (!containsNicknameKey(initialWorldRepOrder, trimmedTarget))
        initialWorldRepOrder.append(trimmedTarget);
}

double FactionWorld::empathyRate(const QString &source, const QString &target, double fallback) const
{
    const Faction *item = faction(source);
    if (!item)
        return fallback;
    const QString targetKey = keyForNickname(target);
    for (const EmpathyRate &rate : item->empathyRates) {
        if (keyForNickname(rate.target) == targetKey)
            return rate.rate;
    }
    return fallback;
}

void FactionWorld::setEmpathyRate(const QString &source, const QString &target, double value)
{
    Faction *item = faction(source);
    if (!item || target.trimmed().isEmpty())
        return;
    const QString targetKey = keyForNickname(target);
    for (EmpathyRate &rate : item->empathyRates) {
        if (keyForNickname(rate.target) == targetKey) {
            rate.rate = value;
            return;
        }
    }
    item->empathyRates.append({target.trimmed(), value});
}

QList<FactionValidationIssue> FactionWorld::validate() const
{
    QList<FactionValidationIssue> issues;
    QHash<QString, QString> originalCase;
    for (const QString &nickname : declaredNicknames) {
        const QString key = keyForNickname(nickname);
        if (originalCase.contains(key) && originalCase.value(key) != nickname) {
            issues.append({FactionValidationSeverity::Critical, nickname,
                           QStringLiteral("Case-collision for faction nickname")});
        }
        originalCase.insert(key, nickname);
    }

    originalCase.clear();
    for (const Faction &faction : factionsByKey) {
        const QString key = keyForNickname(faction.nickname);
        if (originalCase.contains(key) && originalCase.value(key) != faction.nickname) {
            issues.append({FactionValidationSeverity::Critical, faction.nickname,
                           QStringLiteral("Case-collision for faction nickname")});
        }
        originalCase.insert(key, faction.nickname);

        if (!faction.inInitialWorld)
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing in initialworld.ini")});
        if (!faction.inEmpathy)
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing in empathy.ini")});
        if (!faction.inFactionProp)
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing in faction_prop.ini")});
        if (faction.idsName.trimmed().isEmpty())
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing ids_name")});
        if (faction.idsInfo.trimmed().isEmpty())
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing ids_info")});
        if (faction.props.legality.trimmed().isEmpty())
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Missing legality")});
        if (!faction.props.affiliation.trimmed().isEmpty()
            && keyForNickname(faction.props.affiliation) != keyForNickname(faction.nickname)) {
            issues.append({FactionValidationSeverity::Warning, faction.nickname,
                           QStringLiteral("FactionProps affiliation does not match nickname")});
        }
        if (hasDuplicateTarget(faction.reputations))
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Duplicate reputation target")});
        if (faction.inInitialWorld
            && !initialWorldRepOrder.isEmpty()
            && reputationTargetKeys(faction.reputations) != expectedReputationTargetKeys(faction.reputations, initialWorldRepOrder)) {
            issues.append({FactionValidationSeverity::Warning, faction.nickname,
                           QStringLiteral("Reputation entries do not follow initialworld.ini order")});
        }
        if (hasDuplicateTarget(faction.empathyRates))
            issues.append({FactionValidationSeverity::Warning, faction.nickname, QStringLiteral("Duplicate empathy_rate target")});

        for (const FactionRep &rep : faction.reputations) {
            if (!contains(rep.target))
                issues.append({FactionValidationSeverity::Critical, faction.nickname,
                               QStringLiteral("Reputation target does not exist: %1").arg(rep.target)});
            if (rep.value < -1.0 || rep.value > 1.0)
                issues.append({FactionValidationSeverity::Critical, faction.nickname,
                               QStringLiteral("Reputation out of range: %1").arg(rep.target)});
            if (contains(rep.target) && this->faction(rep.target)
                && std::isnan(reputation(rep.target, faction.nickname, std::numeric_limits<double>::quiet_NaN()))) {
                issues.append({FactionValidationSeverity::Warning, faction.nickname,
                               QStringLiteral("Missing reciprocal reputation: %1").arg(rep.target)});
            }
        }
        for (const EmpathyRate &rate : faction.empathyRates) {
            if (!contains(rate.target))
                issues.append({FactionValidationSeverity::Critical, faction.nickname,
                               QStringLiteral("Empathy target does not exist: %1").arg(rate.target)});
            if (rate.rate < -1.0 || rate.rate > 1.0)
                issues.append({FactionValidationSeverity::Warning, faction.nickname,
                               QStringLiteral("Empathy rate outside typical range: %1").arg(rate.target)});
        }
    }
    return issues;
}

} // namespace flatlas::domain
