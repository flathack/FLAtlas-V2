#pragma once
// domain/ModProfile.h – Mod-Manager-Profil

#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace flatlas::domain {

struct ModProfile {
    QString name;
    QString id;
    QString gamePath;
    QString modPath;
    QString description;
    QStringList activeMods;
    bool isDefault = false;
    bool isActive = false;

    QJsonObject toJson() const;
    static ModProfile fromJson(const QJsonObject &obj);
};

} // namespace flatlas::domain
