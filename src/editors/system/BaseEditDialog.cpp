#include "BaseEditDialog.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/view3d/ModelViewport3D.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QStackedLayout>
#include <QStringListModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString normalizedKey(const QString &value)
{
    return value.trimmed().toLower();
}

QString primaryGamePath()
{
    return flatlas::core::EditingContext::instance().primaryGamePath();
}

void configureContainsCompleter(QComboBox *combo)
{
    if (!combo)
        return;

    QStringList values;
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

QComboBox *createEditableCombo(const QStringList &values, const QString &current, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString());
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value.trimmed());
    }
    configureContainsCompleter(combo);
    if (!current.trimmed().isEmpty())
        combo->setCurrentText(current.trimmed());
    return combo;
}

QComboBox *createMappedEditableCombo(const QVector<QPair<QString, QString>> &items,
                                     const QString &currentValue,
                                     QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItem(QString(), QString());
    for (const auto &item : items) {
        const QString label = item.first.trimmed();
        const QString value = item.second.trimmed();
        if (!label.isEmpty())
            combo->addItem(label, value);
    }
    configureContainsCompleter(combo);
    if (!currentValue.trimmed().isEmpty()) {
        for (int index = 0; index < combo->count(); ++index) {
            if (combo->itemData(index).toString().compare(currentValue.trimmed(), Qt::CaseInsensitive) == 0) {
                combo->setCurrentIndex(index);
                return combo;
            }
        }
        combo->setCurrentText(currentValue.trimmed());
    }
    return combo;
}

QString comboStoredValue(const QComboBox *combo)
{
    if (!combo)
        return {};
    const QString dataValue = combo->currentData().toString().trimmed();
    if (!dataValue.isEmpty())
        return dataValue;
    const QString text = combo->currentText().trimmed();
    const int separator = text.indexOf(QStringLiteral(" - "));
    return separator >= 0 ? text.left(separator).trimmed() : text;
}

struct BaseDialogCatalog {
    QStringList archetypes;
    QStringList loadouts;
    QVector<QPair<QString, QString>> factions;
    QStringList pilots;
    QStringList voices;
    QStringList heads;
    QStringList bodies;
    QVector<QPair<QString, QString>> templateBases;
    QHash<QString, QString> archetypeModelPaths;
};

const BaseDialogCatalog &sharedBaseDialogCatalog()
{
    static QString cachedGameKey;
    static BaseDialogCatalog catalog;

    const QString gamePath = primaryGamePath();
    const QString gameKey = normalizedKey(gamePath);
    if (cachedGameKey == gameKey)
        return catalog;

    cachedGameKey = gameKey;
    catalog = {};
    catalog.voices = {
        QStringLiteral("atc_leg_m01"),
        QStringLiteral("atc_leg_f01"),
        QStringLiteral("atc_leg_f01a"),
        QStringLiteral("mc_leg_m01"),
    };

    const QString dataDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
    if (dataDir.isEmpty())
        return catalog;

    const QString bodypartsPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("CHARACTERS/bodyparts.ini"));
    if (!bodypartsPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(bodypartsPath);
        QSet<QString> seenHeads;
        QSet<QString> seenBodies;
        for (const IniSection &section : doc) {
            const QString sectionName = normalizedKey(section.name);
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty())
                continue;
            if (sectionName == QStringLiteral("head") && !seenHeads.contains(key)) {
                seenHeads.insert(key);
                catalog.heads.append(nickname);
            } else if (sectionName == QStringLiteral("body") && !seenBodies.contains(key)) {
                seenBodies.insert(key);
                catalog.bodies.append(nickname);
            }
        }
        std::sort(catalog.heads.begin(), catalog.heads.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
        std::sort(catalog.bodies.begin(), catalog.bodies.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
    }

    const QString solarArchPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
    if (!solarArchPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(solarArchPath);
        QSet<QString> seen;
        for (const IniSection &section : doc) {
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;

            const QString key = normalizedKey(nickname);
            if (!seen.contains(key)) {
                seen.insert(key);
                catalog.archetypes.append(nickname);
            }

            const QString relativeModelPath = section.value(QStringLiteral("DA_archetype")).trimmed();
            if (!relativeModelPath.isEmpty()) {
                const QString modelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModelPath);
                if (!modelPath.isEmpty())
                    catalog.archetypeModelPaths.insert(key, modelPath);
            }
        }
        std::sort(catalog.archetypes.begin(), catalog.archetypes.end(), [](const QString &left, const QString &right) {
            return left.compare(right, Qt::CaseInsensitive) < 0;
        });
    }

    const QStringList loadoutSources = {
        QStringLiteral("SHIPS/loadouts.ini"),
        QStringLiteral("SHIPS/loadouts_special.ini"),
        QStringLiteral("SOLAR/loadouts.ini"),
    };
    QSet<QString> seenLoadouts;
    for (const QString &relativePath : loadoutSources) {
        const QString absolutePath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativePath);
        if (absolutePath.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(absolutePath);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenLoadouts.contains(key))
                continue;
            seenLoadouts.insert(key);
            catalog.loadouts.append(nickname);
        }
    }
    std::sort(catalog.loadouts.begin(), catalog.loadouts.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    const QString exeDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
    IdsStringTable ids;
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    const QString initialWorldPath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
    if (!initialWorldPath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(initialWorldPath);
        QSet<QString> seenFactions;
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            if (nickname.isEmpty())
                continue;
            const QString key = normalizedKey(nickname);
            if (seenFactions.contains(key))
                continue;
            seenFactions.insert(key);

            bool ok = false;
            const int idsName = section.value(QStringLiteral("ids_name")).trimmed().toInt(&ok);
            const QString displayName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
            const QString label = displayName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, displayName);
            catalog.factions.append({label, nickname});
        }
        std::sort(catalog.factions.begin(), catalog.factions.end(), [](const auto &left, const auto &right) {
            return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
        });
    }

    const QString missionsDir = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("MISSIONS"));
    const QStringList pilotSources = {
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_population.ini")),
        flatlas::core::PathUtils::ciResolvePath(missionsDir, QStringLiteral("pilots_story.ini")),
    };
    QSet<QString> seenPilots;
    for (const QString &pilotSource : pilotSources) {
        if (pilotSource.isEmpty())
            continue;
        const IniDocument doc = IniParser::parseFile(pilotSource);
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Pilot"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenPilots.contains(key) || !key.startsWith(QStringLiteral("pilot_solar")))
                continue;
            seenPilots.insert(key);
            catalog.pilots.append(nickname);
        }
    }
    std::sort(catalog.pilots.begin(), catalog.pilots.end(), [](const QString &left, const QString &right) {
        return left.compare(right, Qt::CaseInsensitive) < 0;
    });

    const QString universePath = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("UNIVERSE/universe.ini"));
    if (!universePath.isEmpty()) {
        const IniDocument doc = IniParser::parseFile(universePath);
        QSet<QString> seenTemplates;
        for (const IniSection &section : doc) {
            if (section.name.compare(QStringLiteral("Base"), Qt::CaseInsensitive) != 0)
                continue;
            const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
            const QString key = normalizedKey(nickname);
            if (nickname.isEmpty() || seenTemplates.contains(key))
                continue;
            seenTemplates.insert(key);
            bool ok = false;
            const int idsName = section.value(QStringLiteral("strid_name")).trimmed().toInt(&ok);
            const QString ingameName = ok && idsName > 0 ? ids.getString(idsName).trimmed() : QString();
            const QString label = ingameName.isEmpty() ? nickname : QStringLiteral("%1 - %2").arg(nickname, ingameName);
            catalog.templateBases.append({label, nickname});
        }
        std::sort(catalog.templateBases.begin(), catalog.templateBases.end(), [](const auto &left, const auto &right) {
            return left.first.compare(right.first, Qt::CaseInsensitive) < 0;
        });
    }

    return catalog;
}

QWidget *createPreviewFrame(flatlas::rendering::ModelViewport3D **outPreview,
                            QLabel **outFallback,
                            QStackedLayout **outStack,
                            QWidget *parent)
{
    auto *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *host = new QWidget(frame);
    auto *stack = new QStackedLayout(host);
    stack->setContentsMargins(0, 0, 0, 0);

    auto *preview = new flatlas::rendering::ModelViewport3D(host);
    stack->addWidget(preview);

    auto *fallback = new QLabel(QObject::tr("Waehle einen Archetype um die Vorschau zu laden."), host);
    fallback->setAlignment(Qt::AlignCenter);
    fallback->setWordWrap(true);
    fallback->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    stack->addWidget(fallback);
    stack->setCurrentWidget(fallback);

    layout->addWidget(host, 1);

    if (outPreview)
        *outPreview = preview;
    if (outFallback)
        *outFallback = fallback;
    if (outStack)
        *outStack = stack;
    return frame;
}

} // namespace

namespace flatlas::editors {

BaseEditDialog::BaseEditDialog(const BaseEditState &state,
                               const QHash<QString, QString> &textOverrides,
                               QWidget *parent)
    : QDialog(parent)
    , m_initialState(state)
    , m_textOverrides(textOverrides)
    , m_roomStates(state.rooms)
{
    setWindowTitle(state.editMode ? tr("Base bearbeiten") : tr("Base erstellen"));
    resize(1240, 760);

    const BaseDialogCatalog &catalog = sharedBaseDialogCatalog();

    auto *root = new QHBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    root->addWidget(splitter, 1);

    auto *scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);
    scroll->setWidget(content);
    splitter->addWidget(scroll);

    auto *previewSidebar = new QWidget(splitter);
    auto *previewSidebarLayout = new QVBoxLayout(previewSidebar);
    previewSidebarLayout->setContentsMargins(12, 12, 12, 12);
    previewSidebarLayout->setSpacing(8);
    splitter->addWidget(previewSidebar);
    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 5);

    auto *baseGroup = new QGroupBox(tr("Base"), content);
    auto *baseForm = new QFormLayout(baseGroup);
    m_baseNicknameEdit = new QLineEdit(state.baseNickname, baseGroup);
    m_objectNicknameEdit = new QLineEdit(state.objectNickname, baseGroup);
    m_displayNameEdit = new QLineEdit(state.displayName, baseGroup);
    if (state.editMode) {
        m_baseNicknameEdit->setReadOnly(true);
        m_objectNicknameEdit->setReadOnly(true);
    }
    baseForm->addRow(tr("Base Nickname:"), m_baseNicknameEdit);
    baseForm->addRow(tr("Objekt Nickname:"), m_objectNicknameEdit);
    baseForm->addRow(tr("Name:"), m_displayNameEdit);
    contentLayout->addWidget(baseGroup);

    auto *objectGroup = new QGroupBox(tr("Space-Objekt"), content);
    auto *objectForm = new QFormLayout(objectGroup);
    m_archetypeCombo = createEditableCombo(catalog.archetypes, state.archetype, objectGroup);
    m_loadoutCombo = createEditableCombo(catalog.loadouts, state.loadout, objectGroup);
    m_reputationCombo = createMappedEditableCombo(catalog.factions, state.reputation, objectGroup);
    m_pilotCombo = createEditableCombo(catalog.pilots, state.pilot, objectGroup);
    m_voiceCombo = createEditableCombo(catalog.voices, state.voice, objectGroup);
    m_headCombo = createEditableCombo(catalog.heads, state.head.isEmpty() ? QStringLiteral("benchmark_male_head") : state.head, objectGroup);
    m_bodyCombo = createEditableCombo(catalog.bodies, state.body.isEmpty() ? QStringLiteral("benchmark_male_body") : state.body, objectGroup);
    if (state.editMode)
        m_archetypeCombo->setEnabled(false);

    objectForm->addRow(tr("Archetype:"), m_archetypeCombo);
    objectForm->addRow(tr("Loadout:"), m_loadoutCombo);
    objectForm->addRow(tr("Reputation:"), m_reputationCombo);
    objectForm->addRow(tr("Pilot:"), m_pilotCombo);
    objectForm->addRow(tr("Voice:"), m_voiceCombo);
    auto *costumeGroup = new QGroupBox(tr("Space Costume"), objectGroup);
    auto *costumeForm = new QFormLayout(costumeGroup);
    costumeForm->addRow(tr("Head:"), m_headCombo);
    costumeForm->addRow(tr("Body:"), m_bodyCombo);
    objectForm->addRow(costumeGroup);
    contentLayout->addWidget(objectGroup);

    auto *roomsGroup = new QGroupBox(tr("Raeume"), content);
    auto *roomsLayout = new QVBoxLayout(roomsGroup);
    auto *templateForm = new QFormLayout();
    m_templateCombo = createMappedEditableCombo(catalog.templateBases, QString(), roomsGroup);
    m_copyNpcsCheck = new QCheckBox(tr("Copy NPCs"), roomsGroup);
    m_copyNpcsCheck->setChecked(true);
    m_randomNpcAppearanceCheck = new QCheckBox(tr("Random NPC head/body"), roomsGroup);
    m_templateInfoLabel = new QLabel(QString(), roomsGroup);
    m_templateInfoLabel->setWordWrap(true);
    m_templateInfoLabel->setStyleSheet(QStringLiteral("color:#9aa3ad;"));
    templateForm->addRow(tr("Room Template kopieren:"), m_templateCombo);
    templateForm->addRow(QString(), m_copyNpcsCheck);
    templateForm->addRow(QString(), m_randomNpcAppearanceCheck);
    templateForm->addRow(QString(), m_templateInfoLabel);
    roomsLayout->addLayout(templateForm);

    m_roomTable = new QTableWidget(0, 3, roomsGroup);
    m_roomTable->setHorizontalHeaderLabels({tr("Use"), tr("Room"), tr("Scene")});
    m_roomTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_roomTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_roomTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_roomTable->verticalHeader()->setVisible(false);
    m_roomTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_roomTable->setSelectionMode(QAbstractItemView::SingleSelection);
    roomsLayout->addWidget(m_roomTable, 1);

    auto *roomButtons = new QHBoxLayout();
    m_addRoomButton = new QPushButton(tr("Raum hinzufuegen"), roomsGroup);
    m_removeRoomButton = new QPushButton(tr("Ausgewaehlten Raum entfernen"), roomsGroup);
    roomButtons->addWidget(m_addRoomButton);
    roomButtons->addWidget(m_removeRoomButton);
    roomButtons->addStretch(1);
    roomsLayout->addLayout(roomButtons);

    roomsLayout->addWidget(new QLabel(tr("Fixture-NPCs des aktuell ausgewaehlten Raums."), roomsGroup));

    m_npcTable = new QTableWidget(0, 2, roomsGroup);
    m_npcTable->setHorizontalHeaderLabels({tr("NPC"), tr("Rolle")});
    m_npcTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_npcTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_npcTable->verticalHeader()->setVisible(false);
    m_npcTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_npcTable->setSelectionMode(QAbstractItemView::SingleSelection);
    roomsLayout->addWidget(m_npcTable, 1);

    auto *npcButtons = new QHBoxLayout();
    m_addNpcButton = new QPushButton(tr("NPC hinzufuegen"), roomsGroup);
    m_removeNpcButton = new QPushButton(tr("Ausgewaehlten NPC entfernen"), roomsGroup);
    npcButtons->addWidget(m_addNpcButton);
    npcButtons->addWidget(m_removeNpcButton);
    npcButtons->addStretch(1);
    roomsLayout->addLayout(npcButtons);

    auto *roomsMetaForm = new QFormLayout();
    m_startRoomCombo = new QComboBox(roomsGroup);
    m_priceVarianceSpin = new QDoubleSpinBox(roomsGroup);
    m_priceVarianceSpin->setRange(0.0, 1.0);
    m_priceVarianceSpin->setSingleStep(0.05);
    m_priceVarianceSpin->setDecimals(2);
    m_priceVarianceSpin->setValue(state.priceVariance);
    roomsMetaForm->addRow(tr("Start-Raum"), m_startRoomCombo);
    roomsMetaForm->addRow(tr("Price Variance"), m_priceVarianceSpin);
    roomsLayout->addLayout(roomsMetaForm);
    contentLayout->addWidget(roomsGroup);

    auto *universeGroup = new QGroupBox(tr("Universe"), content);
    auto *universeForm = new QFormLayout(universeGroup);
    m_bgcsEdit = new QLineEdit(state.bgcsBaseRunBy, universeGroup);
    universeForm->addRow(tr("BGCS_base_run_by:"), m_bgcsEdit);
    contentLayout->addWidget(universeGroup);

    auto *infocardGroup = new QGroupBox(tr("ids_info"), content);
    auto *infocardLayout = new QVBoxLayout(infocardGroup);
    m_infocardEdit = new QPlainTextEdit(infocardGroup);
    m_infocardEdit->setPlainText(state.infocardXml);
    m_infocardEdit->setPlaceholderText(tr("ids_info Text"));
    infocardLayout->addWidget(m_infocardEdit);
    contentLayout->addWidget(infocardGroup);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, content);
    contentLayout->addWidget(buttons);
    contentLayout->addStretch(1);

    auto *previewTitle = new QLabel(tr("3D Preview"), previewSidebar);
    previewTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    auto *previewHelp = new QLabel(tr("Die Vorschau folgt dem aktuell gewaehlten Archetype."), previewSidebar);
    previewHelp->setWordWrap(true);
    previewSidebarLayout->addWidget(previewTitle);
    previewSidebarLayout->addWidget(previewHelp);
    previewSidebarLayout->addWidget(createPreviewFrame(&m_preview, &m_previewFallback, &m_previewStack, previewSidebar), 1);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_addRoomButton, &QPushButton::clicked, this, &BaseEditDialog::addRoom);
    connect(m_removeRoomButton, &QPushButton::clicked, this, &BaseEditDialog::removeSelectedRoom);
    connect(m_roomTable, &QTableWidget::itemChanged, this, &BaseEditDialog::onRoomItemChanged);
    connect(m_roomTable, &QTableWidget::itemSelectionChanged, this, &BaseEditDialog::onRoomSelectionChanged);
    connect(m_npcTable, &QTableWidget::itemChanged, this, &BaseEditDialog::onNpcItemChanged);
    connect(m_addNpcButton, &QPushButton::clicked, this, &BaseEditDialog::addNpc);
    connect(m_removeNpcButton, &QPushButton::clicked, this, &BaseEditDialog::removeSelectedNpc);
    connect(m_archetypeCombo, &QComboBox::currentTextChanged, this, [this]() {
        applyArchetypeDefaults();
        refreshPreview();
    });
    connect(m_templateCombo, &QComboBox::currentTextChanged, this, [this]() { applyTemplateSelection(); });
    connect(m_copyNpcsCheck, &QCheckBox::toggled, this, [this]() { applyTemplateSelection(); });

    populateRooms(m_roomStates);
    refreshStartRooms();
    if (!state.startRoom.trimmed().isEmpty())
        m_startRoomCombo->setCurrentText(state.startRoom.trimmed());
    if (m_roomTable->rowCount() > 0) {
        m_roomTable->selectRow(0);
        populateNpcTable(0);
    }
    m_lastSuggestedLoadout = state.loadout.trimmed();
    m_lastSuggestedIdsInfo = state.infocardXml.trimmed();
    if (state.editMode) {
        m_templateCombo->setEnabled(false);
        m_copyNpcsCheck->setEnabled(false);
        m_randomNpcAppearanceCheck->setEnabled(false);
    }
    refreshPreview();
}

void BaseEditDialog::populateRooms(const QVector<BaseRoomState> &rooms)
{
    m_roomTable->blockSignals(true);
    m_roomTable->setRowCount(0);
    for (const BaseRoomState &room : rooms) {
        const int row = m_roomTable->rowCount();
        m_roomTable->insertRow(row);

        auto *enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        enabledItem->setCheckState(room.enabled ? Qt::Checked : Qt::Unchecked);
        m_roomTable->setItem(row, 0, enabledItem);

        auto *roomItem = new QTableWidgetItem(room.roomName);
        m_roomTable->setItem(row, 1, roomItem);
        populateSceneCombo(row, room.roomName, room.scenePath);
    }
    m_roomTable->blockSignals(false);
}

BaseRoomState BaseEditDialog::roomFromRow(int row) const
{
    BaseRoomState room;
    if (const auto *enabledItem = m_roomTable->item(row, 0))
        room.enabled = enabledItem->checkState() == Qt::Checked;
    if (const auto *roomItem = m_roomTable->item(row, 1))
        room.roomName = roomItem->text().trimmed();
    if (auto *sceneCombo = qobject_cast<QComboBox *>(m_roomTable->cellWidget(row, 2)))
        room.scenePath = sceneCombo->currentText().trimmed();
    return room;
}

BaseEditState BaseEditDialog::state() const
{
    BaseEditState result = m_initialState;
    result.objectNickname = m_objectNicknameEdit->text().trimmed();
    result.baseNickname = m_baseNicknameEdit->text().trimmed();
    result.archetype = m_archetypeCombo->currentText().trimmed();
    result.loadout = m_loadoutCombo->currentText().trimmed();
    result.reputation = comboStoredValue(m_reputationCombo);
    result.pilot = m_pilotCombo->currentText().trimmed();
    result.voice = m_voiceCombo->currentText().trimmed();
    result.head = m_headCombo->currentText().trimmed();
    result.body = m_bodyCombo->currentText().trimmed();
    result.bgcsBaseRunBy = m_bgcsEdit->text().trimmed();
    result.displayName = m_displayNameEdit->text().trimmed();
    result.startRoom = m_startRoomCombo->currentText().trimmed();
    result.priceVariance = m_priceVarianceSpin->value();
    result.infocardXml = m_infocardEdit->toPlainText().trimmed();
    result.rooms = m_roomStates;
    return result;
}

void BaseEditDialog::refreshStartRooms()
{
    const QString current = m_startRoomCombo->currentText().trimmed();
    QStringList rooms;
    for (int row = 0; row < m_roomTable->rowCount(); ++row) {
        const BaseRoomState room = roomFromRow(row);
        if (room.enabled && !room.roomName.trimmed().isEmpty())
            rooms.append(room.roomName.trimmed());
    }
    rooms.removeDuplicates();

    m_startRoomCombo->blockSignals(true);
    m_startRoomCombo->clear();
    m_startRoomCombo->addItems(rooms);
    if (!current.isEmpty() && rooms.contains(current, Qt::CaseInsensitive))
        m_startRoomCombo->setCurrentText(current);
    else if (rooms.contains(QStringLiteral("Deck")))
        m_startRoomCombo->setCurrentText(QStringLiteral("Deck"));
    else if (rooms.contains(QStringLiteral("Cityscape")))
        m_startRoomCombo->setCurrentText(QStringLiteral("Cityscape"));
    else if (!rooms.isEmpty())
        m_startRoomCombo->setCurrentIndex(0);
    m_startRoomCombo->blockSignals(false);
}

void BaseEditDialog::addRoom()
{
    bool ok = false;
    const QString roomName = QInputDialog::getText(this,
                                                   tr("Raum hinzufuegen"),
                                                   tr("Raumname"),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("Deck"),
                                                   &ok)
        .trimmed();
    if (!ok || roomName.isEmpty())
        return;

    const int row = m_roomTable->rowCount();
    BaseRoomState room;
    room.enabled = true;
    room.roomName = roomName;
    m_roomStates.append(room);
    populateRooms(m_roomStates);
    m_roomTable->selectRow(row);
    populateNpcTable(row);
    refreshStartRooms();
}

void BaseEditDialog::removeSelectedRoom()
{
    const int row = m_roomTable->currentRow();
    if (row < 0)
        return;

    m_roomStates.removeAt(row);
    populateRooms(m_roomStates);
    if (row < m_roomTable->rowCount())
        m_roomTable->selectRow(row);
    else if (m_roomTable->rowCount() > 0)
        m_roomTable->selectRow(m_roomTable->rowCount() - 1);
    populateNpcTable(m_roomTable->currentRow());
    refreshStartRooms();
}

void BaseEditDialog::onRoomSelectionChanged()
{
    populateNpcTable(m_roomTable->currentRow());
}

void BaseEditDialog::onRoomItemChanged(QTableWidgetItem *item)
{
    if (!item)
        return;

    const int row = item->row();
    if (row < 0 || row >= m_roomStates.size())
        return;

    BaseRoomState updated = m_roomStates.at(row);
    const BaseRoomState visibleState = roomFromRow(row);
    updated.enabled = visibleState.enabled;
    updated.roomName = visibleState.roomName;
    updated.scenePath = visibleState.scenePath;
    m_roomStates[row] = updated;
    if (item->column() == 1)
        populateSceneCombo(row, m_roomStates[row].roomName, m_roomStates[row].scenePath);
    refreshStartRooms();
    if (row == m_roomTable->currentRow())
        populateNpcTable(row);
}

void BaseEditDialog::onNpcItemChanged(QTableWidgetItem *item)
{
    Q_UNUSED(item);
    syncNpcStateFromTable();
}

void BaseEditDialog::addNpc()
{
    const int roomRow = m_roomTable->currentRow();
    if (roomRow < 0 || roomRow >= m_roomStates.size())
        return;

    BaseRoomNpcState npc;
    npc.role = roleChoicesForRoom(m_roomStates.at(roomRow).roomName).value(0);
    m_roomStates[roomRow].npcs.append(npc);
    populateNpcTable(roomRow);
}

void BaseEditDialog::removeSelectedNpc()
{
    const int roomRow = m_roomTable->currentRow();
    const int npcRow = m_npcTable ? m_npcTable->currentRow() : -1;
    if (roomRow < 0 || roomRow >= m_roomStates.size() || npcRow < 0 || npcRow >= m_roomStates.at(roomRow).npcs.size())
        return;

    m_roomStates[roomRow].npcs.removeAt(npcRow);
    populateNpcTable(roomRow);
}

void BaseEditDialog::populateNpcTable(int roomRow)
{
    if (!m_npcTable)
        return;

    m_npcTable->blockSignals(true);
    m_npcTable->setRowCount(0);

    const bool validRoom = roomRow >= 0 && roomRow < m_roomStates.size();
    m_npcTable->setEnabled(validRoom);
    if (m_addNpcButton)
        m_addNpcButton->setEnabled(validRoom);
    if (m_removeNpcButton)
        m_removeNpcButton->setEnabled(validRoom && !m_roomStates.at(roomRow).npcs.isEmpty());
    if (!validRoom) {
        m_npcTable->blockSignals(false);
        return;
    }

    const QStringList roleChoices = roleChoicesForRoom(m_roomStates.at(roomRow).roomName);
    const QVector<BaseRoomNpcState> npcs = m_roomStates.at(roomRow).npcs;
    for (int index = 0; index < npcs.size(); ++index) {
        m_npcTable->insertRow(index);
        m_npcTable->setItem(index, 0, new QTableWidgetItem(npcs.at(index).nickname));

        auto *roleCombo = new QComboBox(m_npcTable);
        roleCombo->addItems(roleChoices);
        if (!npcs.at(index).role.trimmed().isEmpty() && roleCombo->findText(npcs.at(index).role.trimmed()) < 0)
            roleCombo->addItem(npcs.at(index).role.trimmed());
        roleCombo->setCurrentText(npcs.at(index).role.trimmed().isEmpty() ? roleChoices.value(0) : npcs.at(index).role.trimmed());
        connect(roleCombo, &QComboBox::currentTextChanged, this, [this]() { syncNpcStateFromTable(); });
        m_npcTable->setCellWidget(index, 1, roleCombo);
    }

    m_npcTable->blockSignals(false);
}

void BaseEditDialog::syncNpcStateFromTable()
{
    const int roomRow = m_roomTable->currentRow();
    if (roomRow < 0 || roomRow >= m_roomStates.size() || !m_npcTable)
        return;

    QVector<BaseRoomNpcState> npcs;
    for (int row = 0; row < m_npcTable->rowCount(); ++row) {
        BaseRoomNpcState npc = m_roomStates.at(roomRow).npcs.value(row);
        if (const auto *nicknameItem = m_npcTable->item(row, 0))
            npc.nickname = nicknameItem->text().trimmed();
        if (auto *roleCombo = qobject_cast<QComboBox *>(m_npcTable->cellWidget(row, 1)))
            npc.role = roleCombo->currentText().trimmed();
        if (!npc.nickname.isEmpty())
            npcs.append(npc);
    }
    m_roomStates[roomRow].npcs = npcs;
    if (m_removeNpcButton)
        m_removeNpcButton->setEnabled(!npcs.isEmpty());
}

QStringList BaseEditDialog::roleChoicesForRoom(const QString &roomName) const
{
    const QString room = roomName.trimmed().toLower();
    if (room == QStringLiteral("bar"))
        return {QStringLiteral("bartender"), QStringLiteral("BarFly"), QStringLiteral("NewsVendor")};
    if (room == QStringLiteral("trader"))
        return {QStringLiteral("trader")};
    if (room == QStringLiteral("equipment") || room == QStringLiteral("equip"))
        return {QStringLiteral("Equipment")};
    if (room == QStringLiteral("shipdealer") || room == QStringLiteral("ship_dealer") || room == QStringLiteral("ship-dealer"))
        return {QStringLiteral("ShipDealer")};
    if (room == QStringLiteral("deck"))
        return {QStringLiteral("ShipDealer"), QStringLiteral("trader"), QStringLiteral("Equipment"), QStringLiteral("bartender")};
    if (room == QStringLiteral("cityscape"))
        return {QStringLiteral("trader")};
    return {QStringLiteral("trader")};
}

void BaseEditDialog::populateSceneCombo(int row, const QString &roomName, const QString &currentScene)
{
    auto *sceneCombo = new QComboBox(m_roomTable);
    sceneCombo->setEditable(true);

    QStringList options;
    const QString room = roomName.trimmed().toLower();
    if (room == QStringLiteral("deck"))
        options << QStringLiteral("Scripts\\Bases\\Li_08_Deck_ambi_int_01.thn");
    else if (room == QStringLiteral("bar"))
        options << QStringLiteral("Scripts\\Bases\\Li_09_bar_ambi_int_s020x.thn");
    else if (room == QStringLiteral("trader"))
        options << QStringLiteral("Scripts\\Bases\\Li_01_Trader_ambi_int_01.thn");
    else if (room == QStringLiteral("equipment"))
        options << QStringLiteral("Scripts\\Bases\\Li_01_equipment_ambi_int_01.thn");
    else if (room == QStringLiteral("shipdealer"))
        options << QStringLiteral("Scripts\\Bases\\Li_01_shipdealer_ambi_int_01.thn");
    else if (room == QStringLiteral("cityscape"))
        options << QStringLiteral("Scripts\\Bases\\Li_01_cityscape_ambi_day_01.thn");

    if (!currentScene.trimmed().isEmpty() && !options.contains(currentScene.trimmed(), Qt::CaseInsensitive))
        options << currentScene.trimmed();
    sceneCombo->addItems(options);
    configureContainsCompleter(sceneCombo);
    if (!currentScene.trimmed().isEmpty())
        sceneCombo->setCurrentText(currentScene.trimmed());

    connect(sceneCombo, &QComboBox::currentTextChanged, this, [this, row]() {
        if (row >= 0 && row < m_roomStates.size())
            m_roomStates[row].scenePath = qobject_cast<QComboBox *>(m_roomTable->cellWidget(row, 2))->currentText().trimmed();
    });
    m_roomTable->setCellWidget(row, 2, sceneCombo);
}

void BaseEditDialog::refreshPreview()
{
    if (!m_preview || !m_previewFallback || !m_previewStack || !m_archetypeCombo)
        return;

    const BaseDialogCatalog &catalog = sharedBaseDialogCatalog();
    const QString archetype = m_archetypeCombo->currentText().trimmed();
    auto showFallback = [&](const QString &message) {
        m_previewFallback->setText(message);
        m_previewStack->setCurrentWidget(m_previewFallback);
        m_preview->clearModel();
    };

    if (archetype.isEmpty()) {
        showFallback(tr("Waehle einen Archetype um die Vorschau zu laden."));
        return;
    }

    const QString modelPath = catalog.archetypeModelPaths.value(normalizedKey(archetype));
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        showFallback(tr("Kein 3D-Modell fuer %1 vorhanden.").arg(archetype));
        return;
    }

    QString errorMessage;
    if (!m_preview->loadModelFile(modelPath, &errorMessage)) {
        showFallback(tr("Modell konnte nicht geladen werden: %1")
                         .arg(errorMessage.isEmpty() ? tr("unbekannter Fehler") : errorMessage));
        return;
    }

    m_previewStack->setCurrentWidget(m_preview);
}

void BaseEditDialog::applyTemplateSelection()
{
    if (m_initialState.editMode || !m_templateCombo)
        return;

    const QString templateBase = comboStoredValue(m_templateCombo);
    if (templateBase.isEmpty()) {
        m_roomStates = BaseEditService::defaultRoomsForArchetype(m_archetypeCombo ? m_archetypeCombo->currentText().trimmed()
                                                                                  : m_initialState.archetype);
        m_templateInfoLabel->clear();
    } else {
        BaseEditState templateState;
        QString errorMessage;
        if (!BaseEditService::loadTemplateState(templateBase,
                                               flatlas::core::EditingContext::instance().primaryGamePath(),
                                               m_textOverrides,
                                               &templateState,
                                               &errorMessage)) {
            m_templateInfoLabel->setText(errorMessage.trimmed().isEmpty()
                                             ? tr("Template konnte nicht geladen werden.")
                                             : errorMessage.trimmed());
            return;
        }

        BaseEditState targetState = state();
        targetState.archetype = m_archetypeCombo ? m_archetypeCombo->currentText().trimmed() : m_initialState.archetype;
        targetState.baseNickname = m_baseNicknameEdit ? m_baseNicknameEdit->text().trimmed() : m_initialState.baseNickname;
        targetState.reputation = comboStoredValue(m_reputationCombo);
        QVector<BaseRoomState> mergedRooms = BaseEditService::applyTemplateRoomsForCreate(targetState,
                                                                                          templateState,
                                                                                          m_copyNpcsCheck->isChecked());

        QStringList infoLines;
        for (const BaseRoomState &room : templateState.rooms) {
            const QString roomName = room.roomName.trimmed();
            if (roomName.isEmpty())
                continue;
            infoLines.append(QStringLiteral("%1: %2")
                                 .arg(roomName,
                                      room.scenePath.trimmed().isEmpty() ? QStringLiteral("-") : room.scenePath.trimmed()));
        }

        m_roomStates = mergedRooms;
        m_templateInfoLabel->setText(infoLines.isEmpty()
                                         ? tr("Template enthaelt keine Raeume.")
                                         : tr("Template-Raeume:\n%1").arg(infoLines.join(QLatin1Char('\n'))));
        m_priceVarianceSpin->setValue(templateState.priceVariance);
        if (!templateState.startRoom.trimmed().isEmpty())
            m_startRoomCombo->setCurrentText(templateState.startRoom.trimmed());
        if (!templateState.bgcsBaseRunBy.trimmed().isEmpty() && m_bgcsEdit->text().trimmed().isEmpty())
            m_bgcsEdit->setText(templateState.bgcsBaseRunBy.trimmed());
    }

    populateRooms(m_roomStates);
    refreshStartRooms();

    int preferredRow = -1;
    for (int row = 0; row < m_roomStates.size(); ++row) {
        if (m_roomStates.at(row).enabled) {
            preferredRow = row;
            break;
        }
    }
    if (preferredRow < 0 && !m_roomStates.isEmpty())
        preferredRow = 0;
    if (preferredRow >= 0) {
        m_roomTable->selectRow(preferredRow);
        populateNpcTable(preferredRow);
    } else {
        populateNpcTable(-1);
    }
}

void BaseEditDialog::applyArchetypeDefaults()
{
    if (m_initialState.editMode || !m_archetypeCombo)
        return;

    const QString archetype = m_archetypeCombo->currentText().trimmed();
    if (archetype.isEmpty())
        return;

    BaseArchetypeDefaults defaults = m_archetypeDefaultsCache.value(normalizedKey(archetype));
    if (defaults.archetype.isEmpty()) {
        defaults = BaseEditService::archetypeDefaults(archetype,
                                                      flatlas::core::EditingContext::instance().primaryGamePath(),
                                                      m_textOverrides);
        m_archetypeDefaultsCache.insert(normalizedKey(archetype), defaults);
    }

    const QString currentLoadout = m_loadoutCombo ? m_loadoutCombo->currentText().trimmed() : QString();
    if (m_loadoutCombo && (currentLoadout.isEmpty() || currentLoadout == m_lastSuggestedLoadout))
        m_loadoutCombo->setCurrentText(defaults.loadout.trimmed());
    m_lastSuggestedLoadout = defaults.loadout.trimmed();

    const QString suggestedInfo = defaults.infocardXml.trimmed();
    const QString currentInfo = m_infocardEdit ? m_infocardEdit->toPlainText().trimmed() : QString();
    if (m_infocardEdit && (currentInfo.isEmpty() || currentInfo == m_lastSuggestedIdsInfo))
        m_infocardEdit->setPlainText(suggestedInfo);
    m_lastSuggestedIdsInfo = suggestedInfo;
}

} // namespace flatlas::editors