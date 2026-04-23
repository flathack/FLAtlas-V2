#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

namespace flatlas::editors {

struct ExclusionShellSettings {
    bool enabled = false;
    int fogFar = 0;
    QString zoneShell;
    double shellScalar = 1.0;
    double maxAlpha = 0.5;
    QString exclusionTint;
};

class ExclusionZoneUtils {
public:
    static QString normalizeName(const QString &value);
    static QString generateExclusionNickname(const QString &systemNickname,
                                            const QString &fieldZoneNickname,
                                            const QStringList &existingZoneNicknames);
    static QVector<QPair<QString, QString>> buildZoneEntries(const QString &nickname,
                                                             const QString &shape,
                                                             const QVector3D &position,
                                                             const QVector3D &size,
                                                             const QVector3D &rotation,
                                                             const QString &comment,
                                                             int sort);

    static QStringList nebulaShellOptionsForGamePath(const QString &gamePath);
    static QPair<QString, bool> patchFieldIniExclusionSection(const QString &iniText,
                                                              const QString &exclusionZoneNickname,
                                                              const ExclusionShellSettings &shellSettings);
    static QPair<QString, bool> patchFieldIniRemoveExclusion(const QString &iniText,
                                                             const QString &exclusionZoneNickname);
    static ExclusionShellSettings readFieldIniExclusionSettings(const QString &iniText,
                                                                const QString &exclusionZoneNickname,
                                                                bool *found = nullptr);
};

} // namespace flatlas::editors
