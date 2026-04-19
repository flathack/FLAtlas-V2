#pragma once
// editors/base/BasePersistence.h – Laden/Speichern von Basis-Daten (Phase 10)

#include <memory>
#include <QString>
#include "domain/BaseData.h"

namespace flatlas::editors {

class BasePersistence {
public:
    /// Load a base from an INI section block. filePath is for the system INI containing the base.
    static std::unique_ptr<flatlas::domain::BaseData> loadFromIni(const QString &filePath,
                                                                   const QString &baseNickname);

    /// Load all bases from a system INI file.
    static QVector<flatlas::domain::BaseData> loadAllBases(const QString &filePath);

    /// Save a single base back into a system INI (replaces existing [Object] and [BaseRoom] sections).
    static bool save(const flatlas::domain::BaseData &base, const QString &filePath);
};

} // namespace flatlas::editors
