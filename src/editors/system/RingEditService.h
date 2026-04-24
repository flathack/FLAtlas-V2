#pragma once

#include <QString>
#include <QStringList>

namespace flatlas::domain {
class SystemDocument;
class SolarObject;
}

namespace flatlas::editors {

struct RingEditState {
    bool enabled = true;
    QString ringIni;
    QString zoneNickname;
    double outerRadius = 3000.0;
    double innerRadius = 1500.0;
    double thickness = 500.0;
    double rotateX = 0.0;
    double rotateY = 0.0;
    double rotateZ = 0.0;
    QString sortValue = QStringLiteral("99.500000");
};

struct RingEditOptions {
    QStringList ringPresets;
};

class RingEditService
{
public:
    static RingEditOptions loadOptions(const QString &gameRoot);

    static bool canHostRing(const flatlas::domain::SolarObject &object);
    static int resolvedHostRadius(const flatlas::domain::SolarObject &object,
                                  const QString &gameRoot = QString());
    static bool hasRing(const flatlas::domain::SolarObject &object);
    static QString linkedZoneNickname(const flatlas::domain::SolarObject &object);
    static QString linkedRingIniPath(const flatlas::domain::SolarObject &object);
    static bool isValidZoneNickname(const QString &nickname);

    static RingEditState loadState(const flatlas::domain::SystemDocument *document,
                                   const flatlas::domain::SolarObject &object,
                                   const QString &gameRoot = QString());

    static flatlas::domain::SolarObject *findHostForZone(const flatlas::domain::SystemDocument *document,
                                                         const QString &zoneNickname);

    static bool apply(flatlas::domain::SystemDocument *document,
                      flatlas::domain::SolarObject *object,
                      const RingEditState &state,
                      QString *errorMessage = nullptr);
};

} // namespace flatlas::editors