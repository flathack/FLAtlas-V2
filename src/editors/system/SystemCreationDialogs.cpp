#include "SystemCreationDialogs.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringListModel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace flatlas::editors {

namespace {

void configureContainsCompleter(QComboBox *combo)
{
    if (!combo)
        return;
    QStringList values;
    values.reserve(combo->count());
    for (int index = 0; index < combo->count(); ++index) {
        const QString text = combo->itemText(index).trimmed();
        if (!text.isEmpty())
            values.append(text);
    }
    values.removeDuplicates();
    auto *model = new QStringListModel(values, combo);
    auto *completer = new QCompleter(model, combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);
}

QDialogButtonBox *createDialogButtons(QDialog *dialog)
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Erstellen"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QComboBox *createEditableCombo(const QStringList &values, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItems(values);
    configureContainsCompleter(combo);
    return combo;
}

} // namespace

CreateSimpleZoneDialog::CreateSimpleZoneDialog(const QString &suggestedNickname, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Zone erstellen"));
    setMinimumWidth(420);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("optional"));
    layout->addRow(tr("Kommentar:"), m_commentEdit);

    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->addItems({QStringLiteral("SPHERE"),
                            QStringLiteral("ELLIPSOID"),
                            QStringLiteral("BOX"),
                            QStringLiteral("CYLINDER"),
                            QStringLiteral("RING")});
    layout->addRow(tr("Shape:"), m_shapeCombo);

    m_sortSpin = new QSpinBox(this);
    m_sortSpin->setRange(0, 999);
    m_sortSpin->setValue(99);
    layout->addRow(tr("Sort:"), m_sortSpin);

    m_damageSpin = new QSpinBox(this);
    m_damageSpin->setRange(0, 2000000);
    m_damageSpin->setValue(0);
    layout->addRow(tr("Damage:"), m_damageSpin);

    layout->addRow(createDialogButtons(this));
}

CreateSimpleZoneRequest CreateSimpleZoneDialog::result() const
{
    CreateSimpleZoneRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.comment = m_commentEdit->text().trimmed();
    value.shape = m_shapeCombo->currentText().trimmed().toUpper();
    value.sort = m_sortSpin->value();
    value.damage = m_damageSpin->value();
    return value;
}

CreatePatrolZoneDialog::CreatePatrolZoneDialog(const QString &suggestedNickname,
                                               const QStringList &encounters,
                                               const QStringList &factions,
                                               QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Patrol-Zone erstellen"));
    setMinimumWidth(520);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_usageCombo = new QComboBox(this);
    m_usageCombo->addItems({QStringLiteral("patrol"), QStringLiteral("trade")});
    m_usageCombo->setCurrentText(QStringLiteral("patrol"));
    layout->addRow(tr("Usage:"), m_usageCombo);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("optional"));
    layout->addRow(tr("Kommentar:"), m_commentEdit);

    m_sortSpin = new QSpinBox(this);
    m_sortSpin->setRange(0, 999);
    m_sortSpin->setValue(99);
    layout->addRow(tr("Sort:"), m_sortSpin);

    m_radiusSpin = new QSpinBox(this);
    m_radiusSpin->setRange(100, 50000);
    m_radiusSpin->setValue(750);
    layout->addRow(tr("Cylinder Radius:"), m_radiusSpin);

    m_damageSpin = new QSpinBox(this);
    m_damageSpin->setRange(0, 2000000);
    m_damageSpin->setValue(0);
    layout->addRow(tr("Damage:"), m_damageSpin);

    m_toughnessSpin = new QSpinBox(this);
    m_toughnessSpin->setRange(0, 100);
    m_toughnessSpin->setValue(6);
    layout->addRow(tr("Toughness:"), m_toughnessSpin);

    m_densitySpin = new QSpinBox(this);
    m_densitySpin->setRange(0, 100);
    m_densitySpin->setValue(3);
    layout->addRow(tr("Density:"), m_densitySpin);

    m_repopSpin = new QSpinBox(this);
    m_repopSpin->setRange(0, 10000);
    m_repopSpin->setValue(90);
    layout->addRow(tr("Repop Time:"), m_repopSpin);

    m_battleSpin = new QSpinBox(this);
    m_battleSpin->setRange(0, 10000);
    m_battleSpin->setValue(4);
    layout->addRow(tr("Max Battle Size:"), m_battleSpin);

    m_popTypeCombo = createEditableCombo({}, this);
    layout->addRow(tr("Pop Type:"), m_popTypeCombo);

    m_reliefSpin = new QSpinBox(this);
    m_reliefSpin->setRange(0, 10000);
    m_reliefSpin->setValue(30);
    layout->addRow(tr("Relief Time:"), m_reliefSpin);

    m_pathLabelEdit = new QLineEdit(QStringLiteral("patrol"), this);
    layout->addRow(tr("Path Label:"), m_pathLabelEdit);

    m_pathIndexSpin = new QSpinBox(this);
    m_pathIndexSpin->setRange(1, 999);
    m_pathIndexSpin->setValue(1);
    layout->addRow(tr("Path Index:"), m_pathIndexSpin);

    m_encounterCombo = createEditableCombo(encounters, this);
    layout->addRow(tr("Encounter:"), m_encounterCombo);

    m_factionCombo = createEditableCombo(factions, this);
    layout->addRow(tr("Faction:"), m_factionCombo);

    m_encounterLevelSpin = new QSpinBox(this);
    m_encounterLevelSpin->setRange(1, 100);
    m_encounterLevelSpin->setValue(6);
    layout->addRow(tr("Encounter Level:"), m_encounterLevelSpin);

    m_encounterChanceSpin = new QDoubleSpinBox(this);
    m_encounterChanceSpin->setRange(0.0, 1.0);
    m_encounterChanceSpin->setDecimals(2);
    m_encounterChanceSpin->setSingleStep(0.01);
    m_encounterChanceSpin->setValue(0.29);
    layout->addRow(tr("Encounter Chance:"), m_encounterChanceSpin);

    m_missionEligibleCheck = new QCheckBox(tr("Mission Eligible"), this);
    m_missionEligibleCheck->setChecked(true);
    layout->addRow(QString(), m_missionEligibleCheck);

    applyPopTypeItems(m_usageCombo->currentText());
    applyEncounterDefault(m_usageCombo->currentText());
    connect(m_usageCombo, &QComboBox::currentTextChanged, this, &CreatePatrolZoneDialog::onUsageChanged);
    layout->addRow(createDialogButtons(this));
}

QStringList CreatePatrolZoneDialog::popTypesForUsage(const QString &usage) const
{
    const QString normalized = usage.trimmed().toLower();
    if (normalized == QStringLiteral("trade"))
        return {QStringLiteral("trade_path"), QStringLiteral("mining_path")};
    return {QStringLiteral("lane_patrol"),
            QStringLiteral("attack_patrol"),
            QStringLiteral("field_patrol"),
            QStringLiteral("mining_path"),
            QStringLiteral("scavenger_path")};
}

void CreatePatrolZoneDialog::applyPopTypeItems(const QString &usage)
{
    const QString current = m_popTypeCombo ? m_popTypeCombo->currentText().trimmed() : QString();
    const QStringList values = popTypesForUsage(usage);
    m_popTypeCombo->clear();
    m_popTypeCombo->addItems(values);
    configureContainsCompleter(m_popTypeCombo);
    if (!current.isEmpty())
        m_popTypeCombo->setCurrentText(current);
    else if (!values.isEmpty())
        m_popTypeCombo->setCurrentText(values.first());
}

void CreatePatrolZoneDialog::applyEncounterDefault(const QString &usage)
{
    const QString current = m_encounterCombo ? m_encounterCombo->currentText().trimmed() : QString();
    QStringList allItems;
    if (m_encounterCombo) {
        for (int index = 0; index < m_encounterCombo->count(); ++index)
            allItems.append(m_encounterCombo->itemText(index));
    }

    const QStringList preferred =
        usage.trimmed().compare(QStringLiteral("trade"), Qt::CaseInsensitive) == 0
        ? QStringList{QStringLiteral("tradep_trade_armored"),
                      QStringLiteral("tradep_trade_trader"),
                      QStringLiteral("tradep_trade_transport")}
        : QStringList{QStringLiteral("patrolp_assault"),
                      QStringLiteral("patrolp_bh_assault"),
                      QStringLiteral("patrolp_gov_assault")};

    QString chosen = current;
    if (chosen.isEmpty() || !allItems.contains(chosen, Qt::CaseInsensitive)) {
        for (const QString &candidate : preferred) {
            if (allItems.contains(candidate, Qt::CaseInsensitive)) {
                chosen = candidate;
                break;
            }
        }
        if (chosen.isEmpty() && !preferred.isEmpty())
            chosen = preferred.first();
    }

    m_encounterCombo->setCurrentText(chosen);
}

void CreatePatrolZoneDialog::onUsageChanged(const QString &usage)
{
    applyPopTypeItems(usage);
    applyEncounterDefault(usage);
    if (usage.trimmed().compare(QStringLiteral("trade"), Qt::CaseInsensitive) == 0) {
        m_encounterLevelSpin->setValue(6);
        m_encounterChanceSpin->setValue(0.40);
        m_reliefSpin->setValue(20);
    } else {
        m_encounterLevelSpin->setValue(6);
        m_encounterChanceSpin->setValue(0.29);
        m_reliefSpin->setValue(30);
    }
}

CreatePatrolZoneRequest CreatePatrolZoneDialog::result() const
{
    CreatePatrolZoneRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.usage = m_usageCombo->currentText().trimmed().toLower();
    value.comment = m_commentEdit->text().trimmed();
    value.sort = m_sortSpin->value();
    value.radius = m_radiusSpin->value();
    value.damage = m_damageSpin->value();
    value.toughness = m_toughnessSpin->value();
    value.density = m_densitySpin->value();
    value.repopTime = m_repopSpin->value();
    value.maxBattleSize = m_battleSpin->value();
    value.popType = m_popTypeCombo->currentText().trimmed();
    value.reliefTime = m_reliefSpin->value();
    value.pathLabel = m_pathLabelEdit->text().trimmed();
    value.pathIndex = m_pathIndexSpin->value();
    value.encounter = m_encounterCombo->currentText().trimmed();
    value.factionDisplay = m_factionCombo->currentText().trimmed();
    value.encounterLevel = m_encounterLevelSpin->value();
    value.encounterChance = m_encounterChanceSpin->value();
    value.missionEligible = m_missionEligibleCheck->isChecked();
    return value;
}

CreateLightSourceDialog::CreateLightSourceDialog(const QString &suggestedNickname,
                                                 const QStringList &types,
                                                 const QStringList &attenCurves,
                                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Lichtquelle erstellen"));
    setMinimumWidth(460);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    QStringList typeValues = types;
    if (!typeValues.contains(QStringLiteral("DIRECTIONAL"), Qt::CaseInsensitive))
        typeValues.prepend(QStringLiteral("DIRECTIONAL"));
    if (!typeValues.contains(QStringLiteral("POINT"), Qt::CaseInsensitive))
        typeValues.append(QStringLiteral("POINT"));
    typeValues.removeDuplicates();
    m_typeCombo = createEditableCombo(typeValues, this);
    m_typeCombo->setCurrentText(QStringLiteral("DIRECTIONAL"));
    layout->addRow(tr("Type:"), m_typeCombo);

    auto *colorRow = new QWidget(this);
    auto *colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    m_colorButton = new QPushButton(tr("Farbe wählen"), colorRow);
    m_colorLabel = new QLineEdit(QStringLiteral("255, 255, 255"), colorRow);
    m_colorLabel->setReadOnly(true);
    colorLayout->addWidget(m_colorButton);
    colorLayout->addWidget(m_colorLabel, 1);
    layout->addRow(tr("Color:"), colorRow);

    m_rangeSpin = new QSpinBox(this);
    m_rangeSpin->setRange(1, 2000000);
    m_rangeSpin->setValue(100000);
    layout->addRow(tr("Range:"), m_rangeSpin);

    QStringList attenValues = attenCurves;
    if (!attenValues.contains(QStringLiteral("DYNAMIC_DIRECTION"), Qt::CaseInsensitive))
        attenValues.prepend(QStringLiteral("DYNAMIC_DIRECTION"));
    attenValues.removeDuplicates();
    m_attenCurveCombo = createEditableCombo(attenValues, this);
    m_attenCurveCombo->setCurrentText(QStringLiteral("DYNAMIC_DIRECTION"));
    layout->addRow(tr("atten_curve:"), m_attenCurveCombo);

    connect(m_colorButton, &QPushButton::clicked, this, &CreateLightSourceDialog::pickColor);
    layout->addRow(createDialogButtons(this));
}

void CreateLightSourceDialog::pickColor()
{
    const QColor color = QColorDialog::getColor(Qt::white, this, tr("Farbe wählen"));
    if (!color.isValid())
        return;
    m_colorLabel->setText(QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()));
}

CreateLightSourceRequest CreateLightSourceDialog::result() const
{
    CreateLightSourceRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.type = m_typeCombo->currentText().trimmed().toUpper();
    value.color = m_colorLabel->text().trimmed();
    value.range = m_rangeSpin->value();
    value.attenCurve = m_attenCurveCombo->currentText().trimmed();
    return value;
}

CreateSunDialog::CreateSunDialog(const QString &suggestedNickname,
                                 const QStringList &archetypes,
                                 const QStringList &stars,
                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sonne erstellen"));
    setMinimumWidth(520);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(this);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    layout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_infoCardEdit = new QTextEdit(this);
    m_infoCardEdit->setMinimumHeight(100);
    m_infoCardEdit->setPlaceholderText(tr("Infocard-Text (optional)"));
    layout->addRow(tr("ids_info Text:"), m_infoCardEdit);

    QStringList archValues = archetypes;
    if (archValues.isEmpty())
        archValues << QStringLiteral("sun_2000");
    m_archetypeCombo = createEditableCombo(archValues, this);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    layout->addRow(tr("Archetype:"), m_archetypeCombo);

    auto *burnRow = new QWidget(this);
    auto *burnLayout = new QHBoxLayout(burnRow);
    burnLayout->setContentsMargins(0, 0, 0, 0);
    m_burnColorButton = new QPushButton(tr("Farbe wählen"), burnRow);
    m_burnColorLabel = new QLineEdit(burnRow);
    m_burnColorLabel->setReadOnly(true);
    m_burnColorLabel->setPlaceholderText(tr("optional"));
    burnLayout->addWidget(m_burnColorButton);
    burnLayout->addWidget(m_burnColorLabel, 1);
    layout->addRow(tr("Burn Color:"), burnRow);

    m_deathZoneRadiusSpin = new QSpinBox(this);
    m_deathZoneRadiusSpin->setRange(100, 2000000);
    m_deathZoneRadiusSpin->setValue(2000);
    layout->addRow(tr("Death-Zone Radius:"), m_deathZoneRadiusSpin);

    m_deathZoneDamageSpin = new QSpinBox(this);
    m_deathZoneDamageSpin->setRange(1, 2000000);
    m_deathZoneDamageSpin->setValue(200000);
    layout->addRow(tr("Death-Zone Damage:"), m_deathZoneDamageSpin);

    m_atmosphereRangeSpin = new QSpinBox(this);
    m_atmosphereRangeSpin->setRange(0, 2000000);
    m_atmosphereRangeSpin->setValue(5000);
    layout->addRow(tr("atmosphere_range:"), m_atmosphereRangeSpin);

    QStringList starValues = stars;
    if (!starValues.contains(QStringLiteral("med_white_sun"), Qt::CaseInsensitive))
        starValues.prepend(QStringLiteral("med_white_sun"));
    starValues.removeDuplicates();
    m_starCombo = createEditableCombo(starValues, this);
    m_starCombo->setCurrentText(QStringLiteral("med_white_sun"));
    layout->addRow(tr("Star:"), m_starCombo);

    connect(m_burnColorButton, &QPushButton::clicked, this, &CreateSunDialog::pickBurnColor);
    layout->addRow(createDialogButtons(this));
}

void CreateSunDialog::pickBurnColor()
{
    const QColor color = QColorDialog::getColor(Qt::white, this, tr("Farbe wählen"));
    if (!color.isValid())
        return;
    m_burnColorLabel->setText(QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()));
}

CreateSunRequest CreateSunDialog::result() const
{
    CreateSunRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.ingameName = m_ingameNameEdit->text().trimmed();
    value.infoCardText = m_infoCardEdit->toPlainText().trimmed();
    value.archetype = m_archetypeCombo->currentText().trimmed();
    value.burnColor = m_burnColorLabel->text().trimmed();
    value.deathZoneRadius = m_deathZoneRadiusSpin->value();
    value.deathZoneDamage = m_deathZoneDamageSpin->value();
    value.atmosphereRange = m_atmosphereRangeSpin->value();
    value.star = m_starCombo->currentText().trimmed();
    return value;
}

CreateSurpriseDialog::CreateSurpriseDialog(const QString &suggestedNickname,
                                           const QStringList &archetypes,
                                           const QStringList &loadouts,
                                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Surprise erstellen"));
    setMinimumWidth(520);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(this);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    layout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_archetypeCombo = createEditableCombo(archetypes, this);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    layout->addRow(tr("Archetype:"), m_archetypeCombo);

    QStringList loadoutValues = loadouts;
    loadoutValues.prepend(QString());
    loadoutValues.removeDuplicates();
    m_loadoutCombo = createEditableCombo(loadoutValues, this);
    m_loadoutCombo->setCurrentIndex(0);
    layout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("optional"));
    layout->addRow(tr("Kommentar:"), m_commentEdit);

    layout->addRow(createDialogButtons(this));
}

CreateSurpriseRequest CreateSurpriseDialog::result() const
{
    CreateSurpriseRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.ingameName = m_ingameNameEdit->text().trimmed();
    value.archetype = m_archetypeCombo->currentText().trimmed();
    value.loadout = m_loadoutCombo->currentText().trimmed();
    value.comment = m_commentEdit->text().trimmed();
    return value;
}

CreateWeaponPlatformDialog::CreateWeaponPlatformDialog(const QString &suggestedNickname,
                                                       const QStringList &archetypes,
                                                       const QStringList &loadouts,
                                                       const QStringList &factions,
                                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Waffenplattform erstellen"));
    setMinimumWidth(560);

    auto *layout = new QFormLayout(this);
    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    layout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(this);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    layout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_archetypeCombo = createEditableCombo(archetypes, this);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    layout->addRow(tr("Archetype:"), m_archetypeCombo);

    QStringList loadoutValues = loadouts;
    if (!loadoutValues.contains(QString(), Qt::CaseSensitive))
        loadoutValues.prepend(QString());
    loadoutValues.removeDuplicates();
    m_loadoutCombo = createEditableCombo(loadoutValues, this);
    layout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_factionCombo = createEditableCombo(factions, this);
    layout->addRow(tr("Reputation:"), m_factionCombo);

    m_behaviorEdit = new QLineEdit(QStringLiteral("NOTHING"), this);
    layout->addRow(tr("Behavior:"), m_behaviorEdit);

    m_pilotEdit = new QLineEdit(QStringLiteral("pilot_solar_hard"), this);
    layout->addRow(tr("Pilot:"), m_pilotEdit);

    m_difficultySpin = new QSpinBox(this);
    m_difficultySpin->setRange(0, 100);
    m_difficultySpin->setValue(6);
    layout->addRow(tr("difficulty_level:"), m_difficultySpin);

    m_visitCheck = new QCheckBox(tr("visit setzen"), this);
    layout->addRow(QString(), m_visitCheck);

    m_visitSpin = new QSpinBox(this);
    m_visitSpin->setRange(0, 1000000);
    m_visitSpin->setValue(0);
    m_visitSpin->setEnabled(false);
    layout->addRow(tr("Visit:"), m_visitSpin);

    connect(m_visitCheck, &QCheckBox::toggled, this, &CreateWeaponPlatformDialog::updateVisitEnabledState);
    layout->addRow(createDialogButtons(this));
}

void CreateWeaponPlatformDialog::updateVisitEnabledState()
{
    if (m_visitSpin)
        m_visitSpin->setEnabled(m_visitCheck && m_visitCheck->isChecked());
}

CreateWeaponPlatformRequest CreateWeaponPlatformDialog::result() const
{
    CreateWeaponPlatformRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.ingameName = m_ingameNameEdit->text().trimmed();
    value.archetype = m_archetypeCombo->currentText().trimmed();
    value.loadout = m_loadoutCombo->currentText().trimmed();
    value.reputation = m_factionCombo->currentText().trimmed();
    value.behavior = m_behaviorEdit->text().trimmed();
    value.pilot = m_pilotEdit->text().trimmed();
    value.difficultyLevel = m_difficultySpin->value();
    value.hasVisit = m_visitCheck->isChecked();
    value.visit = m_visitSpin->value();
    return value;
}

} // namespace flatlas::editors
