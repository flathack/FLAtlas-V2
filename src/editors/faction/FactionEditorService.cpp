#include "FactionEditorService.h"

#include "infrastructure/freelancer/IdsDataService.h"

namespace flatlas::editors {

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

bool FactionEditorService::addFaction(const QString &nickname, const QString &ingameName, QString *errorMessage)
{
    const QString trimmedNickname = nickname.trimmed();
    const QString trimmedName = ingameName.trimmed();
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
                                                                   trimmedName,
                                                                   &idsShortName,
                                                                   errorMessage)) {
        return false;
    }

    m_world.addFaction(trimmedNickname);
    setIds(trimmedNickname, QString::number(idsName), QString(), QString::number(idsShortName));
    setDirty(true);
    emit worldChanged();
    return true;
}

void FactionEditorService::deactivateFaction(const QString &nickname)
{
    if (!m_world.deactivateFaction(nickname))
        return;
    setDirty(true);
    emit worldChanged();
}

void FactionEditorService::setIds(const QString &nickname, const QString &idsName, const QString &idsInfo, const QString &idsShortName)
{
    auto *item = m_world.faction(nickname);
    if (!item)
        return;
    item->idsName = idsName.trimmed();
    item->idsInfo = idsInfo.trimmed();
    item->idsShortName = idsShortName.trimmed();
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
    item->props = props;
    if (item->props.affiliation.trimmed().isEmpty())
        item->props.affiliation = item->nickname;
    item->inInitialWorld = inInitialWorld;
    item->inEmpathy = inEmpathy;
    item->inFactionProp = inFactionProp;
    setDirty(true);
}

void FactionEditorService::setReputation(const QString &source, const QString &target, double value)
{
    m_world.setReputation(source, target, value);
    setDirty(true);
}

void FactionEditorService::setEmpathyRate(const QString &source, const QString &target, double value)
{
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

} // namespace flatlas::editors
