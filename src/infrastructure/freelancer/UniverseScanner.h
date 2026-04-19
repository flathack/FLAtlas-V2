#pragma once
// infrastructure/freelancer/UniverseScanner.h – Universe-INI scannen
#include <QString>
#include <QVector>
#include "domain/UniverseData.h"
namespace flatlas::infrastructure {
class UniverseScanner {
public:
    /// universe.ini parsen und alle Systeme finden.
    static flatlas::domain::UniverseData scan(const QString &dataDir);
};
} // namespace flatlas::infrastructure
