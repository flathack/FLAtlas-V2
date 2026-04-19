#pragma once
// editors/base/BaseBuilder.h – Neue Basis mit Templates erstellen (Phase 10)

#include <QString>
#include <QVector>
#include "domain/BaseData.h"

namespace flatlas::editors {

/// Predefined base template type.
enum class BaseTemplate {
    Station,   ///< Standard space station (Bar, Equip, Commodity, ShipDealer)
    Planet,    ///< Habitable planet (Bar, Equip, Commodity)
    Outpost,   ///< Small outpost (Bar, Equip)
    Wreck,     ///< Derelict/Wreck (no rooms)
};

class BaseBuilder {
public:
    /// Build a BaseData from a template with the given nickname and system.
    static flatlas::domain::BaseData create(BaseTemplate tmpl,
                                             const QString &nickname,
                                             const QString &system);

    /// List available template names for UI.
    static QStringList templateNames();

    /// Map a template name to its enum.
    static BaseTemplate templateFromName(const QString &name);

    /// Get the default rooms for a template.
    static QVector<flatlas::domain::RoomData> defaultRooms(BaseTemplate tmpl);
};

} // namespace flatlas::editors
