#include "SystemDisplayFilter.h"

#include <QMetaEnum>

namespace flatlas::rendering {

namespace {

QString typeName(flatlas::domain::SolarObject::Type type)
{
    return QString::fromLatin1(QMetaEnum::fromType<flatlas::domain::SolarObject::Type>().valueToKey(type));
}

QString fieldValue(DisplayFilterField field, const SolarObjectDisplayContext &context)
{
    switch (field) {
    case DisplayFilterField::Nickname:
        return context.nickname;
    case DisplayFilterField::Archetype:
        return context.archetype;
    case DisplayFilterField::Type:
        return typeName(context.type);
    }
    return {};
}

bool matchesPattern(DisplayFilterMatchMode mode, const QString &value, const QString &pattern)
{
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    switch (mode) {
    case DisplayFilterMatchMode::Contains:
        return value.contains(pattern, cs);
    case DisplayFilterMatchMode::Equals:
        return value.compare(pattern, cs) == 0;
    case DisplayFilterMatchMode::StartsWith:
        return value.startsWith(pattern, cs);
    case DisplayFilterMatchMode::EndsWith:
        return value.endsWith(pattern, cs);
    }
    return false;
}

}

QJsonObject SystemDisplayFilterRule::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("enabled"), enabled);
    obj.insert(QStringLiteral("target"), displayFilterTargetToString(target));
    obj.insert(QStringLiteral("field"), displayFilterFieldToString(field));
    obj.insert(QStringLiteral("matchMode"), displayFilterMatchModeToString(matchMode));
    obj.insert(QStringLiteral("action"), displayFilterActionToString(action));
    obj.insert(QStringLiteral("pattern"), pattern);
    return obj;
}

SystemDisplayFilterRule SystemDisplayFilterRule::fromJson(const QJsonObject &obj)
{
    SystemDisplayFilterRule rule;
    rule.id = obj.value(QStringLiteral("id")).toString();
    rule.name = obj.value(QStringLiteral("name")).toString();
    rule.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    const QString target = obj.value(QStringLiteral("target")).toString();
    if (target.compare(QStringLiteral("Object"), Qt::CaseInsensitive) == 0)
        rule.target = DisplayFilterTarget::Object;
    else if (target.compare(QStringLiteral("Both"), Qt::CaseInsensitive) == 0)
        rule.target = DisplayFilterTarget::Both;
    else
        rule.target = DisplayFilterTarget::Label;

    const QString field = obj.value(QStringLiteral("field")).toString();
    if (field.compare(QStringLiteral("Archetype"), Qt::CaseInsensitive) == 0)
        rule.field = DisplayFilterField::Archetype;
    else if (field.compare(QStringLiteral("Type"), Qt::CaseInsensitive) == 0)
        rule.field = DisplayFilterField::Type;
    else
        rule.field = DisplayFilterField::Nickname;

    const QString matchMode = obj.value(QStringLiteral("matchMode")).toString();
    if (matchMode.compare(QStringLiteral("Equals"), Qt::CaseInsensitive) == 0)
        rule.matchMode = DisplayFilterMatchMode::Equals;
    else if (matchMode.compare(QStringLiteral("Starts With"), Qt::CaseInsensitive) == 0)
        rule.matchMode = DisplayFilterMatchMode::StartsWith;
    else if (matchMode.compare(QStringLiteral("Ends With"), Qt::CaseInsensitive) == 0)
        rule.matchMode = DisplayFilterMatchMode::EndsWith;
    else
        rule.matchMode = DisplayFilterMatchMode::Contains;

    const QString action = obj.value(QStringLiteral("action")).toString();
    rule.action = action.compare(QStringLiteral("Show"), Qt::CaseInsensitive) == 0
        ? DisplayFilterAction::Show
        : DisplayFilterAction::Hide;
    rule.pattern = obj.value(QStringLiteral("pattern")).toString();
    return rule;
}

bool SystemDisplayFilterSettings::objectVisibleForType(flatlas::domain::SolarObject::Type type) const
{
    return objectVisibilityByType.value(static_cast<int>(type), true);
}

bool SystemDisplayFilterSettings::labelVisibleForType(flatlas::domain::SolarObject::Type type) const
{
    return labelVisibilityByType.value(static_cast<int>(type), true);
}

void SystemDisplayFilterSettings::setObjectVisibleForType(flatlas::domain::SolarObject::Type type, bool visible)
{
    objectVisibilityByType.insert(static_cast<int>(type), visible);
}

void SystemDisplayFilterSettings::setLabelVisibleForType(flatlas::domain::SolarObject::Type type, bool visible)
{
    labelVisibilityByType.insert(static_cast<int>(type), visible);
}

QJsonObject SystemDisplayFilterSettings::toJson() const
{
    QJsonObject obj;
    QJsonObject objectVisibility;
    for (auto it = objectVisibilityByType.begin(); it != objectVisibilityByType.end(); ++it)
        objectVisibility.insert(QString::number(it.key()), it.value());
    obj.insert(QStringLiteral("objectVisibilityByType"), objectVisibility);

    QJsonObject labelVisibility;
    for (auto it = labelVisibilityByType.begin(); it != labelVisibilityByType.end(); ++it)
        labelVisibility.insert(QString::number(it.key()), it.value());
    obj.insert(QStringLiteral("labelVisibilityByType"), labelVisibility);

    QJsonArray rulesArray;
    for (const auto &rule : rules)
        rulesArray.append(rule.toJson());
    obj.insert(QStringLiteral("rules"), rulesArray);
    return obj;
}

SystemDisplayFilterSettings SystemDisplayFilterSettings::fromJson(const QJsonObject &obj)
{
    SystemDisplayFilterSettings settings;
    const QJsonObject objectVisibility = obj.value(QStringLiteral("objectVisibilityByType")).toObject();
    for (auto it = objectVisibility.begin(); it != objectVisibility.end(); ++it)
        settings.objectVisibilityByType.insert(it.key().toInt(), it.value().toBool(true));

    const QJsonObject labelVisibility = obj.value(QStringLiteral("labelVisibilityByType")).toObject();
    for (auto it = labelVisibility.begin(); it != labelVisibility.end(); ++it)
        settings.labelVisibilityByType.insert(it.key().toInt(), it.value().toBool(true));

    const QJsonArray rulesArray = obj.value(QStringLiteral("rules")).toArray();
    for (const QJsonValue &value : rulesArray) {
        if (value.isObject())
            settings.rules.append(SystemDisplayFilterRule::fromJson(value.toObject()));
    }
    return settings;
}

QString displayFilterTargetToString(DisplayFilterTarget target)
{
    switch (target) {
    case DisplayFilterTarget::Object:
        return QStringLiteral("Object");
    case DisplayFilterTarget::Label:
        return QStringLiteral("Label");
    case DisplayFilterTarget::Both:
        return QStringLiteral("Both");
    }
    return {};
}

QString displayFilterFieldToString(DisplayFilterField field)
{
    switch (field) {
    case DisplayFilterField::Nickname:
        return QStringLiteral("Nickname");
    case DisplayFilterField::Archetype:
        return QStringLiteral("Archetype");
    case DisplayFilterField::Type:
        return QStringLiteral("Type");
    }
    return {};
}

QString displayFilterMatchModeToString(DisplayFilterMatchMode mode)
{
    switch (mode) {
    case DisplayFilterMatchMode::Contains:
        return QStringLiteral("Contains");
    case DisplayFilterMatchMode::Equals:
        return QStringLiteral("Equals");
    case DisplayFilterMatchMode::StartsWith:
        return QStringLiteral("Starts With");
    case DisplayFilterMatchMode::EndsWith:
        return QStringLiteral("Ends With");
    }
    return {};
}

QString displayFilterActionToString(DisplayFilterAction action)
{
    switch (action) {
    case DisplayFilterAction::Show:
        return QStringLiteral("Show");
    case DisplayFilterAction::Hide:
        return QStringLiteral("Hide");
    }
    return {};
}

bool matchesDisplayFilterRule(const SystemDisplayFilterRule &rule,
                              const SolarObjectDisplayContext &context)
{
    if (!rule.enabled || rule.pattern.trimmed().isEmpty())
        return false;
    return matchesPattern(rule.matchMode, fieldValue(rule.field, context), rule.pattern.trimmed());
}

} // namespace flatlas::rendering
