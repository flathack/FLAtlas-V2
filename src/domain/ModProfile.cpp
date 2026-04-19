#include "ModProfile.h"
#include <QJsonArray>

namespace flatlas::domain {

QJsonObject ModProfile::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("gamePath")] = gamePath;
    obj[QStringLiteral("modPath")] = modPath;
    obj[QStringLiteral("description")] = description;
    obj[QStringLiteral("isDefault")] = isDefault;
    obj[QStringLiteral("isActive")] = isActive;

    QJsonArray modsArray;
    for (const auto &mod : activeMods)
        modsArray.append(mod);
    obj[QStringLiteral("activeMods")] = modsArray;

    return obj;
}

ModProfile ModProfile::fromJson(const QJsonObject &obj)
{
    ModProfile p;
    p.name = obj[QStringLiteral("name")].toString();
    p.id = obj[QStringLiteral("id")].toString();
    p.gamePath = obj[QStringLiteral("gamePath")].toString();
    p.modPath = obj[QStringLiteral("modPath")].toString();
    p.description = obj[QStringLiteral("description")].toString();
    p.isDefault = obj[QStringLiteral("isDefault")].toBool();
    p.isActive = obj[QStringLiteral("isActive")].toBool();

    const QJsonArray modsArray = obj[QStringLiteral("activeMods")].toArray();
    for (const auto &val : modsArray)
        p.activeMods.append(val.toString());

    return p;
}

} // namespace flatlas::domain
