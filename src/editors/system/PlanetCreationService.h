#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace flatlas::domain {
class SystemDocument;
}

namespace flatlas::editors {

struct PlanetArchetypeOption {
    QString archetype;
    int solarRadius = 0;
    QString suggestedInfocardText;
    QString sourceObjectNickname;
    int sourceIdsInfo = 0;
};

struct PlanetCreationCatalog {
    QVector<PlanetArchetypeOption> options;

    QStringList archetypeNicknames() const;
    PlanetArchetypeOption optionForArchetype(const QString &archetype) const;
    int solarRadiusForArchetype(const QString &archetype) const;
};

class PlanetCreationService {
public:
    static PlanetCreationCatalog loadCatalog(const flatlas::domain::SystemDocument *document,
                                            const QString &gameRoot);

    static int derivePlanetRadius(const QString &archetype,
                                  const PlanetCreationCatalog &catalog);
    static int defaultDeathZoneRadius(int solarRadius);
    static int defaultAtmosphereRange(int solarRadius);
    static QString wrapInfocardXml(const QString &plainText);
    static bool isValidNickname(const QString &nickname);
};

} // namespace flatlas::editors