#pragma once

#include "PlanetCreationService.h"
#include "RingEditService.h"

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedLayout;
class QTextEdit;
class QTimer;

namespace flatlas::rendering { class ModelViewport3D; }

namespace flatlas::editors {

struct CreateSimpleZoneRequest {
    QString nickname;
    QString comment;
    QString shape;
    int sort = 99;
    int damage = 0;
};

struct CreatePatrolZoneRequest {
    QString nickname;
    QString usage = QStringLiteral("patrol");
    QString comment;
    int sort = 99;
    int radius = 750;
    int damage = 0;
    int toughness = 6;
    int density = 3;
    int repopTime = 90;
    int maxBattleSize = 4;
    QString popType = QStringLiteral("attack_patrol");
    int reliefTime = 30;
    QString pathLabel = QStringLiteral("patrol");
    int pathIndex = 1;
    QString encounter;
    QString factionDisplay;
    int encounterLevel = 6;
    double encounterChance = 0.29;
    bool missionEligible = true;
};

struct CreateBuoyRequest {
    enum class Mode {
        Line,
        Circle,
    };
    enum class LineConstraint {
        FixedCount,
        FixedSpacing,
    };

    QString archetype = QStringLiteral("nav_buoy");
    Mode mode = Mode::Line;
    LineConstraint lineConstraint = LineConstraint::FixedCount;
    int count = 8;
    int spacingMeters = 3000;
};

struct CreateTradeLaneRequest {
    int ringCount = 2;
    int spacingMeters = 7500;
    int startNumber = 1;
    QString loadout;
    QString reputationDisplay;
    int difficultyLevel = 1;
    QString pilot = QStringLiteral("pilot_solar_easiest");
    QString routeName;
    QString startSpaceName;
    QString endSpaceName;
};

struct EditTradeLaneRequest {
    double startX = 0.0;
    double startZ = 0.0;
    double endX = 0.0;
    double endZ = 0.0;
    int ringCount = 2;
    QString archetype = QStringLiteral("Trade_Lane_Ring");
    QString loadout;
    QString reputationDisplay;
    int difficultyLevel = 1;
    QString pilot = QStringLiteral("pilot_solar_easiest");
    QString routeName;
    QString startSpaceName;
    QString endSpaceName;
};

struct CreateLightSourceRequest {
    QString nickname;
    QString type;
    QString color;
    int range = 100000;
    QString attenCurve;
};

struct CreateSunRequest {
    QString nickname;
    QString ingameName;
    QString infoCardText;
    QString archetype;
    QString burnColor;
    int deathZoneRadius = 2000;
    int deathZoneDamage = 200000;
    int atmosphereRange = 5000;
    QString star = QStringLiteral("med_white_sun");
};

struct CreatePlanetRequest {
    QString nickname;
    QString ingameName;
    QString infoCardText;
    QString archetype;
    int planetRadius = 0;
    int deathZoneRadius = 1100;
    int deathZoneDamage = 2000000;
    int atmosphereRange = 1200;
};

struct RingEditRequest {
    bool enabled = true;
    QString ringIni;
    QString zoneNickname;
    double outerRadius = 3000.0;
    double innerRadius = 1500.0;
    double thickness = 500.0;
    double rotateX = 0.0;
    double rotateY = 0.0;
    double rotateZ = 0.0;
};

struct CreateSurpriseRequest {
    QString nickname;
    QString ingameName;
    QString infoCardText;
    QString archetype;
    QString loadout;
    QString comment;
    int visit = 0;
};

struct CreateWeaponPlatformRequest {
    QString nickname;
    QString ingameName;
    QString archetype;
    QString loadout;
    QString reputation;
    QString behavior = QStringLiteral("NOTHING");
    QString pilot = QStringLiteral("pilot_solar_hard");
    int difficultyLevel = 6;
    int visit = 0;
};

class CreateSimpleZoneDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateSimpleZoneDialog(const QString &suggestedNickname,
                                    QWidget *parent = nullptr);

    CreateSimpleZoneRequest result() const;

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QComboBox *m_shapeCombo = nullptr;
    QSpinBox *m_sortSpin = nullptr;
    QSpinBox *m_damageSpin = nullptr;
};

class CreatePatrolZoneDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreatePatrolZoneDialog(const QString &suggestedNickname,
                                    const QStringList &encounters,
                                    const QStringList &factions,
                                    QWidget *parent = nullptr);

    CreatePatrolZoneRequest result() const;

private slots:
    void onUsageChanged(const QString &usage);

private:
    QStringList popTypesForUsage(const QString &usage) const;
    void applyPopTypeItems(const QString &usage);
    void applyEncounterDefault(const QString &usage);

    QLineEdit *m_nicknameEdit = nullptr;
    QComboBox *m_usageCombo = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QSpinBox *m_sortSpin = nullptr;
    QSpinBox *m_radiusSpin = nullptr;
    QSpinBox *m_damageSpin = nullptr;
    QSpinBox *m_toughnessSpin = nullptr;
    QSpinBox *m_densitySpin = nullptr;
    QSpinBox *m_repopSpin = nullptr;
    QSpinBox *m_battleSpin = nullptr;
    QComboBox *m_popTypeCombo = nullptr;
    QSpinBox *m_reliefSpin = nullptr;
    QLineEdit *m_pathLabelEdit = nullptr;
    QSpinBox *m_pathIndexSpin = nullptr;
    QComboBox *m_encounterCombo = nullptr;
    QComboBox *m_factionCombo = nullptr;
    QSpinBox *m_encounterLevelSpin = nullptr;
    QDoubleSpinBox *m_encounterChanceSpin = nullptr;
    QCheckBox *m_missionEligibleCheck = nullptr;
};

class CreateBuoyDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateBuoyDialog(QWidget *parent = nullptr);

    CreateBuoyRequest result() const;

private slots:
    void updateModeUi();
    void updateLineConstraintUi();

private:
    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_lineConstraintCombo = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QLabel *m_countDerivedLabel = nullptr;
    QLabel *m_spacingLabel = nullptr;
    QSpinBox *m_spacingSpin = nullptr;
    QLabel *m_spacingDerivedLabel = nullptr;
    QLabel *m_modeHintLabel = nullptr;
};

class CreateTradeLaneDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateTradeLaneDialog(const QString &systemNickname,
                                   int startNumber,
                                   int ringCount,
                                   double distanceMeters,
                                   const QStringList &loadouts,
                                   const QStringList &factions,
                                   const QStringList &pilots,
                                   QWidget *parent = nullptr);

    CreateTradeLaneRequest result() const;

private slots:
    void updateDerivedSpacing();
    void updateCountFromSpacing(int spacingMeters);

private:
    double m_distanceMeters = 0.0;
    QSpinBox *m_countSpin = nullptr;
    QSpinBox *m_spacingSpin = nullptr;
    QSpinBox *m_startNumberSpin = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_reputationCombo = nullptr;
    QSpinBox *m_difficultySpin = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QLineEdit *m_routeNameEdit = nullptr;
    QLineEdit *m_startSpaceNameEdit = nullptr;
    QLineEdit *m_endSpaceNameEdit = nullptr;
    QLabel *m_spacingInfoLabel = nullptr;
    bool m_updatingSpacing = false;
};

class EditTradeLaneDialog : public QDialog {
    Q_OBJECT
public:
    explicit EditTradeLaneDialog(const QString &laneSummary,
                                 const QStringList &archetypes,
                                 const QStringList &loadouts,
                                 const QStringList &factions,
                                 const QStringList &pilots,
                                 const EditTradeLaneRequest &initial,
                                 QWidget *parent = nullptr);

    EditTradeLaneRequest result() const;
    bool deleteRequested() const;

private slots:
    void updateDerivedSpacing();
    void updateCountFromSpacing(int spacingMeters);

private:
    double currentLaneLengthMeters() const;

    QDoubleSpinBox *m_startXSpin = nullptr;
    QDoubleSpinBox *m_startZSpin = nullptr;
    QDoubleSpinBox *m_endXSpin = nullptr;
    QDoubleSpinBox *m_endZSpin = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QSpinBox *m_spacingSpin = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_reputationCombo = nullptr;
    QSpinBox *m_difficultySpin = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QLineEdit *m_routeNameEdit = nullptr;
    QLineEdit *m_startSpaceNameEdit = nullptr;
    QLineEdit *m_endSpaceNameEdit = nullptr;
    QLabel *m_spacingInfoLabel = nullptr;
    bool m_deleteRequested = false;
    bool m_updatingSpacing = false;
};

class CreateLightSourceDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateLightSourceDialog(const QString &suggestedNickname,
                                     const QStringList &types,
                                     const QStringList &attenCurves,
                                     QWidget *parent = nullptr);

    CreateLightSourceRequest result() const;

private slots:
    void pickColor();

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QPushButton *m_colorButton = nullptr;
    QLineEdit *m_colorLabel = nullptr;
    QSpinBox *m_rangeSpin = nullptr;
    QComboBox *m_attenCurveCombo = nullptr;
};

class CreateSunDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateSunDialog(const QString &suggestedNickname,
                             const QStringList &archetypes,
                             const QStringList &stars,
                             QWidget *parent = nullptr);

    CreateSunRequest result() const;

private slots:
    void pickBurnColor();

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QTextEdit *m_infoCardEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QPushButton *m_burnColorButton = nullptr;
    QLineEdit *m_burnColorLabel = nullptr;
    QSpinBox *m_deathZoneRadiusSpin = nullptr;
    QSpinBox *m_deathZoneDamageSpin = nullptr;
    QSpinBox *m_atmosphereRangeSpin = nullptr;
    QComboBox *m_starCombo = nullptr;
};

class CreatePlanetDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreatePlanetDialog(const QString &suggestedNickname,
                                const PlanetCreationCatalog &catalog,
                                QWidget *parent = nullptr);

    CreatePlanetRequest result() const;

public slots:
    void accept() override;

private slots:
    void onArchetypeChanged(const QString &archetype);
    void onInfocardEdited();
    void resetInfocardSuggestion();

private:
    void applyArchetypeDefaults(const QString &archetype, bool forceInfocardRefresh);
    void setInfocardText(const QString &text);

    PlanetCreationCatalog m_catalog;
    QString m_lastSuggestedInfocard;
    bool m_infocardManuallyEdited = false;
    bool m_updatingInfocardText = false;
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QLabel *m_planetRadiusLabel = nullptr;
    QSpinBox *m_deathZoneRadiusSpin = nullptr;
    QSpinBox *m_deathZoneDamageSpin = nullptr;
    QSpinBox *m_atmosphereRangeSpin = nullptr;
    QLabel *m_infocardSourceLabel = nullptr;
    QLabel *m_infocardStateLabel = nullptr;
    QTextEdit *m_infoCardEdit = nullptr;
    QPushButton *m_resetInfocardButton = nullptr;
};

class ObjectRingDialog : public QDialog {
    Q_OBJECT
public:
    explicit ObjectRingDialog(const QString &objectLabel,
                              const QString &hostArchetype,
                              bool showHostRadiusSphere,
                              double hostRadius,
                              bool hostRadiusSphereIsSun,
                              const RingEditOptions &options,
                              const RingEditState &initialState,
                              QWidget *parent = nullptr);

    RingEditRequest result() const;

protected:
    void accept() override;

private slots:
    void syncEnabledState();
    void schedulePreviewRefresh();
    void refreshPreview();

private:
    QString m_hostArchetype;
    bool m_showHostRadiusSphere = false;
    double m_hostRadius = 0.0;
    bool m_hostRadiusSphereIsSun = false;
    QCheckBox *m_enabledCheck = nullptr;
    QComboBox *m_ringIniCombo = nullptr;
    QLineEdit *m_zoneNicknameEdit = nullptr;
    QDoubleSpinBox *m_outerRadiusSpin = nullptr;
    QDoubleSpinBox *m_innerRadiusSpin = nullptr;
    QDoubleSpinBox *m_thicknessSpin = nullptr;
    QDoubleSpinBox *m_rotateXSpin = nullptr;
    QDoubleSpinBox *m_rotateYSpin = nullptr;
    QDoubleSpinBox *m_rotateZSpin = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    QStackedLayout *m_previewStack = nullptr;
    QLabel *m_previewStatusLabel = nullptr;
    QTimer *m_previewRefreshTimer = nullptr;
};

class CreateSurpriseDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateSurpriseDialog(const QString &suggestedNickname,
                                  const QStringList &archetypes,
                                  const QStringList &loadouts,
                                  QWidget *parent = nullptr);

    CreateSurpriseRequest result() const;

private:
    void schedulePreviewRefresh();
    void refreshPreview();
    void updateLoadoutContents();

    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QTextEdit *m_infoCardEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QListWidget *m_loadoutContentsList = nullptr;
    QLabel *m_loadoutContentsStatus = nullptr;
    QComboBox *m_visitCombo = nullptr;
    QLabel *m_visitHintLabel = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    QStackedLayout *m_previewStack = nullptr;
    QTimer *m_previewRefreshTimer = nullptr;
    int m_previewRefreshGeneration = 0;
    QHash<QString, QString> m_archetypeModelPaths;
    QHash<QString, QStringList> m_loadoutContents;
};

class CreateWeaponPlatformDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateWeaponPlatformDialog(const QString &suggestedNickname,
                                        const QStringList &archetypes,
                                        const QStringList &loadouts,
                                        const QStringList &factions,
                                        QWidget *parent = nullptr);

    CreateWeaponPlatformRequest result() const;

private slots:
    void schedulePreviewRefresh();
    void refreshPreview();
    void updateLoadoutContents();

private:
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_ingameNameEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_factionCombo = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QLineEdit *m_behaviorEdit = nullptr;
    QSpinBox *m_difficultySpin = nullptr;
    QListWidget *m_loadoutContentsList = nullptr;
    QLabel *m_loadoutContentsStatus = nullptr;
    QComboBox *m_visitCombo = nullptr;
    QLabel *m_visitHintLabel = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    QStackedLayout *m_previewStack = nullptr;
    QTimer *m_previewRefreshTimer = nullptr;
    int m_previewRefreshGeneration = 0;
    QHash<QString, QString> m_archetypeModelPaths;
    QHash<QString, QStringList> m_loadoutContents;
};

} // namespace flatlas::editors
