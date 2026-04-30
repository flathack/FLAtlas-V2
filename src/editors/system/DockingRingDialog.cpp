#include "editors/system/DockingRingDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QStringList buildRoomState(const QStringList &roomNames,
                           const QString &preferredStartRoom,
                           const QString &currentStartRoom,
                           QString *outStartRoom)
{
    QStringList rooms;
    for (const QString &roomName : roomNames) {
        const QString trimmed = roomName.trimmed();
        if (!trimmed.isEmpty() && !rooms.contains(trimmed, Qt::CaseInsensitive))
            rooms.append(trimmed);
    }

    QString startRoom;
    if (!preferredStartRoom.trimmed().isEmpty() && rooms.contains(preferredStartRoom.trimmed(), Qt::CaseInsensitive)) {
        startRoom = preferredStartRoom.trimmed();
    } else if (!currentStartRoom.trimmed().isEmpty() && rooms.contains(currentStartRoom.trimmed(), Qt::CaseInsensitive)) {
        startRoom = currentStartRoom.trimmed();
    } else if (rooms.contains(QStringLiteral("Deck"), Qt::CaseInsensitive)) {
        startRoom = QStringLiteral("Deck");
    } else if (!rooms.isEmpty()) {
        startRoom = rooms.first();
    }

    if (outStartRoom)
        *outStartRoom = startRoom;
    return rooms;
}

QStringList defaultRingLoadouts()
{
    return {
        QStringLiteral("docking_ring"),
        QStringLiteral("docking_ring_li_01"),
        QStringLiteral("docking_ring_br_01"),
        QStringLiteral("docking_ring_ku_01"),
        QStringLiteral("docking_ring_rh_01"),
        QStringLiteral("docking_ring_co_01"),
        QStringLiteral("docking_ring_co_02"),
        QStringLiteral("docking_ring_co_03"),
        QStringLiteral("docking_ring_pi_01"),
    };
}

QStringList defaultVoices()
{
    return {
        QStringLiteral("atc_leg_m01"),
        QStringLiteral("atc_leg_f01"),
        QStringLiteral("atc_leg_f01a"),
        QStringLiteral("mc_leg_m01"),
    };
}

QStringList defaultPilots()
{
    return {
        QStringLiteral("pilot_solar_easiest"),
        QStringLiteral("pilot_solar_easy"),
        QStringLiteral("pilot_solar_hard"),
        QStringLiteral("pilot_solar_hardest"),
    };
}

const QVector<QPair<QString, bool>> kRoomChoices = {
    {QStringLiteral("Deck"), true},
    {QStringLiteral("Bar"), true},
    {QStringLiteral("Trader"), true},
    {QStringLiteral("Equipment"), false},
    {QStringLiteral("ShipDealer"), false},
};

const QStringList kTemplateStartRoomPreference = {
    QStringLiteral("Cityscape"),
    QStringLiteral("Planetscape"),
    QStringLiteral("Deck"),
};

} // namespace

namespace flatlas::editors {

DockingRingDialog::DockingRingDialog(QWidget *parent,
                                     const QString &planetNickname,
                                     const QString &baseNickname,
                                     const QStringList &loadouts,
                                     const QStringList &factions,
                                     const QVector<QPair<QString, QString>> &existingBases,
                                     const QStringList &pilots,
                                     const QStringList &voices,
                                     const QHash<QString, QStringList> &templateRoomsByBase,
                                     bool needsBase,
                                     const QString &defaultFactionDisplay,
                                     const QString &idsNameText,
                                     const QString &idsInfoValue,
                                     double initialDistanceToPlanetCore,
                                     int stridNameValue,
                                     bool initialCreateFixture,
                                     const QString &windowTitle,
                                     const DockingRingCreateRequest *initialRequest)
    : QDialog(parent)
    , m_needsBase(needsBase)
    , m_templateRoomsByBase(templateRoomsByBase)
{
    setWindowTitle(windowTitle.trimmed().isEmpty() ? QStringLiteral("Create Docking Ring") : windowTitle.trimmed());
    setMinimumWidth(540);
    buildUi(planetNickname,
            baseNickname,
            loadouts,
            factions,
            existingBases,
            pilots,
            voices,
            defaultFactionDisplay,
            idsNameText,
            idsInfoValue,
            initialDistanceToPlanetCore,
            stridNameValue);
    if (m_createFixtureCheck)
        m_createFixtureCheck->setChecked(initialCreateFixture);
    if (initialRequest) {
        m_nicknameEdit->setText(initialRequest->nickname);
        m_archetypeCombo->setCurrentText(initialRequest->archetype);
        m_loadoutCombo->setCurrentText(initialRequest->loadout);
        m_factionCombo->setCurrentText(initialRequest->factionDisplay);
        m_voiceCombo->setCurrentText(initialRequest->voice);
        m_costumeEdit->setText(initialRequest->costume);
        m_pilotCombo->setCurrentText(initialRequest->pilot);
        m_difficultySpin->setValue(initialRequest->difficulty);
        m_idsNameEdit->setText(initialRequest->idsNameText);
        m_idsInfoEdit->setText(initialRequest->idsInfoValue);
        if (m_distanceSpin)
            m_distanceSpin->setValue(std::max(0.0, initialRequest->distanceToPlanetCore));
        if (m_createFixtureCheck)
            m_createFixtureCheck->setChecked(initialRequest->createFixture);
    }
}

DockingRingCreateRequest DockingRingDialog::result() const
{
    DockingRingCreateRequest request;
    request.nickname = m_nicknameEdit->text().trimmed();
    request.archetype = m_archetypeCombo->currentText().trimmed();
    request.loadout = m_loadoutCombo->currentText().trimmed();
    request.factionDisplay = m_factionCombo->currentText().trimmed();
    request.voice = m_voiceCombo->currentText().trimmed();
    request.costume = m_costumeEdit->text().trimmed();
    request.pilot = m_pilotCombo->currentText().trimmed();
    request.difficulty = m_difficultySpin->value();
    request.idsNameText = m_idsNameEdit->text().trimmed();
    request.idsInfoValue = m_idsInfoEdit->text().trimmed();
    request.distanceToPlanetCore = m_distanceSpin ? m_distanceSpin->value() : 0.0;
    request.createFixture = m_createFixtureCheck && m_createFixtureCheck->isChecked();
    request.needsBase = m_needsBase;

    if (m_needsBase) {
        request.baseNickname = m_baseNicknameEdit->text().trimmed();
        request.stridName = m_stridNameSpin->value();
        request.startRoom = m_startRoomCombo->currentText().trimmed();
        request.priceVariance = m_priceVarianceSpin->value();
        request.templateBase = m_templateBaseCombo->currentData().toString().trimmed();
        for (auto it = m_roomChecks.cbegin(); it != m_roomChecks.cend(); ++it) {
            if (it.value() && it.value()->isChecked())
                request.roomNames.append(it.key());
        }
    } else {
        request.baseNickname = m_baseNicknameEdit ? m_baseNicknameEdit->text().trimmed() : QString();
    }

    return request;
}

void DockingRingDialog::buildUi(const QString &planetNickname,
                                const QString &baseNickname,
                                const QStringList &loadouts,
                                const QStringList &factions,
                                const QVector<QPair<QString, QString>> &existingBases,
                                const QStringList &pilots,
                                const QStringList &voices,
                                const QString &defaultFactionDisplay,
                                const QString &idsNameText,
                                const QString &idsInfoValue,
                                double initialDistanceToPlanetCore,
                                int stridNameValue)
{
    auto *outerLayout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    outerLayout->addWidget(scroll);

    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    scroll->setWidget(content);

    auto *ringGroup = new QGroupBox(QStringLiteral("Docking Ring"), content);
    auto *ringLayout = new QFormLayout(ringGroup);
    m_nicknameEdit = new QLineEdit(QStringLiteral("Dock_Ring_%1").arg(planetNickname), ringGroup);
    ringLayout->addRow(QStringLiteral("Nickname:"), m_nicknameEdit);

    m_archetypeCombo = new QComboBox(ringGroup);
    m_archetypeCombo->setEditable(true);
    m_archetypeCombo->addItems({QStringLiteral("dock_ring"), QStringLiteral("destructable_dock_ring")});
    ringLayout->addRow(QStringLiteral("Archetype:"), m_archetypeCombo);

    QStringList ringLoadouts;
    for (const QString &loadout : loadouts) {
        if (loadout.contains(QStringLiteral("docking_ring"), Qt::CaseInsensitive))
            ringLoadouts.append(loadout);
    }
    if (ringLoadouts.isEmpty())
        ringLoadouts = defaultRingLoadouts();
    ringLoadouts.removeDuplicates();

    m_loadoutCombo = new QComboBox(ringGroup);
    m_loadoutCombo->setEditable(true);
    m_loadoutCombo->addItems(ringLoadouts);
    ringLayout->addRow(QStringLiteral("Loadout:"), m_loadoutCombo);

    m_factionCombo = new QComboBox(ringGroup);
    m_factionCombo->setEditable(true);
    m_factionCombo->addItems(factions);
    m_factionCombo->setCurrentText(defaultFactionDisplay);
    ringLayout->addRow(QStringLiteral("Reputation:"), m_factionCombo);

    QStringList mergedVoices = defaultVoices();
    for (const QString &voice : voices) {
        if (!mergedVoices.contains(voice, Qt::CaseInsensitive))
            mergedVoices.append(voice);
    }
    m_voiceCombo = new QComboBox(ringGroup);
    m_voiceCombo->setEditable(true);
    m_voiceCombo->addItems(mergedVoices);
    m_voiceCombo->setCurrentText(QStringLiteral("atc_leg_m01"));
    ringLayout->addRow(QStringLiteral("Voice:"), m_voiceCombo);

    m_costumeEdit = new QLineEdit(QStringLiteral("robot_body_A"), ringGroup);
    ringLayout->addRow(QStringLiteral("Space Costume:"), m_costumeEdit);

    QStringList mergedPilots = defaultPilots();
    for (const QString &pilot : pilots) {
        if (!mergedPilots.contains(pilot, Qt::CaseInsensitive))
            mergedPilots.append(pilot);
    }
    m_pilotCombo = new QComboBox(ringGroup);
    m_pilotCombo->setEditable(true);
    m_pilotCombo->addItems(mergedPilots);
    m_pilotCombo->setCurrentText(QStringLiteral("pilot_solar_easiest"));
    ringLayout->addRow(QStringLiteral("Pilot:"), m_pilotCombo);

    m_difficultySpin = new QSpinBox(ringGroup);
    m_difficultySpin->setRange(1, 50);
    m_difficultySpin->setValue(1);
    ringLayout->addRow(QStringLiteral("Difficulty Level:"), m_difficultySpin);

    m_idsNameEdit = new QLineEdit(idsNameText, ringGroup);
    ringLayout->addRow(QStringLiteral("Name:"), m_idsNameEdit);

    m_idsInfoEdit = new QLineEdit(idsInfoValue, ringGroup);
    ringLayout->addRow(QStringLiteral("ids_info:"), m_idsInfoEdit);

    m_distanceSpin = new QDoubleSpinBox(ringGroup);
    m_distanceSpin->setRange(1.0, 1000000.0);
    m_distanceSpin->setDecimals(1);
    m_distanceSpin->setSingleStep(10.0);
    m_distanceSpin->setValue(std::max(1.0, initialDistanceToPlanetCore));
    ringLayout->addRow(QStringLiteral("Distance to Planet Core:"), m_distanceSpin);

    m_createFixtureCheck = new QCheckBox(QStringLiteral("Create docking_fixture"), ringGroup);
    m_createFixtureCheck->setToolTip(QStringLiteral("Creates or keeps a docking_fixture above the docking ring with ids_name=261166 and ids_info=66489."));
    ringLayout->addRow(QString(), m_createFixtureCheck);
    contentLayout->addWidget(ringGroup);

    if (m_needsBase) {
        auto *baseGroup = new QGroupBox(QStringLiteral("Base"), content);
        auto *baseLayout = new QFormLayout(baseGroup);
        m_baseNicknameEdit = new QLineEdit(baseNickname, baseGroup);
        baseLayout->addRow(QStringLiteral("Base Nickname:"), m_baseNicknameEdit);
        m_stridNameSpin = new QSpinBox(baseGroup);
        m_stridNameSpin->setRange(0, 999999);
        m_stridNameSpin->setValue(stridNameValue);
        baseLayout->addRow(QStringLiteral("strid_name:"), m_stridNameSpin);
        contentLayout->addWidget(baseGroup);

        auto *roomsGroup = new QGroupBox(QStringLiteral("Rooms"), content);
        auto *roomsLayout = new QVBoxLayout(roomsGroup);
        m_roomsLayout = roomsLayout;
        for (const auto &roomChoice : kRoomChoices)
            ensureRoomCheckbox(roomChoice.first, roomChoice.second);

        auto *startRoomRow = new QHBoxLayout();
        startRoomRow->addWidget(new QLabel(QStringLiteral("Start Room"), roomsGroup));
        m_startRoomCombo = new QComboBox(roomsGroup);
        startRoomRow->addWidget(m_startRoomCombo);
        roomsLayout->addLayout(startRoomRow);

        auto *priceRow = new QHBoxLayout();
        priceRow->addWidget(new QLabel(QStringLiteral("Price Variance"), roomsGroup));
        m_priceVarianceSpin = new QDoubleSpinBox(roomsGroup);
        m_priceVarianceSpin->setRange(0.0, 1.0);
        m_priceVarianceSpin->setSingleStep(0.05);
        m_priceVarianceSpin->setDecimals(2);
        m_priceVarianceSpin->setValue(0.15);
        priceRow->addWidget(m_priceVarianceSpin);
        roomsLayout->addLayout(priceRow);
        refreshStartRoomChoices(QStringLiteral("Deck"));
        contentLayout->addWidget(roomsGroup);

        auto *templateGroup = new QGroupBox(QStringLiteral("Room Template"), content);
        auto *templateLayout = new QFormLayout(templateGroup);
        m_templateBaseCombo = new QComboBox(templateGroup);
        m_templateBaseCombo->setEditable(true);
        m_templateBaseCombo->addItem(QString(), QString());
        for (const auto &existingBase : existingBases)
            m_templateBaseCombo->addItem(existingBase.first, existingBase.second);
        templateLayout->addRow(QStringLiteral("Copy Rooms From:"), m_templateBaseCombo);
        connect(m_templateBaseCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            applyTemplateRooms();
        });
        contentLayout->addWidget(templateGroup);
    } else {
        m_baseNicknameEdit = new QLineEdit(baseNickname, content);
        m_baseNicknameEdit->hide();
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, content);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    contentLayout->addWidget(buttonBox);
    contentLayout->addStretch(1);
}

void DockingRingDialog::ensureRoomCheckbox(const QString &roomName, bool checkedByDefault)
{
    if (m_roomChecks.contains(roomName))
        return;

    auto *checkBox = new QCheckBox(roomName, this);
    checkBox->setChecked(checkedByDefault);
    connect(checkBox, &QCheckBox::toggled, this, [this](bool /*checked*/) {
        refreshStartRoomChoices();
    });
    m_roomsLayout->addWidget(checkBox);
    m_roomChecks.insert(roomName, checkBox);
}

void DockingRingDialog::refreshStartRoomChoices(const QString &preferredRoom)
{
    if (!m_needsBase || !m_startRoomCombo)
        return;

    QStringList roomNames;
    for (auto it = m_roomChecks.cbegin(); it != m_roomChecks.cend(); ++it) {
        if (it.value() && it.value()->isChecked())
            roomNames.append(it.key());
    }

    QString startRoom;
    const QStringList rooms = buildRoomState(roomNames, preferredRoom, m_startRoomCombo->currentText(), &startRoom);
    m_startRoomCombo->blockSignals(true);
    m_startRoomCombo->clear();
    m_startRoomCombo->addItems(rooms);
    if (!startRoom.isEmpty())
        m_startRoomCombo->setCurrentText(startRoom);
    m_startRoomCombo->blockSignals(false);
}

void DockingRingDialog::applyTemplateRooms()
{
    if (!m_needsBase || !m_templateBaseCombo)
        return;

    const QString templateBase = m_templateBaseCombo->currentData().toString().trimmed().isEmpty()
        ? m_templateBaseCombo->currentText().trimmed()
        : m_templateBaseCombo->currentData().toString().trimmed();
    if (templateBase.isEmpty() || !m_templateRoomsByBase.contains(templateBase))
        return;

    const QStringList roomNames = m_templateRoomsByBase.value(templateBase);
    if (roomNames.isEmpty())
        return;

    QSet<QString> selectedRooms;
    for (const QString &roomName : roomNames)
        selectedRooms.insert(roomName);
    for (auto it = m_roomChecks.begin(); it != m_roomChecks.end(); ++it)
        it.value()->setChecked(selectedRooms.contains(it.key()));
    for (const QString &roomName : roomNames)
        ensureRoomCheckbox(roomName, true);

    QString preferredRoom;
    for (const QString &candidate : kTemplateStartRoomPreference) {
        if (roomNames.contains(candidate, Qt::CaseInsensitive)) {
            preferredRoom = candidate;
            break;
        }
    }
    if (preferredRoom.isEmpty())
        preferredRoom = roomNames.first();
    refreshStartRoomChoices(preferredRoom);
}

} // namespace flatlas::editors