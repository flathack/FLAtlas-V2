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
#include <QMessageBox>
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

QDialogButtonBox *createDialogButtons(QDialog *dialog, const QString &acceptText = QObject::tr("Erstellen"))
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(acceptText);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    return buttons;
}

QDoubleSpinBox *createCoordinateSpinBox(double value, QWidget *parent)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(-10000000.0, 10000000.0);
    spin->setDecimals(0);
    spin->setSingleStep(1000.0);
    spin->setValue(value);
    return spin;
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

CreateBuoyDialog::CreateBuoyDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Bojen erstellen"));
    setMinimumWidth(440);

    auto *layout = new QFormLayout(this);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(QStringLiteral("nav_buoy"), QStringLiteral("nav_buoy"));
    m_typeCombo->addItem(QStringLiteral("hazard_buoy"), QStringLiteral("hazard_buoy"));
    layout->addRow(tr("Typ:"), m_typeCombo);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Linie"), QVariant::fromValue(static_cast<int>(CreateBuoyRequest::Mode::Line)));
    m_modeCombo->addItem(tr("Kreis"), QVariant::fromValue(static_cast<int>(CreateBuoyRequest::Mode::Circle)));
    layout->addRow(tr("Muster:"), m_modeCombo);

    m_lineConstraintCombo = new QComboBox(this);
    m_lineConstraintCombo->addItem(tr("Feste Anzahl"),
                                   QVariant::fromValue(static_cast<int>(CreateBuoyRequest::LineConstraint::FixedCount)));
    m_lineConstraintCombo->addItem(tr("Fester Abstand"),
                                   QVariant::fromValue(static_cast<int>(CreateBuoyRequest::LineConstraint::FixedSpacing)));
    layout->addRow(tr("Linienmodus:"), m_lineConstraintCombo);

    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(2, 128);
    m_countSpin->setValue(8);
    layout->addRow(tr("Anzahl:"), m_countSpin);
    m_countDerivedLabel = new QLabel(tr("Wird waehrend der Platzierung berechnet."), this);
    m_countDerivedLabel->setWordWrap(true);
    layout->addRow(QString(), m_countDerivedLabel);

    m_spacingLabel = new QLabel(tr("Abstand (m):"), this);
    m_spacingSpin = new QSpinBox(this);
    m_spacingSpin->setRange(100, 100000);
    m_spacingSpin->setSingleStep(100);
    m_spacingSpin->setValue(3000);
    layout->addRow(m_spacingLabel, m_spacingSpin);
    m_spacingDerivedLabel = new QLabel(tr("Wird waehrend der Platzierung berechnet."), this);
    m_spacingDerivedLabel->setWordWrap(true);
    layout->addRow(QString(), m_spacingDerivedLabel);

    m_modeHintLabel = new QLabel(this);
    m_modeHintLabel->setWordWrap(true);
    m_modeHintLabel->setFrameStyle(QFrame::NoFrame);
    layout->addRow(QString(), m_modeHintLabel);

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &CreateBuoyDialog::updateModeUi);
    connect(m_lineConstraintCombo, &QComboBox::currentIndexChanged, this, &CreateBuoyDialog::updateLineConstraintUi);
    updateModeUi();

    layout->addRow(createDialogButtons(this));
}

void CreateBuoyDialog::updateModeUi()
{
    const auto mode = static_cast<CreateBuoyRequest::Mode>(
        m_modeCombo->currentData().toInt());
    const bool lineMode = mode == CreateBuoyRequest::Mode::Line;
    m_lineConstraintCombo->setVisible(lineMode);
    m_countSpin->setMinimum(lineMode ? 2 : 3);
    if (m_countSpin->value() < m_countSpin->minimum())
        m_countSpin->setValue(m_countSpin->minimum());
    if (!lineMode) {
        m_countSpin->setEnabled(true);
        m_countDerivedLabel->setVisible(false);
        m_spacingLabel->setVisible(false);
        m_spacingSpin->setVisible(false);
        m_spacingDerivedLabel->setVisible(false);
        m_modeHintLabel->setText(tr("1. Klick setzt den Mittelpunkt. 2. Klick bestimmt den Radius. "
                                    "Die Bojen werden gleichmaessig auf dem Kreis verteilt."));
        return;
    }

    updateLineConstraintUi();
}

void CreateBuoyDialog::updateLineConstraintUi()
{
    const auto mode = static_cast<CreateBuoyRequest::Mode>(
        m_modeCombo->currentData().toInt());
    if (mode != CreateBuoyRequest::Mode::Line)
        return;

    const auto constraint = static_cast<CreateBuoyRequest::LineConstraint>(
        m_lineConstraintCombo->currentData().toInt());
    const bool fixedCount = constraint == CreateBuoyRequest::LineConstraint::FixedCount;

    m_countSpin->setEnabled(fixedCount);
    m_countDerivedLabel->setVisible(!fixedCount);
    m_spacingLabel->setVisible(true);
    m_spacingSpin->setVisible(!fixedCount);
    m_spacingSpin->setEnabled(!fixedCount);
    m_spacingDerivedLabel->setVisible(fixedCount);

    m_countDerivedLabel->setText(tr("Die Anzahl wird waehrend der Platzierung aus Linienlaenge und Abstand berechnet."));
    m_spacingDerivedLabel->setText(tr("Der Abstand wird waehrend der Platzierung aus Linienlaenge und Anzahl berechnet."));
    m_modeHintLabel->setText(fixedCount
                                 ? tr("Linienmodus: feste Anzahl. 1. Klick setzt den Startpunkt. 2. Klick bestimmt die Richtung. "
                                      "Der Abstand zwischen den Bojen wird aus der gezeichneten Linienlaenge berechnet.")
                                 : tr("Linienmodus: fester Abstand. 1. Klick setzt den Startpunkt. 2. Klick bestimmt die Richtung. "
                                      "Die Anzahl der Bojen wird aus Linienlaenge und Abstand berechnet."));
}

CreateBuoyRequest CreateBuoyDialog::result() const
{
    CreateBuoyRequest value;
    value.archetype = m_typeCombo->currentData().toString().trimmed();
    value.mode = static_cast<CreateBuoyRequest::Mode>(m_modeCombo->currentData().toInt());
    value.lineConstraint = static_cast<CreateBuoyRequest::LineConstraint>(m_lineConstraintCombo->currentData().toInt());
    value.count = m_countSpin->value();
    value.spacingMeters = m_spacingSpin->value();
    return value;
}

CreateTradeLaneDialog::CreateTradeLaneDialog(const QString &systemNickname,
                                             int startNumber,
                                             int ringCount,
                                             double distanceMeters,
                                             const QStringList &loadouts,
                                             const QStringList &factions,
                                             const QStringList &pilots,
                                             QWidget *parent)
    : QDialog(parent)
    , m_distanceMeters(std::max(distanceMeters, 0.0))
{
    setWindowTitle(tr("Trade Lane erstellen"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(2, 200);
    m_countSpin->setValue(std::max(ringCount, 2));
    form->addRow(tr("Ring-Anzahl:"), m_countSpin);

    m_spacingSpin = new QSpinBox(this);
    m_spacingSpin->setRange(500, 50000);
    m_spacingSpin->setSingleStep(500);
    m_spacingSpin->setSuffix(tr(" m"));
    m_spacingSpin->setValue(7500);
    form->addRow(tr("Abstand:"), m_spacingSpin);

    m_spacingInfoLabel = new QLabel(this);
    m_spacingInfoLabel->setWordWrap(true);
    m_spacingInfoLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    form->addRow(QString(), m_spacingInfoLabel);

    m_startNumberSpin = new QSpinBox(this);
    m_startNumberSpin->setRange(1, 99999);
    m_startNumberSpin->setValue(std::max(startNumber, 1));
    form->addRow(tr("Startnummer:"), m_startNumberSpin);

    QStringList loadoutValues = loadouts;
    loadoutValues.removeDuplicates();
    if (loadoutValues.isEmpty()) {
        loadoutValues = {
            QStringLiteral("trade_lane_ring_li_01"),
            QStringLiteral("trade_lane_ring_br_01"),
            QStringLiteral("trade_lane_ring_ku_01"),
        };
    }
    m_loadoutCombo = createEditableCombo(loadoutValues, this);
    if (m_loadoutCombo->count() > 0)
        m_loadoutCombo->setCurrentIndex(0);
    form->addRow(tr("Loadout:"), m_loadoutCombo);

    m_reputationCombo = createEditableCombo(factions, this);
    form->addRow(tr("Reputation:"), m_reputationCombo);

    m_difficultySpin = new QSpinBox(this);
    m_difficultySpin->setRange(1, 7);
    m_difficultySpin->setValue(1);
    form->addRow(tr("difficulty_level:"), m_difficultySpin);

    m_pilotCombo = createEditableCombo(pilots, this);
    const int easiestIndex =
        m_pilotCombo->findText(QStringLiteral("pilot_solar_easiest"), Qt::MatchFixedString);
    m_pilotCombo->setCurrentIndex(easiestIndex >= 0 ? easiestIndex : 0);
    form->addRow(tr("Pilot:"), m_pilotCombo);

    m_routeNameEdit = new QLineEdit(this);
    m_routeNameEdit->setPlaceholderText(tr("Anzeigename der Route (optional)"));
    form->addRow(tr("Route / ids_name:"), m_routeNameEdit);

    m_startSpaceNameEdit = new QLineEdit(this);
    m_startSpaceNameEdit->setPlaceholderText(tr("Name am ersten Ring (optional)"));
    form->addRow(tr("Start tradelane_space_name:"), m_startSpaceNameEdit);

    m_endSpaceNameEdit = new QLineEdit(this);
    m_endSpaceNameEdit->setPlaceholderText(tr("Name am letzten Ring (optional)"));
    form->addRow(tr("Ende tradelane_space_name:"), m_endSpaceNameEdit);

    layout->addLayout(form);

    auto *infoLabel = new QLabel(
        tr("System: %1\nNicknames: %1_Trade_Lane_Ring_N")
            .arg(systemNickname.trimmed().isEmpty() ? QStringLiteral("System") : systemNickname),
        this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(infoLabel);

    layout->addWidget(createDialogButtons(this));

    connect(m_countSpin, &QSpinBox::valueChanged, this, &CreateTradeLaneDialog::updateDerivedSpacing);
    connect(m_spacingSpin, &QSpinBox::valueChanged, this, &CreateTradeLaneDialog::updateCountFromSpacing);

    updateDerivedSpacing();
}

void CreateTradeLaneDialog::updateDerivedSpacing()
{
    if (!m_countSpin || !m_spacingInfoLabel)
        return;

    const int ringCount = std::max(m_countSpin->value(), 2);
    const double spacingMeters =
        ringCount > 1 ? m_distanceMeters / static_cast<double>(ringCount - 1) : m_distanceMeters;
    m_spacingInfoLabel->setText(
        tr("Gemessene Lane-Laenge: %1 m. Bei %2 Ringen ergibt das ca. %3 m Abstand.")
            .arg(QString::number(m_distanceMeters, 'f', 0))
            .arg(ringCount)
            .arg(QString::number(spacingMeters, 'f', 0)));
}

void CreateTradeLaneDialog::updateCountFromSpacing(int spacingMeters)
{
    if (m_updatingSpacing || !m_countSpin)
        return;

    const int safeSpacing = std::max(spacingMeters, 1);
    const int ringCount = std::max(2, qRound(m_distanceMeters / static_cast<double>(safeSpacing)) + 1);
    m_updatingSpacing = true;
    m_countSpin->setValue(ringCount);
    m_updatingSpacing = false;
    updateDerivedSpacing();
}

CreateTradeLaneRequest CreateTradeLaneDialog::result() const
{
    CreateTradeLaneRequest value;
    value.ringCount = m_countSpin ? m_countSpin->value() : 2;
    value.spacingMeters = m_spacingSpin ? m_spacingSpin->value() : 7500;
    value.startNumber = m_startNumberSpin ? m_startNumberSpin->value() : 1;
    value.loadout = m_loadoutCombo ? m_loadoutCombo->currentText().trimmed() : QString();
    value.reputationDisplay = m_reputationCombo ? m_reputationCombo->currentText().trimmed() : QString();
    value.difficultyLevel = m_difficultySpin ? m_difficultySpin->value() : 1;
    value.pilot = m_pilotCombo ? m_pilotCombo->currentText().trimmed() : QString();
    value.routeName = m_routeNameEdit ? m_routeNameEdit->text().trimmed() : QString();
    value.startSpaceName = m_startSpaceNameEdit ? m_startSpaceNameEdit->text().trimmed() : QString();
    value.endSpaceName = m_endSpaceNameEdit ? m_endSpaceNameEdit->text().trimmed() : QString();
    return value;
}

EditTradeLaneDialog::EditTradeLaneDialog(const QString &laneSummary,
                                         const QStringList &archetypes,
                                         const QStringList &loadouts,
                                         const QStringList &factions,
                                         const QStringList &pilots,
                                         const EditTradeLaneRequest &initial,
                                         QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Trade Lane bearbeiten"));
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);

    auto *summaryLabel = new QLabel(laneSummary, this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(summaryLabel);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_startXSpin = createCoordinateSpinBox(initial.startX, this);
    form->addRow(tr("Start X:"), m_startXSpin);
    m_startZSpin = createCoordinateSpinBox(initial.startZ, this);
    form->addRow(tr("Start Z:"), m_startZSpin);
    m_endXSpin = createCoordinateSpinBox(initial.endX, this);
    form->addRow(tr("Ende X:"), m_endXSpin);
    m_endZSpin = createCoordinateSpinBox(initial.endZ, this);
    form->addRow(tr("Ende Z:"), m_endZSpin);

    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(2, 200);
    m_countSpin->setValue(std::max(initial.ringCount, 2));
    form->addRow(tr("Ring-Anzahl:"), m_countSpin);

    m_spacingSpin = new QSpinBox(this);
    m_spacingSpin->setRange(500, 50000);
    m_spacingSpin->setSingleStep(500);
    m_spacingSpin->setSuffix(tr(" m"));
    m_spacingSpin->setValue(7500);
    form->addRow(tr("Abstand:"), m_spacingSpin);

    m_spacingInfoLabel = new QLabel(this);
    m_spacingInfoLabel->setWordWrap(true);
    m_spacingInfoLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    form->addRow(QString(), m_spacingInfoLabel);

    m_archetypeCombo = createEditableCombo(archetypes, this);
    m_archetypeCombo->setCurrentText(initial.archetype.trimmed());
    form->addRow(tr("Archetype:"), m_archetypeCombo);

    m_loadoutCombo = createEditableCombo(loadouts, this);
    m_loadoutCombo->setCurrentText(initial.loadout.trimmed());
    form->addRow(tr("Loadout:"), m_loadoutCombo);

    m_reputationCombo = createEditableCombo(factions, this);
    m_reputationCombo->setCurrentText(initial.reputationDisplay.trimmed());
    form->addRow(tr("Reputation:"), m_reputationCombo);

    m_difficultySpin = new QSpinBox(this);
    m_difficultySpin->setRange(1, 7);
    m_difficultySpin->setValue(std::max(initial.difficultyLevel, 1));
    form->addRow(tr("difficulty_level:"), m_difficultySpin);

    m_pilotCombo = createEditableCombo(pilots, this);
    m_pilotCombo->setCurrentText(initial.pilot.trimmed());
    form->addRow(tr("Pilot:"), m_pilotCombo);

    m_routeNameEdit = new QLineEdit(initial.routeName.trimmed(), this);
    form->addRow(tr("Route / ids_name:"), m_routeNameEdit);

    m_startSpaceNameEdit = new QLineEdit(initial.startSpaceName.trimmed(), this);
    form->addRow(tr("Start tradelane_space_name:"), m_startSpaceNameEdit);

    m_endSpaceNameEdit = new QLineEdit(initial.endSpaceName.trimmed(), this);
    form->addRow(tr("Ende tradelane_space_name:"), m_endSpaceNameEdit);

    layout->addLayout(form);

    auto *hintLabel = new QLabel(
        tr("Lane-weite Felder werden auf die komplette Ring-Kette angewendet. "
           "tradelane_space_name bleibt auf Start- und Endring beschraenkt."),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(hintLabel);

    auto *buttons = createDialogButtons(this, tr("Anwenden"));
    auto *deleteButton = buttons->addButton(tr("Trade Lane loeschen"), QDialogButtonBox::DestructiveRole);
    QObject::connect(deleteButton, &QPushButton::clicked, this, [this]() {
        m_deleteRequested = true;
        accept();
    });
    layout->addWidget(buttons);

    connect(m_countSpin, &QSpinBox::valueChanged, this, &EditTradeLaneDialog::updateDerivedSpacing);
    connect(m_spacingSpin, &QSpinBox::valueChanged, this, &EditTradeLaneDialog::updateCountFromSpacing);
    connect(m_startXSpin, &QDoubleSpinBox::valueChanged, this, &EditTradeLaneDialog::updateDerivedSpacing);
    connect(m_startZSpin, &QDoubleSpinBox::valueChanged, this, &EditTradeLaneDialog::updateDerivedSpacing);
    connect(m_endXSpin, &QDoubleSpinBox::valueChanged, this, &EditTradeLaneDialog::updateDerivedSpacing);
    connect(m_endZSpin, &QDoubleSpinBox::valueChanged, this, &EditTradeLaneDialog::updateDerivedSpacing);

    updateDerivedSpacing();
}

double EditTradeLaneDialog::currentLaneLengthMeters() const
{
    if (!m_startXSpin || !m_startZSpin || !m_endXSpin || !m_endZSpin)
        return 0.0;
    const double dx = m_endXSpin->value() - m_startXSpin->value();
    const double dz = m_endZSpin->value() - m_startZSpin->value();
    return std::hypot(dx, dz);
}

void EditTradeLaneDialog::updateDerivedSpacing()
{
    if (!m_countSpin || !m_spacingInfoLabel)
        return;

    const double lengthMeters = currentLaneLengthMeters();
    const int ringCount = std::max(m_countSpin->value(), 2);
    const double spacingMeters = ringCount > 1
        ? lengthMeters / static_cast<double>(ringCount - 1)
        : lengthMeters;
    m_spacingInfoLabel->setText(
        tr("Lane-Laenge: %1 m. Bei %2 Ringen ergibt das ca. %3 m Abstand.")
            .arg(QString::number(lengthMeters, 'f', 0))
            .arg(ringCount)
            .arg(QString::number(spacingMeters, 'f', 0)));
}

void EditTradeLaneDialog::updateCountFromSpacing(int spacingMeters)
{
    if (m_updatingSpacing || !m_countSpin)
        return;

    const int safeSpacing = std::max(spacingMeters, 1);
    const int ringCount = std::max(2, qRound(currentLaneLengthMeters() / static_cast<double>(safeSpacing)) + 1);
    m_updatingSpacing = true;
    m_countSpin->setValue(ringCount);
    m_updatingSpacing = false;
    updateDerivedSpacing();
}

EditTradeLaneRequest EditTradeLaneDialog::result() const
{
    EditTradeLaneRequest value;
    value.startX = m_startXSpin ? m_startXSpin->value() : 0.0;
    value.startZ = m_startZSpin ? m_startZSpin->value() : 0.0;
    value.endX = m_endXSpin ? m_endXSpin->value() : 0.0;
    value.endZ = m_endZSpin ? m_endZSpin->value() : 0.0;
    value.ringCount = m_countSpin ? m_countSpin->value() : 2;
    value.archetype = m_archetypeCombo ? m_archetypeCombo->currentText().trimmed() : QString();
    value.loadout = m_loadoutCombo ? m_loadoutCombo->currentText().trimmed() : QString();
    value.reputationDisplay = m_reputationCombo ? m_reputationCombo->currentText().trimmed() : QString();
    value.difficultyLevel = m_difficultySpin ? m_difficultySpin->value() : 1;
    value.pilot = m_pilotCombo ? m_pilotCombo->currentText().trimmed() : QString();
    value.routeName = m_routeNameEdit ? m_routeNameEdit->text().trimmed() : QString();
    value.startSpaceName = m_startSpaceNameEdit ? m_startSpaceNameEdit->text().trimmed() : QString();
    value.endSpaceName = m_endSpaceNameEdit ? m_endSpaceNameEdit->text().trimmed() : QString();
    return value;
}

bool EditTradeLaneDialog::deleteRequested() const
{
    return m_deleteRequested;
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

CreatePlanetDialog::CreatePlanetDialog(const QString &suggestedNickname,
                                       const PlanetCreationCatalog &catalog,
                                       QWidget *parent)
    : QDialog(parent)
    , m_catalog(catalog)
{
    setWindowTitle(tr("Planet erstellen"));
    resize(760, 640);

    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(suggestedNickname, this);
    m_nicknameEdit->setPlaceholderText(tr("Objekt-Nickname, z.B. Li01_planet_001"));
    form->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(this);
    m_ingameNameEdit->setPlaceholderText(tr("Sichtbarer Planetenname"));
    form->addRow(tr("Planet Name:"), m_ingameNameEdit);

    QStringList archetypes = m_catalog.archetypeNicknames();
    if (archetypes.isEmpty())
        archetypes << QStringLiteral("planet_earthgrncld_3000");
    m_archetypeCombo = createEditableCombo(archetypes, this);
    if (m_archetypeCombo->count() > 0)
        m_archetypeCombo->setCurrentIndex(0);
    form->addRow(tr("Planet Archetype:"), m_archetypeCombo);

    m_planetRadiusLabel = new QLabel(this);
    m_planetRadiusLabel->setWordWrap(true);
    form->addRow(tr("Planet Radius:"), m_planetRadiusLabel);

    m_deathZoneRadiusSpin = new QSpinBox(this);
    m_deathZoneRadiusSpin->setRange(1, 2000000);
    form->addRow(tr("Death-Zone Radius:"), m_deathZoneRadiusSpin);

    m_deathZoneDamageSpin = new QSpinBox(this);
    m_deathZoneDamageSpin->setRange(1, 2000000);
    m_deathZoneDamageSpin->setValue(2000000);
    form->addRow(tr("Death-Zone Damage:"), m_deathZoneDamageSpin);

    m_atmosphereRangeSpin = new QSpinBox(this);
    m_atmosphereRangeSpin->setRange(0, 2000000);
    form->addRow(tr("Atmosphere Range:"), m_atmosphereRangeSpin);

    layout->addLayout(form);

    auto *infoLabel = new QLabel(
        tr("Der Infocard-Text wird archetypebasiert vorgeschlagen und beim Erstellen als neuer ids_info-Eintrag gespeichert. Bestehende Shared-Texte werden nicht ueberschrieben."),
        this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(infoLabel);

    auto *toolbar = new QHBoxLayout();
    m_infocardSourceLabel = new QLabel(this);
    m_infocardSourceLabel->setWordWrap(true);
    toolbar->addWidget(m_infocardSourceLabel, 1);
    m_resetInfocardButton = new QPushButton(tr("Vorschlag neu laden"), this);
    toolbar->addWidget(m_resetInfocardButton);
    layout->addLayout(toolbar);

    m_infocardStateLabel = new QLabel(this);
    m_infocardStateLabel->setWordWrap(true);
    m_infocardStateLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    layout->addWidget(m_infocardStateLabel);

    m_infoCardEdit = new QTextEdit(this);
    m_infoCardEdit->setMinimumHeight(220);
    m_infoCardEdit->setPlaceholderText(tr("Infocard-Text fuer den neuen Planeten"));
    layout->addWidget(m_infoCardEdit, 1);

    connect(m_archetypeCombo, &QComboBox::currentTextChanged, this, &CreatePlanetDialog::onArchetypeChanged);
    connect(m_infoCardEdit, &QTextEdit::textChanged, this, &CreatePlanetDialog::onInfocardEdited);
    connect(m_resetInfocardButton, &QPushButton::clicked, this, &CreatePlanetDialog::resetInfocardSuggestion);

    layout->addWidget(createDialogButtons(this));
    applyArchetypeDefaults(m_archetypeCombo->currentText().trimmed(), true);
}

void CreatePlanetDialog::accept()
{
    const CreatePlanetRequest request = result();
    if (request.nickname.isEmpty()) {
        QMessageBox::warning(this, tr("Planet erstellen"), tr("Bitte einen Objekt-Nickname angeben."));
        return;
    }
    if (!PlanetCreationService::isValidNickname(request.nickname)) {
        QMessageBox::warning(this, tr("Planet erstellen"),
                             tr("Der Nickname darf nur Buchstaben, Zahlen und Unterstriche enthalten."));
        return;
    }
    if (request.ingameName.isEmpty()) {
        QMessageBox::warning(this, tr("Planet erstellen"), tr("Bitte einen Planetennamen angeben."));
        return;
    }
    if (request.archetype.isEmpty()) {
        QMessageBox::warning(this, tr("Planet erstellen"), tr("Bitte einen Planet-Archetype auswaehlen."));
        return;
    }
    if (request.infoCardText.isEmpty()) {
        QMessageBox::warning(this, tr("Planet erstellen"), tr("Bitte einen Infocard-Text angeben."));
        return;
    }
    if (request.planetRadius > 0 && request.deathZoneRadius < request.planetRadius) {
        QMessageBox::warning(this, tr("Planet erstellen"),
                             tr("Der Death-Zone-Radius darf nicht kleiner als der Planet-Radius sein."));
        return;
    }
    if (request.planetRadius > 0 && request.atmosphereRange < request.planetRadius) {
        QMessageBox::warning(this, tr("Planet erstellen"),
                             tr("Die Atmosphaeren-Reichweite darf nicht kleiner als der Planet-Radius sein."));
        return;
    }

    QDialog::accept();
}

void CreatePlanetDialog::onArchetypeChanged(const QString &archetype)
{
    applyArchetypeDefaults(archetype, false);
}

void CreatePlanetDialog::onInfocardEdited()
{
    if (m_updatingInfocardText || !m_infoCardEdit)
        return;

    m_infocardManuallyEdited = m_infoCardEdit->toPlainText().trimmed() != m_lastSuggestedInfocard.trimmed();
    m_infocardStateLabel->setText(m_infocardManuallyEdited
                                      ? tr("Manuell angepasst. Archetype-Wechsel behalten den aktuellen Text, bis du den Vorschlag explizit neu laedst.")
                                      : tr("Der aktuelle Text folgt dem geladenen Archetype-Vorschlag."));
}

void CreatePlanetDialog::resetInfocardSuggestion()
{
    applyArchetypeDefaults(m_archetypeCombo ? m_archetypeCombo->currentText().trimmed() : QString(), true);
}

void CreatePlanetDialog::applyArchetypeDefaults(const QString &archetype, bool forceInfocardRefresh)
{
    const PlanetArchetypeOption option = m_catalog.optionForArchetype(archetype);
    const int planetRadius = PlanetCreationService::derivePlanetRadius(archetype, m_catalog);

    if (m_planetRadiusLabel) {
        m_planetRadiusLabel->setText(planetRadius > 0
                                         ? tr("%1 m aus solar_radius des Archetypes.").arg(planetRadius)
                                         : tr("Kein solar_radius gefunden. Die Defaultwerte bleiben editierbar."));
    }
    if (m_deathZoneRadiusSpin)
        m_deathZoneRadiusSpin->setValue(PlanetCreationService::defaultDeathZoneRadius(planetRadius));
    if (m_atmosphereRangeSpin)
        m_atmosphereRangeSpin->setValue(PlanetCreationService::defaultAtmosphereRange(planetRadius));

    m_infocardSourceLabel->setText(option.sourceObjectNickname.trimmed().isEmpty()
                                       ? tr("Kein archetypegleicher Planet mit Infocard gefunden. Bitte Text manuell eintragen.")
                                       : tr("Vorschlag aus %1 (ids_info %2).").arg(
                                             option.sourceObjectNickname,
                                             option.sourceIdsInfo > 0 ? QString::number(option.sourceIdsInfo)
                                                                      : tr("ohne ID")));

    const QString suggestedText = option.suggestedInfocardText.trimmed();
    const bool shouldRefreshText = forceInfocardRefresh
        || !m_infocardManuallyEdited
        || (m_infoCardEdit && m_infoCardEdit->toPlainText().trimmed().isEmpty())
        || (m_infoCardEdit && m_infoCardEdit->toPlainText().trimmed() == m_lastSuggestedInfocard.trimmed());

    m_lastSuggestedInfocard = suggestedText;
    if (shouldRefreshText)
        setInfocardText(suggestedText);

    if (!m_infocardManuallyEdited || forceInfocardRefresh) {
        m_infocardManuallyEdited = false;
        m_infocardStateLabel->setText(suggestedText.isEmpty()
                                          ? tr("Kein Standardtext gefunden. Der finale Text wird trotzdem als neuer ids_info-Eintrag gespeichert.")
                                          : tr("Der aktuelle Text folgt dem geladenen Archetype-Vorschlag."));
    }
}

void CreatePlanetDialog::setInfocardText(const QString &text)
{
    if (!m_infoCardEdit)
        return;
    m_updatingInfocardText = true;
    m_infoCardEdit->setPlainText(text);
    m_updatingInfocardText = false;
}

CreatePlanetRequest CreatePlanetDialog::result() const
{
    CreatePlanetRequest value;
    value.nickname = m_nicknameEdit ? m_nicknameEdit->text().trimmed() : QString();
    value.ingameName = m_ingameNameEdit ? m_ingameNameEdit->text().trimmed() : QString();
    value.infoCardText = m_infoCardEdit ? m_infoCardEdit->toPlainText().trimmed() : QString();
    value.archetype = m_archetypeCombo ? m_archetypeCombo->currentText().trimmed() : QString();
    value.planetRadius = PlanetCreationService::derivePlanetRadius(value.archetype, m_catalog);
    value.deathZoneRadius = m_deathZoneRadiusSpin ? m_deathZoneRadiusSpin->value() : 0;
    value.deathZoneDamage = m_deathZoneDamageSpin ? m_deathZoneDamageSpin->value() : 0;
    value.atmosphereRange = m_atmosphereRangeSpin ? m_atmosphereRangeSpin->value() : 0;
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
