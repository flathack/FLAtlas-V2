#pragma once
// editors/universe/UniverseSerializer.h – Universe.ini Laden/Speichern (Phase 9)

#include <QString>
#include <memory>
#include "domain/UniverseData.h"

namespace flatlas::editors {

/// Loads and saves Freelancer Universe.ini files.
class UniverseSerializer {
public:
    /// Load Universe.ini into UniverseData. Returns nullptr on error.
    static std::unique_ptr<flatlas::domain::UniverseData> load(const QString &filePath);

    /// Save UniverseData to Universe.ini. Returns true on success.
    static bool save(const flatlas::domain::UniverseData &data, const QString &filePath);
};

} // namespace flatlas::editors
