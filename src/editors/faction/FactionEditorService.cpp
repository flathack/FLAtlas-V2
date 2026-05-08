#include "FactionEditorService.h"

#include "infrastructure/freelancer/IdsDataService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flatlas::editors {

namespace {

QString canonicalFactionPath(const QString &relativePath)
{
    return QDir::cleanPath(relativePath).toLower();
}

bool isFactionRepositoryFile(const QString &relativePath)
{
    const QString path = canonicalFactionPath(relativePath);
    return path == QStringLiteral("data/initialworld.ini")
        || path == QStringLiteral("data/missions/empathy.ini")
        || path == QStringLiteral("data/missions/faction_prop.ini");
}

QRegularExpression factionTokenRegex(const QString &nickname)
{
    return QRegularExpression(QStringLiteral("(?i)(^|[^A-Za-z0-9_])(%1)(?=$|[^A-Za-z0-9_])")
                                  .arg(QRegularExpression::escape(nickname.trimmed())));
}

QString defaultInfocardXml(const QString &name, const QString &body)
{
    const QString text = body.trimmed().isEmpty() ? name.trimmed() : body.trimmed();
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-16\"?><RDL><PUSH/><TEXT>%1</TEXT><PARA/><POP/></RDL>")
        .arg(text.toHtmlEscaped());
}

QString defaultMsgIdPrefix(const QString &nickname)
{
    QString base = nickname.trimmed();
    if (base.endsWith(QStringLiteral("_grp"), Qt::CaseInsensitive))
        base.chop(4);
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    return QStringLiteral("gcs_refer_faction_%1").arg(base);
}

flatlas::domain::FactionPropData fallbackCreationProps(const QString &nickname)
{
    flatlas::domain::FactionPropData props;
    props.affiliation = nickname;
    props.legality = QStringLiteral("lawful");
    props.nicknamePlurality = QStringLiteral("singular");
    props.msgIdPrefix = defaultMsgIdPrefix(nickname);
    props.jumpPreference = QStringLiteral("jumpgate");
    return props;
}

bool factionPropsEqual(const flatlas::domain::FactionPropData &left,
                       const flatlas::domain::FactionPropData &right)
{
    return left.affiliation == right.affiliation
        && left.legality == right.legality
        && left.nicknamePlurality == right.nicknamePlurality
        && left.msgIdPrefix == right.msgIdPrefix
        && left.jumpPreference == right.jumpPreference
        && left.npcShips == right.npcShips
        && left.voices == right.voices
        && left.mcCostume == right.mcCostume
        && left.spaceCostumes == right.spaceCostumes
        && left.firstnameMale == right.firstnameMale
        && left.firstnameFemale == right.firstnameFemale
        && left.lastname == right.lastname
        && left.rankDesig == right.rankDesig
        && left.formationDesig == right.formationDesig
        && left.largeShipDesig == right.largeShipDesig
        && left.largeShipNames == right.largeShipNames
        && left.scanForCargo == right.scanForCargo
        && left.scanAnnounce == right.scanAnnounce
        && left.scanChance == right.scanChance
        && left.formations == right.formations;
}

bool nearlyEqual(double left, double right)
{
    return std::abs(left - right) <= 0.000001;
}

QString replaceFactionToken(const QString &line, const QString &nickname, const QString &replacement)
{
    const QRegularExpression regex = factionTokenRegex(nickname);
    QString result = line;
    qsizetype offset = 0;
    QRegularExpressionMatch match = regex.match(result, offset);
    while (match.hasMatch()) {
        const QString prefix = match.captured(1);
        const int start = match.capturedStart(0);
        const int length = match.capturedLength(0);
        const QString replacementText = prefix + replacement;
        result.replace(start, length, replacementText);
        offset = start + replacementText.size();
        match = regex.match(result, offset);
    }
    return result;
}

void replaceOrRemoveReputation(QList<flatlas::domain::FactionRep> *items,
                               const QString &targetKey,
                               const QString &replacement)
{
    if (!items)
        return;
    if (replacement.trimmed().isEmpty()) {
        items->erase(std::remove_if(items->begin(), items->end(), [&](const auto &rep) {
            return flatlas::domain::FactionWorld::keyForNickname(rep.target) == targetKey;
        }), items->end());
        return;
    }

    bool replacementExists = false;
    for (auto &rep : *items) {
        if (flatlas::domain::FactionWorld::keyForNickname(rep.target)
            == flatlas::domain::FactionWorld::keyForNickname(replacement)) {
            replacementExists = true;
            break;
        }
    }
    for (int i = items->size() - 1; i >= 0; --i) {
        auto &rep = (*items)[i];
        if (flatlas::domain::FactionWorld::keyForNickname(rep.target) != targetKey)
            continue;
        if (replacementExists) {
            items->removeAt(i);
        } else {
            rep.target = replacement;
            replacementExists = true;
        }
    }
}

void replaceOrRemoveEmpathy(QList<flatlas::domain::EmpathyRate> *items,
                            const QString &targetKey,
                            const QString &replacement)
{
    if (!items)
        return;
    if (replacement.trimmed().isEmpty()) {
        items->erase(std::remove_if(items->begin(), items->end(), [&](const auto &rate) {
            return flatlas::domain::FactionWorld::keyForNickname(rate.target) == targetKey;
        }), items->end());
        return;
    }

    bool replacementExists = false;
    for (auto &rate : *items) {
        if (flatlas::domain::FactionWorld::keyForNickname(rate.target)
            == flatlas::domain::FactionWorld::keyForNickname(replacement)) {
            replacementExists = true;
            break;
        }
    }
    for (int i = items->size() - 1; i >= 0; --i) {
        auto &rate = (*items)[i];
        if (flatlas::domain::FactionWorld::keyForNickname(rate.target) != targetKey)
            continue;
        if (replacementExists) {
            items->removeAt(i);
        } else {
            rate.target = replacement;
            replacementExists = true;
        }
    }
}

} // namespace

FactionEditorService::FactionEditorService(QObject *parent)
    : QObject(parent)
{
}

bool FactionEditorService::load(const QString &gameRoot, QString *errorMessage)
{
    const auto result = m_repository.load(gameRoot);
    m_world = result.world;
    m_gameRoot = gameRoot;
    setDirty(false);
    emit worldChanged();
    if (errorMessage)
        *errorMessage = result.warnings.join(QLatin1Char('\n'));
    return !gameRoot.trimmed().isEmpty();
}

bool FactionEditorService::save(QString *errorMessage)
{
    if (!m_repository.save(m_world, m_gameRoot, errorMessage))
        return false;
    setDirty(false);
    emit worldChanged();
    return true;
}

flatlas::domain::Faction *FactionEditorService::faction(const QString &nickname)
{
    return m_world.faction(nickname);
}

const flatlas::domain::Faction *FactionEditorService::faction(const QString &nickname) const
{
    return m_world.faction(nickname);
}

bool FactionEditorService::addFaction(const FactionCreationRequest &request, QString *errorMessage)
{
    const QString trimmedNickname = request.nickname.trimmed();
    const QString trimmedName = request.ingameName.trimmed();
    const QString trimmedShortName = request.shortName.trimmed().isEmpty() ? trimmedName : request.shortName.trimmed();
    const QString trimmedTemplate = request.templateNickname.trimmed();
    if (trimmedNickname.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Nickname is empty.");
        return false;
    }
    if (trimmedName.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("Ingame name is empty.");
        return false;
    }
    if (m_world.contains(trimmedNickname)) {
        if (errorMessage)
            *errorMessage = tr("Faction already exists: %1").arg(trimmedNickname);
        return false;
    }
    if (m_gameRoot.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No active editing context.");
        return false;
    }

    flatlas::domain::FactionPropData props = fallbackCreationProps(trimmedNickname);
    if (!trimmedTemplate.isEmpty()) {
        const auto *templateFaction = m_world.faction(trimmedTemplate);
        if (!templateFaction || !templateFaction->inFactionProp) {
            if (errorMessage)
                *errorMessage = tr("Template faction does not exist in faction_prop.ini: %1").arg(trimmedTemplate);
            return false;
        }
        props = templateFaction->props;
        props.affiliation = trimmedNickname;
    }
    if (!request.legality.trimmed().isEmpty())
        props.legality = request.legality.trimmed();
    if (props.nicknamePlurality.trimmed().isEmpty())
        props.nicknamePlurality = QStringLiteral("singular");
    if (props.msgIdPrefix.trimmed().isEmpty())
        props.msgIdPrefix = defaultMsgIdPrefix(trimmedNickname);
    if (props.jumpPreference.trimmed().isEmpty())
        props.jumpPreference = QStringLiteral("jumpgate");

    const auto dataset = flatlas::infrastructure::IdsDataService::loadFromGameRoot(m_gameRoot);
    const QString targetDll = flatlas::infrastructure::IdsDataService::defaultCreationDllName(dataset);
    if (targetDll.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No target DLL for IDS entries found.");
        return false;
    }

    int idsName = 0;
    if (!flatlas::infrastructure::IdsDataService::writeStringEntry(dataset,
                                                                   targetDll,
                                                                   0,
                                                                   trimmedName,
                                                                   &idsName,
                                                                   errorMessage)) {
        return false;
    }

    int idsShortName = 0;
    if (!flatlas::infrastructure::IdsDataService::writeStringEntry(dataset,
                                                                   targetDll,
                                                                   0,
                                                                   trimmedShortName,
                                                                   &idsShortName,
                                                                   errorMessage)) {
        return false;
    }

    int idsInfo = 0;
    if (!flatlas::infrastructure::IdsDataService::writeInfocardEntry(dataset,
                                                                     targetDll,
                                                                     0,
                                                                     defaultInfocardXml(trimmedName, request.infocardText),
                                                                     &idsInfo,
                                                                     errorMessage)) {
        return false;
    }

    m_world.addFaction(trimmedNickname);
    setIds(trimmedNickname, QString::number(idsName), QString::number(idsInfo), QString::number(idsShortName));
    if (auto *created = m_world.faction(trimmedNickname))
        created->props = props;
    setDirty(true);
    emit worldChanged();
    return true;
}

bool FactionEditorService::addFaction(const QString &nickname, const QString &ingameName, QString *errorMessage)
{
    FactionCreationRequest request;
    request.nickname = nickname;
    request.ingameName = ingameName;
    return addFaction(request, errorMessage);
}

QList<FactionReferenceRecord> FactionEditorService::referencesForFaction(const QString &nickname) const
{
    QList<FactionReferenceRecord> records;
    const auto *target = m_world.faction(nickname);
    if (!target)
        return records;

    const QString key = flatlas::domain::FactionWorld::keyForNickname(nickname);
    if (target->inInitialWorld) {
        records.append({tr("Faction data"), target->nickname, QStringLiteral("[Group]"),
                        QStringLiteral("DATA/initialworld.ini"), 0,
                        tr("Faction is present in initialworld.ini"), false, false});
    }
    if (target->inEmpathy) {
        records.append({tr("Faction data"), target->nickname, QStringLiteral("[RepChangeEffects]"),
                        QStringLiteral("DATA/MISSIONS/empathy.ini"), 0,
                        tr("Faction is present in empathy.ini"), false, false});
    }
    if (target->inFactionProp) {
        records.append({tr("Faction data"), target->nickname, QStringLiteral("[FactionProps]"),
                        QStringLiteral("DATA/MISSIONS/faction_prop.ini"), 0,
                        tr("Faction is present in faction_prop.ini"), false, false});
    }

    for (const auto &source : m_world.factionsByKey) {
        const bool isTargetFaction = flatlas::domain::FactionWorld::keyForNickname(source.nickname) == key;
        for (const auto &rep : source.reputations) {
            if (flatlas::domain::FactionWorld::keyForNickname(rep.target) == key) {
                records.append({tr("Reputation"), source.nickname, QStringLiteral("rep"),
                                QStringLiteral("DATA/initialworld.ini"), 0,
                                tr("%1 has reputation entry to %2").arg(source.nickname, target->nickname),
                                !isTargetFaction, false});
            }
        }
        for (const auto &rate : source.empathyRates) {
            if (flatlas::domain::FactionWorld::keyForNickname(rate.target) == key) {
                records.append({tr("Empathy"), source.nickname, QStringLiteral("empathy_rate"),
                                QStringLiteral("DATA/MISSIONS/empathy.ini"), 0,
                                tr("%1 has empathy_rate entry to %2").arg(source.nickname, target->nickname),
                                !isTargetFaction, false});
            }
        }
        if (!source.props.affiliation.trimmed().isEmpty()
            && flatlas::domain::FactionWorld::keyForNickname(source.props.affiliation) == key) {
            records.append({tr("Faction properties"), source.nickname, QStringLiteral("affiliation"),
                            QStringLiteral("DATA/MISSIONS/faction_prop.ini"), 0,
                            tr("%1 uses %2 as affiliation").arg(source.nickname, target->nickname),
                            !isTargetFaction, false});
        }
    }

    if (!m_gameRoot.trimmed().isEmpty()) {
        const QDir root(m_gameRoot);
        const QString dataPath = root.filePath(QStringLiteral("DATA"));
        const QRegularExpression regex = factionTokenRegex(target->nickname);
        QDirIterator it(dataPath,
                        QStringList{QStringLiteral("*.ini"), QStringLiteral("*.cfg"), QStringLiteral("*.txt")},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString relative = root.relativeFilePath(path);
            if (isFactionRepositoryFile(relative))
                continue;
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            int lineNumber = 0;
            while (!file.atEnd()) {
                ++lineNumber;
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')))
                    continue;
                if (regex.match(line).hasMatch()) {
                    records.append({tr("External file"), QFileInfo(path).fileName(), QStringLiteral("text"),
                                    relative, lineNumber, line, true, true});
                }
            }
        }
    }

    return records;
}

bool FactionEditorService::deactivateFaction(const QString &nickname,
                                             const QString &replacementNickname,
                                             QString *errorMessage)
{
    auto *item = m_world.faction(nickname);
    if (!item) {
        if (errorMessage)
            *errorMessage = tr("Faction not found: %1").arg(nickname);
        return false;
    }
    if (!replacementNickname.trimmed().isEmpty()
        && !m_world.contains(replacementNickname)) {
        if (errorMessage)
            *errorMessage = tr("Replacement faction not found: %1").arg(replacementNickname);
        return false;
    }
    if (replacementNickname.trimmed().isEmpty()) {
        for (const auto &record : referencesForFaction(nickname)) {
            if (record.externalFileReference) {
                if (errorMessage) {
                    *errorMessage = tr("References outside the global faction files must be replaced, not removed. "
                                       "Choose a replacement faction before deactivating.");
                }
                return false;
            }
        }
    }

    replaceOrRemoveFactionLinks(nickname, replacementNickname);
    if (!rewriteExternalReferences(nickname, replacementNickname, errorMessage))
        return false;
    item->inInitialWorld = false;
    item->inEmpathy = false;
    item->inFactionProp = false;
    setDirty(true);
    emit worldChanged();
    return true;
}

bool FactionEditorService::deleteFaction(const QString &nickname,
                                         const QString &replacementNickname,
                                         QString *errorMessage)
{
    if (!deactivateFaction(nickname, replacementNickname, errorMessage))
        return false;
    if (hasBlockingReferences(nickname)) {
        if (errorMessage)
            *errorMessage = tr("The faction still has references. Remove or replace all relationships before deleting.");
        return false;
    }
    if (!m_world.removeFaction(nickname)) {
        if (errorMessage)
            *errorMessage = tr("Faction could not be deleted: %1").arg(nickname);
        return false;
    }
    setDirty(true);
    emit worldChanged();
    return true;
}

void FactionEditorService::setIds(const QString &nickname, const QString &idsName, const QString &idsInfo, const QString &idsShortName)
{
    auto *item = m_world.faction(nickname);
    if (!item)
        return;
    const QString trimmedIdsName = idsName.trimmed();
    const QString trimmedIdsInfo = idsInfo.trimmed();
    const QString trimmedIdsShortName = idsShortName.trimmed();
    if (item->idsName == trimmedIdsName
        && item->idsInfo == trimmedIdsInfo
        && item->idsShortName == trimmedIdsShortName) {
        return;
    }
    item->idsName = trimmedIdsName;
    item->idsInfo = trimmedIdsInfo;
    item->idsShortName = trimmedIdsShortName;
    setDirty(true);
}

void FactionEditorService::setProperties(const QString &nickname,
                                         const flatlas::domain::FactionPropData &props,
                                         bool inInitialWorld,
                                         bool inEmpathy,
                                         bool inFactionProp)
{
    auto *item = m_world.faction(nickname);
    if (!item)
        return;
    flatlas::domain::FactionPropData normalizedProps = props;
    if (normalizedProps.affiliation.trimmed().isEmpty())
        normalizedProps.affiliation = item->nickname;
    if (factionPropsEqual(item->props, normalizedProps)
        && item->inInitialWorld == inInitialWorld
        && item->inEmpathy == inEmpathy
        && item->inFactionProp == inFactionProp) {
        return;
    }
    item->props = normalizedProps;
    item->inInitialWorld = inInitialWorld;
    item->inEmpathy = inEmpathy;
    item->inFactionProp = inFactionProp;
    setDirty(true);
}

void FactionEditorService::setReputation(const QString &source, const QString &target, double value)
{
    const double clampedValue = std::clamp(value, -1.0, 1.0);
    const double currentValue = m_world.reputation(source, target, std::numeric_limits<double>::quiet_NaN());
    if (!std::isnan(currentValue) && nearlyEqual(currentValue, clampedValue))
        return;
    m_world.setReputation(source, target, value);
    setDirty(true);
}

void FactionEditorService::setEmpathyRate(const QString &source, const QString &target, double value)
{
    const double currentValue = m_world.empathyRate(source, target, std::numeric_limits<double>::quiet_NaN());
    if (!std::isnan(currentValue) && nearlyEqual(currentValue, value))
        return;
    m_world.setEmpathyRate(source, target, value);
    setDirty(true);
}

QList<flatlas::domain::FactionValidationIssue> FactionEditorService::validate() const
{
    return m_world.validate();
}

void FactionEditorService::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

void FactionEditorService::replaceOrRemoveFactionLinks(const QString &nickname, const QString &replacementNickname)
{
    const QString targetKey = flatlas::domain::FactionWorld::keyForNickname(nickname);
    for (auto &faction : m_world.factionsByKey) {
        if (flatlas::domain::FactionWorld::keyForNickname(faction.nickname) == targetKey)
            continue;
        replaceOrRemoveReputation(&faction.reputations, targetKey, replacementNickname);
        replaceOrRemoveEmpathy(&faction.empathyRates, targetKey, replacementNickname);
        if (flatlas::domain::FactionWorld::keyForNickname(faction.props.affiliation) == targetKey)
            faction.props.affiliation = replacementNickname.trimmed().isEmpty() ? faction.nickname : replacementNickname;
    }
}

bool FactionEditorService::rewriteExternalReferences(const QString &nickname,
                                                     const QString &replacementNickname,
                                                     QString *errorMessage)
{
    if (m_gameRoot.trimmed().isEmpty())
        return true;

    const QDir root(m_gameRoot);
    const QString dataPath = root.filePath(QStringLiteral("DATA"));
    const QRegularExpression regex = factionTokenRegex(nickname);
    QDirIterator it(dataPath,
                    QStringList{QStringLiteral("*.ini"), QStringLiteral("*.cfg"), QStringLiteral("*.txt")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (isFactionRepositoryFile(root.relativeFilePath(path)))
            continue;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QStringList lines;
        bool changed = false;
        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine());
            const QString trimmedLine = line.trimmed();
            if ((trimmedLine.startsWith(QLatin1Char(';')) || trimmedLine.startsWith(QLatin1Char('#')))) {
                lines.append(line);
                continue;
            }
            if (regex.match(line).hasMatch()) {
                changed = true;
                if (replacementNickname.trimmed().isEmpty()) {
                    if (errorMessage) {
                        *errorMessage = tr("Cannot remove faction reference in %1 without a replacement faction.")
                                            .arg(path);
                    }
                    return false;
                }
                line = replaceFactionToken(line, nickname, replacementNickname);
            }
            lines.append(line);
        }
        file.close();

        if (!changed)
            continue;

        QSaveFile out(path);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorMessage)
                *errorMessage = tr("Could not write %1").arg(path);
            return false;
        }
        for (const QString &line : lines)
            out.write(line.toUtf8());
        if (!out.commit()) {
            if (errorMessage)
                *errorMessage = tr("Could not save %1").arg(path);
            return false;
        }
    }
    return true;
}

bool FactionEditorService::hasBlockingReferences(const QString &nickname) const
{
    for (const auto &record : referencesForFaction(nickname)) {
        if (record.blocksDelete)
            return true;
    }
    return false;
}

} // namespace flatlas::editors
