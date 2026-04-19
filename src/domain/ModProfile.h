#pragma once
// domain/ModProfile.h – Mod-Manager-Profil
// TODO Phase 13
#include <QString>
#include <QStringList>
namespace flatlas::domain {
struct ModProfile {
    QString name;
    QString id; // SHA1-basiert
    QStringList activeMods;
    bool isDefault = false;
};
} // namespace flatlas::domain
