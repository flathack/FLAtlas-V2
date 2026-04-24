#pragma once

#include <QString>
#include <QStringList>

namespace flatlas::domain {
class SystemDocument;
}

namespace flatlas::editors {

struct SystemSettingsState {
    QString systemNickname;
    QString musicSpace;
    QString musicDanger;
    QString musicBattle;
    QString spaceColor;
    QString localFaction;
    QString ambientColor;
    QString dust;
    QString backgroundBasicStars;
    QString backgroundComplexStars;
    QString backgroundNebulae;
};

struct SystemSettingsOptions {
    QStringList musicSpaceOptions;
    QStringList musicDangerOptions;
    QStringList musicBattleOptions;
    QStringList backgroundBasicStarsOptions;
    QStringList backgroundComplexStarsOptions;
    QStringList backgroundNebulaeOptions;
    QStringList factionDisplayOptions;
    QStringList dustOptions;
};

class SystemSettingsService {
public:
    static SystemSettingsState load(const flatlas::domain::SystemDocument *document);
    static SystemSettingsOptions loadOptions(const QString &gameRoot);

    static bool apply(flatlas::domain::SystemDocument *document,
                      const SystemSettingsState &state,
                      QString *errorMessage = nullptr);

    static bool normalizeRgbText(const QString &rawValue,
                                 QString *normalizedValue,
                                 QString *errorMessage = nullptr);

    static QString factionDisplayForNickname(const QString &nickname,
                                            const QStringList &displayOptions);
    static QString factionNicknameFromDisplay(const QString &rawDisplay);
};

} // namespace flatlas::editors
