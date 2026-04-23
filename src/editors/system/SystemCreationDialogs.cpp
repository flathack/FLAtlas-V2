#include "SystemCreationDialogs.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"
#include "infrastructure/parser/XmlInfocard.h"
#include "rendering/view3d/ModelViewport3D.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStringListModel>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

namespace {

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

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

QString primaryGamePath()
{
    return flatlas::core::EditingContext::instance().primaryGamePath();
}

QString dataDirFor(const QString &gamePath)
{
    if (gamePath.isEmpty())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
}

QString exeDirFor(const QString &gamePath)
{
    if (gamePath.isEmpty())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
}

QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}

QString equipmentNicknameFromLoadoutValue(const QString &rawValue)
{
    return rawValue.split(QLatin1Char(','), Qt::KeepEmptyParts).value(0).trimmed();
}

struct SharedCreationCatalog {
    QHash<QString, QString> archetypeModelPaths;
    QHash<QString, QStringList> loadoutContents;
    QStringList pilotNicknames;
};

const SharedCreationCatalog &sharedCreationCatalog()
{
    static QString cachedGameKey;
    static SharedCreationCatalog catalog;

    const QString gamePath = primaryGamePath();
    const QString gameKey = normalizedKey(gamePath);
    if (gameKey == cachedGameKey)
        return catalog;

    cachedGameKey = gameKey;
    catalog = {};

    const QString dataDir = dataDirFor(gamePath);
    const QString exeDir = exeDirFor(gamePath);
    if (dataDir.isEmpty())
        return catalog;

    IdsStringTable ids;
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    QHash<QString, QString> resolvedNames;
    const QStringList nameScanRoots = {
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("EQUIPMENT")),
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SHIPS")),
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR")),
    };

    for (const QString &root : nameScanRoots) {
        if (root.isEmpty())
            continue;
        QDirIterator it(root, QStringList{QStringLiteral("*.ini")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString filePath = it.next();
            const IniDocument doc = IniParser::parseFile(filePath);
            for (const IniSection &section : doc) {
                const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
                if (nickname.isEmpty())
                    continue;
                const QString key = normalizedKey(nickname);
                if (resolvedNames.contains(key))
                    continue;

                bool ok = false;
                const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
                if (!ok || idsName <= 0)
                    continue;
                const QString ingameName = ids.getString(idsName).trimmed();
                if (!ingameName.isEmpty())
                    resolvedNames.insert(key, ingameName);
            }
        }
    }

    const QString solarArchPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (!solarArchPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(solarArchPath);
        for (const IniSection &section : doc) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString relativeModelPath = section.value(QStringLiteral("DA_archetype")).trimmed();
            if (nickname.isEmpty() || relativeModelPath.isEmpty())
                continue;
            const QString absoluteModelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModelPath);
            if (!absoluteModelPath.isEmpty())
                catalog.archetypeModelPaths.insert(normalizedKey(nickname), absoluteModelPath);
        }
    }

    const QStringList loadoutSources = {
        QStringLiteral("SHIPS/loadouts.ini"),
        QStringLiteral("SHIPS/loadouts_special.ini"),
        QStringLiteral("SOLAR/loadouts.ini"),
    };
    for (const QString &relativePath : loadoutSources) {
        const QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        if (absolutePath.isEmpty())
            continue;

        const IniDocument doc = IniParser::parseFile(absolutePath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            QStringList displayEntries;
            for (const auto &entry : section.entries) {
                const QString entryKey = entry.first.trimmed();
                if (entryKey.compare(QStringLiteral("equip"), Qt::CaseInsensitive) != 0
                    && entryKey.compare(QStringLiteral("cargo"), Qt::CaseInsensitive) != 0
                    && entryKey.compare(QStringLiteral("addon"), Qt::CaseInsensitive) != 0) {
                    continue;
                }

                const QString itemNickname = equipmentNicknameFromLoadoutValue(entry.second);
                if (itemNickname.isEmpty())
                    continue;

                const QString ingameName = resolvedNames.value(normalizedKey(itemNickname));
                displayEntries.append(QStringLiteral("%1 - %2")
                                          .arg(itemNickname,
                                               ingameName.isEmpty()
                                                   ? QObject::tr("(kein Ingame-Name)")
                                                   : ingameName));
            }
            catalog.loadoutContents.insert(normalizedKey(nickname), displayEntries);
        }
    }

    QStringList pilots;
    const QString missionsDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS"));
    const QStringList pilotSources = {
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_population.ini")),
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_story.ini")),
    };
    for (const QString &pilotSource : pilotSources) {
        if (pilotSource.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(pilotSource);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Pilot"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (!nickname.isEmpty())
                pilots.append(nickname);
        }
    }
    pilots.removeDuplicates();
    std::sort(pilots.begin(), pilots.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    catalog.pilotNicknames = pilots;

    return catalog;
}

QWidget *createPreviewFrame(flatlas::rendering::ModelViewport3D **outPreview,
                            QLabel **outFallback,
                            QStackedLayout **outStack,
                            QWidget *parent)
{
    auto *previewFrame = new QFrame(parent);
    previewFrame->setFrameShape(QFrame::StyledPanel);
    previewFrame->setMinimumWidth(340);

    auto *previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(8, 8, 8, 8);
    previewLayout->setSpacing(6);

    auto *previewTitle = new QLabel(QObject::tr("3D-Vorschau"), previewFrame);
    previewTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    previewLayout->addWidget(previewTitle);

    auto *previewHost = new QWidget(previewFrame);
    auto *stack = new QStackedLayout(previewHost);
    stack->setContentsMargins(0, 0, 0, 0);

    auto *preview = new flatlas::rendering::ModelViewport3D(previewHost);
    stack->addWidget(preview);

    auto *fallback = new QLabel(QObject::tr("Waehle einen Archetype um die Vorschau zu laden."), previewHost);
    fallback->setAlignment(Qt::AlignCenter);
    fallback->setWordWrap(true);
    fallback->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    stack->addWidget(fallback);
    stack->setCurrentWidget(fallback);

    previewLayout->addWidget(previewHost, 1);

    if (outPreview)
        *outPreview = preview;
    if (outFallback)
        *outFallback = fallback;
    if (outStack)
        *outStack = stack;
    return previewFrame;
}

void refreshArchetypePreview(QComboBox *archetypeCombo,
                             const QHash<QString, QString> &archetypeModelPaths,
                             flatlas::rendering::ModelViewport3D *preview,
                             QLabel *fallback,
                             QStackedLayout *stack)
{
    if (!archetypeCombo || !preview || !fallback || !stack)
        return;

    const QString archetype = archetypeCombo->currentText().trimmed();
    auto showFallback = [&](const QString &message) {
        fallback->setText(message);
        stack->setCurrentWidget(fallback);
        preview->clearModel();
    };

    if (archetype.isEmpty()) {
        showFallback(QObject::tr("Waehle einen Archetype um die Vorschau zu laden."));
        return;
    }

    const QString modelPath = archetypeModelPaths.value(normalizedKey(archetype));
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        showFallback(QObject::tr("Kein 3D-Modell fuer %1 vorhanden.").arg(archetype));
        return;
    }

    QString errorMessage;
    if (!preview->loadModelFile(modelPath, &errorMessage)) {
        showFallback(QObject::tr("Modell konnte nicht geladen werden: %1")
                         .arg(errorMessage.isEmpty() ? QObject::tr("unbekannter Fehler") : errorMessage));
        return;
    }

    stack->setCurrentWidget(preview);
}

void updateLoadoutContentsUi(QComboBox *loadoutCombo,
                             const QHash<QString, QStringList> &loadoutContents,
                             QListWidget *listWidget,
                             QLabel *statusLabel)
{
    if (!loadoutCombo || !listWidget || !statusLabel)
        return;

    listWidget->clear();
    const QString loadout = loadoutCombo->currentText().trimmed();
    if (loadout.isEmpty()) {
        statusLabel->setText(QObject::tr("Kein Loadout ausgewaehlt."));
        return;
    }

    const QStringList entries = loadoutContents.value(normalizedKey(loadout));
    if (entries.isEmpty()) {
        statusLabel->setText(QObject::tr("Keine aufloesbaren Loadout-Inhalte gefunden."));
        return;
    }

    listWidget->addItems(entries);
    statusLabel->setText(QObject::tr("%1 Eintraege im Loadout.").arg(entries.size()));
}

QStringList fixedVisitOptions()
{
    return {
        QStringLiteral("32 - Standard nebula (vanilla-typisch)"),
        QStringLiteral("36 - Vanilla-Variante (zusaetzliches Flag, genaue Bedeutung unklar)"),
        QStringLiteral("0 - Keine speziellen Visit-Flags"),
        QStringLiteral("128 - Versteckt / nicht auf Karte zeigen"),
    };
}

void populateFixedVisitCombo(QComboBox *combo)
{
    if (!combo)
        return;
    combo->clear();
    const QStringList options = fixedVisitOptions();
    for (const QString &option : options)
        combo->addItem(option);
    configureContainsCompleter(combo);
    const int fixedIndex = combo->findText(QStringLiteral("0 - Keine speziellen Visit-Flags"), Qt::MatchFixedString);
    combo->setCurrentIndex(fixedIndex >= 0 ? fixedIndex : 0);
    combo->setEnabled(false);
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
    setMinimumSize(1020, 660);

    const SharedCreationCatalog &catalog = sharedCreationCatalog();
    m_archetypeModelPaths = catalog.archetypeModelPaths;
    m_loadoutContents = catalog.loadoutContents;

    auto *root = new QHBoxLayout(this);

    auto *formHost = new QWidget(this);
    auto *formLayout = new QFormLayout(formHost);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(suggestedNickname, formHost);
    formLayout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(formHost);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    formLayout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_infoCardEdit = new QTextEdit(formHost);
    m_infoCardEdit->setMinimumHeight(120);
    m_infoCardEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_infoCardEdit->setPlaceholderText(tr("Info text / Infocard text (optional)"));
    formLayout->addRow(tr("ids_info Text:"), m_infoCardEdit);

    m_archetypeCombo = createEditableCombo(archetypes, formHost);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    formLayout->addRow(tr("Archetype:"), m_archetypeCombo);

    QStringList loadoutValues = loadouts;
    loadoutValues.prepend(QString());
    loadoutValues.removeDuplicates();
    m_loadoutCombo = createEditableCombo(loadoutValues, formHost);
    m_loadoutCombo->setCurrentIndex(0);
    formLayout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_loadoutContentsStatus = new QLabel(tr("Kein Loadout ausgewaehlt."), formHost);
    m_loadoutContentsStatus->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_loadoutContentsStatus->setWordWrap(true);
    formLayout->addRow(QString(), m_loadoutContentsStatus);

    m_loadoutContentsList = new QListWidget(formHost);
    m_loadoutContentsList->setMinimumHeight(170);
    formLayout->addRow(tr("Loadout-Inhalt:"), m_loadoutContentsList);

    m_visitCombo = new QComboBox(formHost);
    m_visitCombo->setEditable(true);
    m_visitCombo->setInsertPolicy(QComboBox::NoInsert);
    populateFixedVisitCombo(m_visitCombo);
    formLayout->addRow(tr("Visit:"), m_visitCombo);

    m_visitHintLabel = new QLabel(tr("Dieser Create-Flow schreibt immer visit = 0."), formHost);
    m_visitHintLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_visitHintLabel->setWordWrap(true);
    formLayout->addRow(QString(), m_visitHintLabel);

    m_commentEdit = new QLineEdit(formHost);
    m_commentEdit->setPlaceholderText(tr("optional"));
    formLayout->addRow(tr("Kommentar:"), m_commentEdit);

    auto *buttons = createDialogButtons(this);

    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(formHost);
    leftLayout->addStretch(1);
    leftLayout->addWidget(buttons);

    root->addLayout(leftLayout, 3);
    root->addWidget(createPreviewFrame(&m_preview, &m_previewFallback, &m_previewStack, this), 4);

    m_previewRefreshTimer = new QTimer(this);
    m_previewRefreshTimer->setSingleShot(true);
    m_previewRefreshTimer->setInterval(180);
    connect(m_previewRefreshTimer, &QTimer::timeout, this, &CreateSurpriseDialog::refreshPreview);

    connect(m_archetypeCombo, &QComboBox::currentTextChanged, this, &CreateSurpriseDialog::schedulePreviewRefresh);
    connect(m_loadoutCombo, &QComboBox::currentTextChanged, this, &CreateSurpriseDialog::updateLoadoutContents);

    updateLoadoutContents();
    refreshPreview();
}

CreateSurpriseRequest CreateSurpriseDialog::result() const
{
    CreateSurpriseRequest value;
    value.nickname = m_nicknameEdit->text().trimmed();
    value.ingameName = m_ingameNameEdit->text().trimmed();
    value.infoCardText = m_infoCardEdit->toPlainText().trimmed();
    value.archetype = m_archetypeCombo->currentText().trimmed();
    value.loadout = m_loadoutCombo->currentText().trimmed();
    value.comment = m_commentEdit->text().trimmed();
    value.visit = 0;
    return value;
}

void CreateSurpriseDialog::schedulePreviewRefresh()
{
    if (!m_previewRefreshTimer) {
        refreshPreview();
        return;
    }
    ++m_previewRefreshGeneration;
    m_previewRefreshTimer->start();
}

void CreateSurpriseDialog::refreshPreview()
{
    refreshArchetypePreview(m_archetypeCombo, m_archetypeModelPaths, m_preview, m_previewFallback, m_previewStack);
}

void CreateSurpriseDialog::updateLoadoutContents()
{
    updateLoadoutContentsUi(m_loadoutCombo, m_loadoutContents, m_loadoutContentsList, m_loadoutContentsStatus);
}

CreateWeaponPlatformDialog::CreateWeaponPlatformDialog(const QString &suggestedNickname,
                                                       const QStringList &archetypes,
                                                       const QStringList &loadouts,
                                                       const QStringList &factions,
                                                       QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Waffenplattform erstellen"));
    setMinimumSize(1060, 720);

    const SharedCreationCatalog &catalog = sharedCreationCatalog();
    m_archetypeModelPaths = catalog.archetypeModelPaths;
    m_loadoutContents = catalog.loadoutContents;

    auto *root = new QHBoxLayout(this);

    auto *formHost = new QWidget(this);
    auto *formLayout = new QFormLayout(formHost);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(suggestedNickname, formHost);
    formLayout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(formHost);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    formLayout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_archetypeCombo = createEditableCombo(archetypes, formHost);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    formLayout->addRow(tr("Archetype:"), m_archetypeCombo);

    QStringList loadoutValues = loadouts;
    if (!loadoutValues.contains(QString(), Qt::CaseSensitive))
        loadoutValues.prepend(QString());
    loadoutValues.removeDuplicates();
    m_loadoutCombo = createEditableCombo(loadoutValues, formHost);
    formLayout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_loadoutContentsStatus = new QLabel(tr("Kein Loadout ausgewaehlt."), formHost);
    m_loadoutContentsStatus->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_loadoutContentsStatus->setWordWrap(true);
    formLayout->addRow(QString(), m_loadoutContentsStatus);

    m_loadoutContentsList = new QListWidget(formHost);
    m_loadoutContentsList->setMinimumHeight(190);
    formLayout->addRow(tr("Loadout-Inhalt:"), m_loadoutContentsList);

    m_factionCombo = createEditableCombo(factions, formHost);
    formLayout->addRow(tr("Reputation:"), m_factionCombo);

    m_behaviorEdit = new QLineEdit(QStringLiteral("NOTHING"), formHost);
    formLayout->addRow(tr("Behavior:"), m_behaviorEdit);

    m_pilotCombo = new QComboBox(formHost);
    m_pilotCombo->setInsertPolicy(QComboBox::NoInsert);
    m_pilotCombo->addItems(catalog.pilotNicknames);
    const int defaultPilotIndex = m_pilotCombo->findText(QStringLiteral("pilot_solar_hard"), Qt::MatchFixedString);
    m_pilotCombo->setCurrentIndex(defaultPilotIndex >= 0 ? defaultPilotIndex : 0);
    formLayout->addRow(tr("Pilot:"), m_pilotCombo);

    m_difficultySpin = new QSpinBox(formHost);
    m_difficultySpin->setRange(0, 100);
    m_difficultySpin->setValue(6);
    formLayout->addRow(tr("difficulty_level:"), m_difficultySpin);

    m_visitCombo = new QComboBox(formHost);
    m_visitCombo->setEditable(true);
    m_visitCombo->setInsertPolicy(QComboBox::NoInsert);
    populateFixedVisitCombo(m_visitCombo);
    formLayout->addRow(tr("Visit:"), m_visitCombo);

    m_visitHintLabel = new QLabel(tr("Die Anzeige erklaert Visit-Flags, gespeichert wird hier immer visit = 0."), formHost);
    m_visitHintLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_visitHintLabel->setWordWrap(true);
    formLayout->addRow(QString(), m_visitHintLabel);

    auto *buttons = createDialogButtons(this);

    auto *leftLayout = new QVBoxLayout();
    leftLayout->addWidget(formHost);
    leftLayout->addStretch(1);
    leftLayout->addWidget(buttons);

    root->addLayout(leftLayout, 3);
    root->addWidget(createPreviewFrame(&m_preview, &m_previewFallback, &m_previewStack, this), 4);

    m_previewRefreshTimer = new QTimer(this);
    m_previewRefreshTimer->setSingleShot(true);
    m_previewRefreshTimer->setInterval(180);
    connect(m_previewRefreshTimer, &QTimer::timeout, this, &CreateWeaponPlatformDialog::refreshPreview);

    connect(m_archetypeCombo, &QComboBox::currentTextChanged, this, &CreateWeaponPlatformDialog::schedulePreviewRefresh);
    connect(m_loadoutCombo, &QComboBox::currentTextChanged, this, &CreateWeaponPlatformDialog::updateLoadoutContents);

    updateLoadoutContents();
    refreshPreview();
}

void CreateWeaponPlatformDialog::schedulePreviewRefresh()
{
    if (!m_previewRefreshTimer) {
        refreshPreview();
        return;
    }
    ++m_previewRefreshGeneration;
    m_previewRefreshTimer->start();
}

void CreateWeaponPlatformDialog::refreshPreview()
{
    refreshArchetypePreview(m_archetypeCombo, m_archetypeModelPaths, m_preview, m_previewFallback, m_previewStack);
}

void CreateWeaponPlatformDialog::updateLoadoutContents()
{
    updateLoadoutContentsUi(m_loadoutCombo, m_loadoutContents, m_loadoutContentsList, m_loadoutContentsStatus);
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
    value.pilot = m_pilotCombo ? m_pilotCombo->currentText().trimmed() : QString();
    value.difficultyLevel = m_difficultySpin->value();
    value.visit = 0;
    return value;
}

} // namespace flatlas::editors
