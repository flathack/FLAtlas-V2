#pragma once

#include "domain/FactionData.h"

#include <QString>
#include <QStringList>

namespace flatlas::infrastructure {

struct FactionRepositoryResult {
    flatlas::domain::FactionWorld world;
    QStringList warnings;
};

class FactionRepository {
public:
    FactionRepositoryResult load(const QString &gameRoot) const;
    bool save(const flatlas::domain::FactionWorld &world, const QString &gameRoot, QString *errorMessage = nullptr) const;

    QString initialWorldPath(const QString &gameRoot) const;
    QString empathyPath(const QString &gameRoot) const;
    QString factionPropPath(const QString &gameRoot) const;
};

} // namespace flatlas::infrastructure
