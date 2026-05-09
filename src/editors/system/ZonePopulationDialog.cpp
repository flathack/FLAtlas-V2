#include "ZonePopulationDialog.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStringListModel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

namespace {

const QSet<QString> kPopulationKeys{
    QStringLiteral("toughness"),
    QStringLiteral("density"),
    QStringLiteral("repop_time"),
    QStringLiteral("max_battle_size"),
    QStringLiteral("pop_type"),
    QStringLiteral("relief_time"),
};

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

bool containsCaseInsensitive(const QStringList &values, const QString &needle)
{
    return values.contains(needle, Qt::CaseInsensitive);
}

} // namespace

ZonePopulationDialog::ZonePopulationDialog(const QString &zoneNickname,
                                           const QVector<QPair<QString, QString>> &entries,
                                           const QStringList &encounterParameters,
                                           const QStringList &allEncounters,
                                           const QStringList &factions,
                                           QWidget *parent)
    : QDialog(parent),
      m_encounterParameters(encounterParameters),
      m_allEncounters(allEncounters),
      m_factions(factions)
{
    setWindowTitle(tr("Zone Population - %1").arg(zoneNickname));
    setMinimumSize(720, 580);

    m_encounterParameters.removeDuplicates();
    m_allEncounters.removeDuplicates();
    m_factions.removeDuplicates();
    std::sort(m_encounterParameters.begin(), m_encounterParameters.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    std::sort(m_allEncounters.begin(), m_allEncounters.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });
    std::sort(m_factions.begin(), m_factions.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    QHash<QString, QString> pop;
    QVector<EncounterRow> encounters;
    QStringList restrictions;
    EncounterRow *currentEncounter = nullptr;

    for (const auto &entry : entries) {
        const QString key = entry.first.trimmed();
        const QString normalizedKey = key.toLower();
        const QString value = entry.second.trimmed();
        if (kPopulationKeys.contains(normalizedKey)) {
            pop.insert(normalizedKey, value);
        } else if (normalizedKey == QLatin1String("density_restriction")) {
            restrictions.append(value);
        } else if (normalizedKey == QLatin1String("encounter")) {
            const QStringList parts = value.split(QLatin1Char(','), Qt::KeepEmptyParts);
            EncounterRow row;
            row.name = parts.value(0).trimmed();
            row.level = parts.value(1, QStringLiteral("1")).trimmed();
            row.chance = parts.value(2, QStringLiteral("0.100000")).trimmed();
            encounters.append(row);
            currentEncounter = &encounters.last();
        } else if (normalizedKey == QLatin1String("faction") && currentEncounter) {
            const QStringList parts = value.split(QLatin1Char(','), Qt::KeepEmptyParts);
            currentEncounter->factions.append({parts.value(0).trimmed(),
                                               parts.value(1, QStringLiteral("1.000000")).trimmed()});
        } else {
            m_otherEntries.append(entry);
        }
    }

    m_initialPopType = pop.value(QStringLiteral("pop_type")).trimmed().toLower();
    m_profile = inferProfile();
    m_defaults = defaultsForProfile(m_profile);

    auto *mainLayout = new QVBoxLayout(this);

    auto *populationGroup = new QGroupBox(tr("Population-Parameter"), this);
    auto *form = new QFormLayout(populationGroup);

    m_toughnessSpin = new QSpinBox(populationGroup);
    m_toughnessSpin->setRange(0, 100);
    m_toughnessSpin->setValue(toInt(pop.value(QStringLiteral("toughness"), m_defaults.value(QStringLiteral("toughness")))));
    form->addRow(tr("Toughness:"), m_toughnessSpin);

    m_densitySpin = new QSpinBox(populationGroup);
    m_densitySpin->setRange(0, 100);
    m_densitySpin->setValue(toInt(pop.value(QStringLiteral("density"), m_defaults.value(QStringLiteral("density")))));
    form->addRow(tr("Density:"), m_densitySpin);

    m_repopSpin = new QSpinBox(populationGroup);
    m_repopSpin->setRange(0, 9999);
    m_repopSpin->setValue(toInt(pop.value(QStringLiteral("repop_time"), m_defaults.value(QStringLiteral("repop_time")))));
    form->addRow(tr("Repop Time:"), m_repopSpin);

    m_battleSpin = new QSpinBox(populationGroup);
    m_battleSpin->setRange(0, 100);
    m_battleSpin->setValue(toInt(pop.value(QStringLiteral("max_battle_size"), m_defaults.value(QStringLiteral("max_battle_size")))));
    form->addRow(tr("Max Battle Size:"), m_battleSpin);

    m_popTypeCombo = new QComboBox(populationGroup);
    m_popTypeCombo->setEditable(true);
    m_popTypeCombo->addItems(popTypesForProfile(m_profile));
    m_popTypeCombo->setCurrentText(pop.value(QStringLiteral("pop_type"), m_defaults.value(QStringLiteral("pop_type"))));
    configureContainsCompleter(m_popTypeCombo);
    form->addRow(tr("Pop Type:"), m_popTypeCombo);

    m_reliefSpin = new QSpinBox(populationGroup);
    m_reliefSpin->setRange(0, 9999);
    m_reliefSpin->setValue(toInt(pop.value(QStringLiteral("relief_time"), m_defaults.value(QStringLiteral("relief_time")))));
    form->addRow(tr("Relief Time:"), m_reliefSpin);

    auto *profileLabel = new QLabel(profileSummaryText(m_profile), populationGroup);
    profileLabel->setWordWrap(true);
    form->addRow(tr("Zone Style:"), profileLabel);
    mainLayout->addWidget(populationGroup);

    auto *restrictionGroup = new QGroupBox(tr("Density Restrictions"), this);
    auto *restrictionLayout = new QVBoxLayout(restrictionGroup);
    m_densityList = new QListWidget(restrictionGroup);
    m_densityList->setMaximumHeight(120);
    for (const QString &restriction : restrictions) {
        auto *item = new QListWidgetItem(restriction, m_densityList);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    restrictionLayout->addWidget(m_densityList);
    auto *restrictionButtons = new QHBoxLayout();
    auto *addRestrictionButton = new QPushButton(tr("+ Hinzufügen"), restrictionGroup);
    auto *removeRestrictionButton = new QPushButton(tr("Entfernen"), restrictionGroup);
    connect(addRestrictionButton, &QPushButton::clicked, this, &ZonePopulationDialog::addDensityRestriction);
    connect(removeRestrictionButton, &QPushButton::clicked, this, &ZonePopulationDialog::removeDensityRestriction);
    restrictionButtons->addWidget(addRestrictionButton);
    restrictionButtons->addWidget(removeRestrictionButton);
    restrictionButtons->addStretch(1);
    restrictionLayout->addLayout(restrictionButtons);
    mainLayout->addWidget(restrictionGroup);

    auto *encounterGroup = new QGroupBox(tr("Encounters && Factions"), this);
    auto *encounterLayout = new QVBoxLayout(encounterGroup);
    auto *helpLabel = new QLabel(tr("Encounter-Zeile: Name, Level, Chance. Faction-Zeilen darunter: Faction, Gewicht."), encounterGroup);
    helpLabel->setWordWrap(true);
    encounterLayout->addWidget(helpLabel);

    m_encounterTree = new QTreeWidget(encounterGroup);
    m_encounterTree->setHeaderLabels({tr("Name"), tr("Level / Gewicht"), tr("Chance")});
    m_encounterTree->setColumnWidth(0, 310);
    m_encounterTree->setColumnWidth(1, 130);
    m_encounterTree->setAlternatingRowColors(true);
    for (const EncounterRow &encounter : encounters) {
        auto *encounterItem = new QTreeWidgetItem(m_encounterTree, {encounter.name, encounter.level, encounter.chance});
        encounterItem->setFlags(encounterItem->flags() | Qt::ItemIsEditable);
        for (const auto &faction : encounter.factions) {
            auto *factionItem = new QTreeWidgetItem(encounterItem, {faction.first, faction.second, QString()});
            factionItem->setFlags(factionItem->flags() | Qt::ItemIsEditable);
        }
        encounterItem->setExpanded(true);
    }
    if (m_encounterTree->topLevelItemCount() == 0 && !m_allEncounters.isEmpty()) {
        auto *encounterItem = new QTreeWidgetItem(m_encounterTree,
                                                  {m_allEncounters.first(),
                                                   m_defaults.value(QStringLiteral("encounter_level")),
                                                   m_defaults.value(QStringLiteral("encounter_chance"))});
        encounterItem->setFlags(encounterItem->flags() | Qt::ItemIsEditable);
        if (!m_factions.isEmpty()) {
            auto *factionItem = new QTreeWidgetItem(encounterItem,
                                                    {factionNicknameFromDisplay(m_factions.first()),
                                                     m_defaults.value(QStringLiteral("faction_weight")),
                                                     QString()});
            factionItem->setFlags(factionItem->flags() | Qt::ItemIsEditable);
        }
        encounterItem->setExpanded(true);
    }
    encounterLayout->addWidget(m_encounterTree, 1);

    auto *encounterButtons = new QHBoxLayout();
    auto *addEncounterButton = new QPushButton(tr("+ Encounter"), encounterGroup);
    auto *addFactionButton = new QPushButton(tr("+ Faction"), encounterGroup);
    auto *removeEncounterButton = new QPushButton(tr("Entfernen"), encounterGroup);
    connect(addEncounterButton, &QPushButton::clicked, this, &ZonePopulationDialog::addEncounter);
    connect(addFactionButton, &QPushButton::clicked, this, &ZonePopulationDialog::addFaction);
    connect(removeEncounterButton, &QPushButton::clicked, this, &ZonePopulationDialog::removeEncounterItem);
    encounterButtons->addWidget(addEncounterButton);
    encounterButtons->addWidget(addFactionButton);
    encounterButtons->addWidget(removeEncounterButton);
    encounterButtons->addStretch(1);
    encounterLayout->addLayout(encounterButtons);
    mainLayout->addWidget(encounterGroup, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ZonePopulationDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ZonePopulationDialog::reject);
    mainLayout->addWidget(buttons);
}

QVector<QPair<QString, QString>> ZonePopulationDialog::entries() const
{
    QVector<QPair<QString, QString>> result = m_otherEntries;
    result.append({QStringLiteral("toughness"), QString::number(m_toughnessSpin->value())});
    result.append({QStringLiteral("density"), QString::number(m_densitySpin->value())});
    result.append({QStringLiteral("repop_time"), QString::number(m_repopSpin->value())});
    result.append({QStringLiteral("max_battle_size"), QString::number(m_battleSpin->value())});
    result.append({QStringLiteral("pop_type"), m_popTypeCombo->currentText().trimmed()});
    result.append({QStringLiteral("relief_time"), QString::number(m_reliefSpin->value())});

    for (const QString &restriction : densityRestrictions())
        result.append({QStringLiteral("density_restriction"), restriction});

    for (const EncounterRow &encounter : encounterRows()) {
        if (encounter.name.trimmed().isEmpty())
            continue;
        result.append({QStringLiteral("encounter"),
                       QStringLiteral("%1, %2, %3")
                           .arg(encounter.name.trimmed(),
                                encounter.level.trimmed(),
                                formatFloat(encounter.chance, m_defaults.value(QStringLiteral("encounter_chance"))))});
        for (const auto &faction : encounter.factions) {
            const QString factionName = factionNicknameFromDisplay(faction.first);
            if (factionName.isEmpty())
                continue;
            result.append({QStringLiteral("faction"),
                           QStringLiteral("%1, %2")
                               .arg(factionName,
                                    formatFloat(faction.second, m_defaults.value(QStringLiteral("faction_weight"))))});
        }
    }
    return result;
}

QString ZonePopulationDialog::inferProfile() const
{
    QHash<QString, QString> values;
    QStringList sourceBits;
    for (const auto &entry : m_otherEntries) {
        const QString key = entry.first.trimmed().toLower();
        const QString value = entry.second.trimmed().toLower();
        if (!values.contains(key))
            values.insert(key, value);
        sourceBits.append(QStringLiteral("%1=%2").arg(key, value));
    }
    const QString source = sourceBits.join(QLatin1Char(' '));
    const QString popType = m_initialPopType;
    const QString usage = values.value(QStringLiteral("usage"));
    const QString type = values.value(QStringLiteral("zone_type"));
    const QString comment = values.value(QStringLiteral("comment"));
    const QString nickname = values.value(QStringLiteral("nickname"));
    if (popType.contains(QStringLiteral("trade")) || popType.contains(QStringLiteral("lane"))
        || usage == QLatin1String("trade")
        || source.contains(QStringLiteral("trade_lane")) || source.contains(QStringLiteral("tradelane")))
        return QStringLiteral("lane");
    if (popType.contains(QStringLiteral("field")) || popType.contains(QStringLiteral("lootable")) || popType.contains(QStringLiteral("mining")))
        return QStringLiteral("field");
    if (type == QLatin1String("asteroids") || type == QLatin1String("nebula"))
        return QStringLiteral("field");
    if (comment.contains(QStringLiteral("asteroid")) || comment.contains(QStringLiteral("nebula")) || comment.contains(QStringLiteral("field"))
        || nickname.contains(QStringLiteral("asteroid")) || nickname.contains(QStringLiteral("nebula")) || nickname.contains(QStringLiteral("field")))
        return QStringLiteral("field");
    if (!popType.isEmpty() || usage == QLatin1String("patrol"))
        return QStringLiteral("patrol");
    return QStringLiteral("generic");
}

QStringList ZonePopulationDialog::popTypesForProfile(const QString &profile) const
{
    const QHash<QString, QStringList> byProfile{
        {QStringLiteral("field"), {QStringLiteral("field"), QStringLiteral("lootable_field"), QStringLiteral("mining_field")}},
        {QStringLiteral("patrol"), {QStringLiteral("lane_patrol"), QStringLiteral("attack_patrol"), QStringLiteral("field_patrol"), QStringLiteral("scavenger_path")}},
        {QStringLiteral("lane"), {QStringLiteral("trade_lane"), QStringLiteral("trade_path"), QStringLiteral("lane_patrol"), QStringLiteral("mining_path")}},
        {QStringLiteral("generic"), {QStringLiteral("field"), QStringLiteral("lootable_field"), QStringLiteral("mining_field"),
                                      QStringLiteral("lane_patrol"), QStringLiteral("attack_patrol"), QStringLiteral("field_patrol"),
                                      QStringLiteral("scavenger_path"), QStringLiteral("trade_lane"), QStringLiteral("trade_path"),
                                      QStringLiteral("mining_path")}},
    };
    QStringList values = byProfile.value(profile, byProfile.value(QStringLiteral("generic")));
    for (const QString &generic : byProfile.value(QStringLiteral("generic"))) {
        if (!values.contains(generic, Qt::CaseInsensitive))
            values.append(generic);
    }
    return values;
}

QHash<QString, QString> ZonePopulationDialog::defaultsForProfile(const QString &profile) const
{
    QHash<QString, QString> defaults{
        {QStringLiteral("toughness"), QStringLiteral("12")},
        {QStringLiteral("density"), QStringLiteral("6")},
        {QStringLiteral("repop_time"), QStringLiteral("45")},
        {QStringLiteral("max_battle_size"), QStringLiteral("6")},
        {QStringLiteral("pop_type"), QStringLiteral("attack_patrol")},
        {QStringLiteral("relief_time"), QStringLiteral("30")},
        {QStringLiteral("encounter_level"), QStringLiteral("12")},
        {QStringLiteral("encounter_chance"), QStringLiteral("0.100000")},
        {QStringLiteral("faction_weight"), QStringLiteral("1.000000")},
    };
    if (profile == QLatin1String("field")) {
        defaults.insert(QStringLiteral("toughness"), QStringLiteral("10"));
        defaults.insert(QStringLiteral("density"), QStringLiteral("5"));
        defaults.insert(QStringLiteral("repop_time"), QStringLiteral("25"));
        defaults.insert(QStringLiteral("max_battle_size"), QStringLiteral("4"));
        defaults.insert(QStringLiteral("pop_type"), QStringLiteral("field"));
        defaults.insert(QStringLiteral("relief_time"), QStringLiteral("25"));
        defaults.insert(QStringLiteral("encounter_level"), QStringLiteral("10"));
    } else if (profile == QLatin1String("lane")) {
        defaults.insert(QStringLiteral("pop_type"), QStringLiteral("trade_lane"));
        defaults.insert(QStringLiteral("max_battle_size"), QStringLiteral("4"));
    }
    return defaults;
}

QString ZonePopulationDialog::profileSummaryText(const QString &profile) const
{
    if (profile == QLatin1String("field"))
        return tr("Field Zone erkannt. Empfohlen: field, lootable_field oder mining_field.");
    if (profile == QLatin1String("lane"))
        return tr("Traffic/Trade Zone erkannt. Empfohlen: trade_lane oder trade_path.");
    return tr("No clear zone type detected. Custom setups remain allowed.");
}

QVector<ZonePopulationDialog::EncounterRow> ZonePopulationDialog::encounterRows() const
{
    QVector<EncounterRow> rows;
    for (int index = 0; index < m_encounterTree->topLevelItemCount(); ++index) {
        QTreeWidgetItem *item = m_encounterTree->topLevelItem(index);
        EncounterRow row;
        row.name = item->text(0).trimmed();
        row.level = item->text(1).trimmed();
        row.chance = item->text(2).trimmed();
        for (int child = 0; child < item->childCount(); ++child) {
            QTreeWidgetItem *factionItem = item->child(child);
            row.factions.append({factionItem->text(0).trimmed(), factionItem->text(1).trimmed()});
        }
        rows.append(row);
    }
    return rows;
}

QStringList ZonePopulationDialog::densityRestrictions() const
{
    QStringList values;
    for (int index = 0; index < m_densityList->count(); ++index) {
        const QString value = m_densityList->item(index)->text().trimmed();
        if (!value.isEmpty())
            values.append(value);
    }
    return values;
}

QStringList ZonePopulationDialog::validationErrors(QStringList *warnings) const
{
    QStringList errors;
    QSet<QString> encounterNames;
    double totalEncounterChance = 0.0;
    const QVector<EncounterRow> rows = encounterRows();
    for (int index = 0; index < rows.size(); ++index) {
        const EncounterRow &row = rows.at(index);
        const QString label = row.name.isEmpty() ? QStringLiteral("#%1").arg(index + 1) : row.name;
        if (row.name.isEmpty()) {
            errors.append(tr("Encounter %1 hat keinen Namen.").arg(index + 1));
        } else if (encounterNames.contains(row.name.toLower())) {
            if (warnings)
                warnings->append(tr("Encounter '%1' ist mehrfach eingetragen.").arg(row.name));
        } else {
            encounterNames.insert(row.name.toLower());
        }
        const int level = toInt(row.level, -1);
        if (level <= 0)
            errors.append(tr("Encounter '%1' braucht ein Level > 0.").arg(label));
        if (level > 19 && warnings)
            warnings->append(tr("Encounter '%1' hat Level %2. Vanilla liegt meist bei 1 bis 19.").arg(label).arg(level));

        const double chance = toDouble(row.chance, -1.0);
        if (chance < 0.0 || chance > 1.0)
            errors.append(tr("Encounter '%1' braucht eine Chance zwischen 0.0 und 1.0.").arg(label));
        else
            totalEncounterChance += chance;

        if (row.factions.isEmpty())
            errors.append(tr("Encounter '%1' hat keine Faction-Zuordnung.").arg(label));
        double factionWeight = 0.0;
        for (const auto &faction : row.factions) {
            const QString factionName = factionNicknameFromDisplay(faction.first);
            const double weight = toDouble(faction.second, -1.0);
            if (factionName.isEmpty())
                errors.append(tr("Encounter '%1' hat eine leere Faction.").arg(label));
            if (weight <= 0.0)
                errors.append(tr("Faction '%1' in Encounter '%2' braucht ein Gewicht > 0.").arg(factionName, label));
            factionWeight += std::max(0.0, weight);
        }
        if (factionWeight > 1.000001)
            errors.append(tr("Die Summe der Faction-Gewichte in Encounter '%1' darf 1.0 nicht übersteigen.").arg(label));
    }

    for (const QString &restriction : densityRestrictions()) {
        const QStringList parts = restriction.split(QLatin1Char(','), Qt::KeepEmptyParts);
        if (parts.size() < 2 || parts.value(0).trimmed().isEmpty() || parts.value(1).trimmed().isEmpty()) {
            errors.append(tr("Density Restriction '%1' ist ungültig. Erwartet wird 'Anzahl, Encounter'.").arg(restriction));
            continue;
        }
        if (toInt(parts.value(0), -1) < 0)
            errors.append(tr("Density Restriction '%1' braucht vorne eine Zahl.").arg(restriction));
        if (!encounterNames.contains(parts.value(1).trimmed().toLower()))
            errors.append(tr("Density Restriction '%1' verweist auf einen unbekannten Encounter.").arg(restriction));
    }
    if (totalEncounterChance > 1.000001)
        errors.append(tr("Die Summe aller Encounter-Chancen darf 1.0 nicht übersteigen."));
    if (!rows.isEmpty() && m_densitySpin->value() <= 0 && warnings)
        warnings->append(tr("Die Zone hat Encounters, aber Density ist 0."));
    if (m_densitySpin->value() > 0 && m_battleSpin->value() > m_densitySpin->value() && warnings)
        warnings->append(tr("Max Battle Size ist größer als Density."));
    if (m_repopSpin->value() > 0 && m_reliefSpin->value() > m_repopSpin->value() && warnings)
        warnings->append(tr("Relief Time ist größer als Repop Time."));
    return errors;
}

void ZonePopulationDialog::addDensityRestriction()
{
    auto *item = new QListWidgetItem(QStringLiteral("1, encounter_name"), m_densityList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_densityList->setCurrentItem(item);
    m_densityList->editItem(item);
}

void ZonePopulationDialog::removeDensityRestriction()
{
    delete m_densityList->takeItem(m_densityList->currentRow());
}

void ZonePopulationDialog::addEncounter()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Encounter auswählen"));
    dialog.setMinimumWidth(420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *combo = new QComboBox(&dialog);
    combo->setEditable(true);
    for (const QString &encounter : m_allEncounters) {
        const bool exists = containsCaseInsensitive(m_encounterParameters, encounter);
        combo->addItem(exists ? QStringLiteral("✓ %1").arg(encounter)
                              : QStringLiteral("◇ %1 (neu)").arg(encounter),
                       encounter);
    }
    configureContainsCompleter(combo);
    layout->addWidget(combo);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name = combo->currentData().toString().trimmed().isEmpty()
        ? combo->currentText().trimmed()
        : combo->currentData().toString().trimmed();
    if (name.isEmpty())
        return;
    if (!containsCaseInsensitive(m_encounterParameters, name))
        m_newEncounterParameters.insert(name);

    auto *item = new QTreeWidgetItem(m_encounterTree,
                                     {name,
                                      m_defaults.value(QStringLiteral("encounter_level")),
                                      m_defaults.value(QStringLiteral("encounter_chance"))});
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setExpanded(true);
    m_encounterTree->setCurrentItem(item);
}

void ZonePopulationDialog::addFaction()
{
    QTreeWidgetItem *current = m_encounterTree->currentItem();
    if (!current)
        return;
    if (current->parent())
        current = current->parent();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Faction auswählen"));
    dialog.setMinimumWidth(420);
    auto *layout = new QVBoxLayout(&dialog);
    auto *combo = new QComboBox(&dialog);
    combo->setEditable(true);
    combo->addItems(m_factions);
    configureContainsCompleter(combo);
    layout->addWidget(combo);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name = factionNicknameFromDisplay(combo->currentText());
    if (name.isEmpty())
        return;
    auto *item = new QTreeWidgetItem(current, {name, m_defaults.value(QStringLiteral("faction_weight")), QString()});
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    current->setExpanded(true);
    m_encounterTree->setCurrentItem(item);
}

void ZonePopulationDialog::removeEncounterItem()
{
    QTreeWidgetItem *current = m_encounterTree->currentItem();
    if (!current)
        return;
    if (QTreeWidgetItem *parent = current->parent()) {
        parent->removeChild(current);
        delete current;
    } else {
        delete m_encounterTree->takeTopLevelItem(m_encounterTree->indexOfTopLevelItem(current));
    }
}

void ZonePopulationDialog::accept()
{
    QStringList warnings;
    const QStringList errors = validationErrors(&warnings);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Zone Population"),
                             tr("Bitte korrigiere zuerst diese Punkte:\n\n- %1").arg(errors.join(QStringLiteral("\n- "))));
        return;
    }
    if (!warnings.isEmpty()) {
        const auto answer = QMessageBox::question(
            this,
            tr("Zone Population Warnung"),
            tr("Diese Kombination ist speicherbar, aber auffällig:\n\n- %1\n\nSoll trotzdem fortgefahren werden?")
                .arg(warnings.join(QStringLiteral("\n- "))),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    QDialog::accept();
}

QString ZonePopulationDialog::factionNicknameFromDisplay(const QString &raw)
{
    const QString value = raw.trimmed();
    const int sep = value.indexOf(QStringLiteral(" - "));
    return sep > 0 ? value.left(sep).trimmed() : value;
}

int ZonePopulationDialog::toInt(const QString &value, int fallback)
{
    bool ok = false;
    const int parsed = value.trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

double ZonePopulationDialog::toDouble(const QString &value, double fallback)
{
    bool ok = false;
    const double parsed = value.trimmed().toDouble(&ok);
    return ok ? parsed : fallback;
}

QString ZonePopulationDialog::formatFloat(const QString &value, const QString &fallback)
{
    bool ok = false;
    const double parsed = value.trimmed().toDouble(&ok);
    return ok ? QString::number(parsed, 'f', 6) : fallback;
}

} // namespace flatlas::editors
