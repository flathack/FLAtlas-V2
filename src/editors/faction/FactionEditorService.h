#pragma once

#include "domain/FactionData.h"
#include "infrastructure/freelancer/FactionRepository.h"

#include <QObject>
#include <QString>

namespace flatlas::editors {

class FactionEditorService : public QObject {
    Q_OBJECT
public:
    explicit FactionEditorService(QObject *parent = nullptr);

    bool load(const QString &gameRoot, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr);

    const flatlas::domain::FactionWorld &world() const { return m_world; }
    flatlas::domain::FactionWorld &world() { return m_world; }
    QString gameRoot() const { return m_gameRoot; }
    bool isDirty() const { return m_dirty; }

    flatlas::domain::Faction *faction(const QString &nickname);
    const flatlas::domain::Faction *faction(const QString &nickname) const;

    bool addFaction(const QString &nickname, const QString &ingameName, QString *errorMessage = nullptr);
    void deactivateFaction(const QString &nickname);
    void setIds(const QString &nickname, const QString &idsName, const QString &idsInfo, const QString &idsShortName);
    void setProperties(const QString &nickname,
                       const flatlas::domain::FactionPropData &props,
                       bool inInitialWorld,
                       bool inEmpathy,
                       bool inFactionProp);
    void setReputation(const QString &source, const QString &target, double value);
    void setEmpathyRate(const QString &source, const QString &target, double value);
    QList<flatlas::domain::FactionValidationIssue> validate() const;

signals:
    void worldChanged();
    void dirtyChanged(bool dirty);

private:
    void setDirty(bool dirty);

    flatlas::infrastructure::FactionRepository m_repository;
    flatlas::domain::FactionWorld m_world;
    QString m_gameRoot;
    bool m_dirty = false;
};

} // namespace flatlas::editors
