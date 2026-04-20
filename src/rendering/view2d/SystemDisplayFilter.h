#pragma once

#include "domain/SolarObject.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

namespace flatlas::rendering {

enum class DisplayFilterTarget {
    Object,
    Label,
    Both
};

enum class DisplayFilterField {
    Nickname,
    Archetype,
    Type
};

enum class DisplayFilterMatchMode {
    Contains,
    Equals,
    StartsWith,
    EndsWith
};

enum class DisplayFilterAction {
    Show,
    Hide
};

struct SystemDisplayFilterRule {
    QString id;
    QString name;
    bool enabled = true;
    DisplayFilterTarget target = DisplayFilterTarget::Label;
    DisplayFilterField field = DisplayFilterField::Nickname;
    DisplayFilterMatchMode matchMode = DisplayFilterMatchMode::Contains;
    DisplayFilterAction action = DisplayFilterAction::Hide;
    QString pattern;

    QJsonObject toJson() const;
    static SystemDisplayFilterRule fromJson(const QJsonObject &obj);
};

struct SystemDisplayFilterSettings {
    QMap<int, bool> objectVisibilityByType;
    QMap<int, bool> labelVisibilityByType;
    QList<SystemDisplayFilterRule> rules;

    bool objectVisibleForType(flatlas::domain::SolarObject::Type type) const;
    bool labelVisibleForType(flatlas::domain::SolarObject::Type type) const;
    void setObjectVisibleForType(flatlas::domain::SolarObject::Type type, bool visible);
    void setLabelVisibleForType(flatlas::domain::SolarObject::Type type, bool visible);

    QJsonObject toJson() const;
    static SystemDisplayFilterSettings fromJson(const QJsonObject &obj);
};

struct SolarObjectDisplayContext {
    QString nickname;
    QString archetype;
    flatlas::domain::SolarObject::Type type = flatlas::domain::SolarObject::Other;
    QString typeNameOverride;
};

QString displayFilterTargetToString(DisplayFilterTarget target);
QString displayFilterFieldToString(DisplayFilterField field);
QString displayFilterMatchModeToString(DisplayFilterMatchMode mode);
QString displayFilterActionToString(DisplayFilterAction action);

bool matchesDisplayFilterRule(const SystemDisplayFilterRule &rule,
                              const SolarObjectDisplayContext &context);

} // namespace flatlas::rendering
