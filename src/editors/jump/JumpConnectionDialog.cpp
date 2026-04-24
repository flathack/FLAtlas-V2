#include "JumpConnectionDialog.h"

#include "editors/system/SystemPersistence.h"
#include "rendering/view2d/MapScene.h"
#include "rendering/view2d/SystemMapView.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPen>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

namespace flatlas::editors {

namespace {

QString alphaSuffix(int index)
{
    int value = qMax(0, index);
    QString suffix;
    while (true) {
        const int remainder = value % 26;
        suffix.prepend(QChar(static_cast<char>('a' + remainder)));
        value /= 26;
        if (value == 0)
            break;
        --value;
    }
    return suffix;
}

QString systemLabel(const flatlas::domain::SystemInfo &system)
{
    const QString display = system.displayName.trimmed();
    return display.isEmpty()
        ? system.nickname
        : QStringLiteral("%1 (%2)").arg(display, system.nickname);
}

} // namespace

JumpConnectionDialog::JumpConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Jump-Verbindung erstellen"));
    setMinimumSize(1240, 760);
    setupUi();
}

void JumpConnectionDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);

    auto *topGroup = new QGroupBox(tr("Verbindung"));
    auto *topLayout = new QFormLayout(topGroup);

    m_typeCombo = new QComboBox(topGroup);
    m_typeCombo->addItem(tr("Jumpgate"), QStringLiteral("gate"));
    m_typeCombo->addItem(tr("Jump Hole"), QStringLiteral("hole"));
    topLayout->addRow(tr("Typ:"), m_typeCombo);

    m_sourceSystemLabel = new QLabel(topGroup);
    m_sourceSystemLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    topLayout->addRow(tr("Quelle:"), m_sourceSystemLabel);

    m_destinationSystemCombo = new QComboBox(topGroup);
    m_destinationSystemCombo->setEditable(false);
    topLayout->addRow(tr("Zielsystem:"), m_destinationSystemCombo);

    m_archetypeCombo = new QComboBox(topGroup);
    topLayout->addRow(tr("Archetype:"), m_archetypeCombo);

    m_sourceObjectEdit = new QLineEdit(topGroup);
    topLayout->addRow(tr("Quellobjekt:"), m_sourceObjectEdit);

    m_destinationObjectEdit = new QLineEdit(topGroup);
    topLayout->addRow(tr("Zielobjekt:"), m_destinationObjectEdit);

    root->addWidget(topGroup);

    auto *gateGroup = new QGroupBox(tr("Gate-Parameter"));
    auto *gateLayout = new QFormLayout(gateGroup);
    m_behaviorEdit = new QLineEdit(QStringLiteral("NOTHING"), gateGroup);
    gateLayout->addRow(tr("Behavior:"), m_behaviorEdit);

    m_difficultySpin = new QSpinBox(gateGroup);
    m_difficultySpin->setRange(0, 10);
    m_difficultySpin->setValue(1);
    gateLayout->addRow(tr("Difficulty:"), m_difficultySpin);

    m_loadoutCombo = new QComboBox(gateGroup);
    gateLayout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_factionCombo = new QComboBox(gateGroup);
    m_factionCombo->setEditable(false);
    gateLayout->addRow(tr("Reputation:"), m_factionCombo);

    m_pilotCombo = new QComboBox(gateGroup);
    gateLayout->addRow(tr("Pilot:"), m_pilotCombo);

    root->addWidget(gateGroup);

    auto *mapsLayout = new QGridLayout();
    auto *sourceLabel = new QLabel(tr("Quelle platzieren"));
    sourceLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    mapsLayout->addWidget(sourceLabel, 0, 0);
    auto *destLabel = new QLabel(tr("Ziel platzieren"));
    destLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    mapsLayout->addWidget(destLabel, 0, 1);
    auto *universeLabel = new QLabel(tr("Universe"));
    universeLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    mapsLayout->addWidget(universeLabel, 0, 2);

    m_sourceScene = new flatlas::rendering::MapScene(this);
    m_sourceView = new flatlas::rendering::SystemMapView(this);
    m_sourceView->setMapScene(m_sourceScene);
    mapsLayout->addWidget(m_sourceView, 1, 0);

    m_destinationScene = new flatlas::rendering::MapScene(this);
    m_destinationView = new flatlas::rendering::SystemMapView(this);
    m_destinationView->setMapScene(m_destinationScene);
    mapsLayout->addWidget(m_destinationView, 1, 1);

    m_universeScene = new QGraphicsScene(this);
    m_universeView = new QGraphicsView(this);
    m_universeView->setScene(m_universeScene);
    m_universeView->setRenderHint(QPainter::Antialiasing);
    mapsLayout->addWidget(m_universeView, 1, 2);
    root->addLayout(mapsLayout, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#ef4444;"));
    root->addWidget(m_statusLabel);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setText(tr("Erstellen"));
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        updateStatus();
        if (m_statusLabel->text().isEmpty())
            accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttonBox);

    connect(m_typeCombo, &QComboBox::currentTextChanged, this, [this]() {
        refreshTypeUi();
        refreshNicknameSuggestions();
        updateStatus();
    });
    connect(m_destinationSystemCombo, &QComboBox::currentTextChanged, this, [this]() {
        refreshDestinationSystem();
        refreshNicknameSuggestions();
        updateStatus();
    });
    connect(m_sourceObjectEdit, &QLineEdit::textChanged, this, &JumpConnectionDialog::updateStatus);
    connect(m_destinationObjectEdit, &QLineEdit::textChanged, this, &JumpConnectionDialog::updateStatus);
    connect(m_behaviorEdit, &QLineEdit::textChanged, this, &JumpConnectionDialog::updateStatus);
    connect(m_loadoutCombo, &QComboBox::currentTextChanged, this, &JumpConnectionDialog::updateStatus);
    connect(m_factionCombo, &QComboBox::currentTextChanged, this, &JumpConnectionDialog::updateStatus);
    connect(m_pilotCombo, &QComboBox::currentTextChanged, this, &JumpConnectionDialog::updateStatus);

    connect(m_sourceView, &flatlas::rendering::SystemMapView::placementClicked, this, [this](const QPointF &scenePos) {
        const QPointF world = flatlas::rendering::MapScene::qtToFl(scenePos.x(), scenePos.y());
        m_sourcePosition = QVector3D(static_cast<float>(world.x()), 0.0f, static_cast<float>(world.y()));
        placeMarker(m_sourceScene, m_sourceMarker, m_sourcePosition);
        updateStatus();
    });
    connect(m_destinationView, &flatlas::rendering::SystemMapView::placementClicked, this, [this](const QPointF &scenePos) {
        const QPointF world = flatlas::rendering::MapScene::qtToFl(scenePos.x(), scenePos.y());
        m_destinationPosition = QVector3D(static_cast<float>(world.x()), 0.0f, static_cast<float>(world.y()));
        placeMarker(m_destinationScene, m_destinationMarker, m_destinationPosition);
        updateStatus();
    });

    m_sourceView->setPlacementMode(true, tr("Klicke auf die Quellkarte, um die Jump-Verbindung zu platzieren."));
    m_destinationView->setPlacementMode(true, tr("Klicke auf die Zielkarte, um das Gegenobjekt zu platzieren."));
    refreshTypeUi();
    updateStatus();
}

void JumpConnectionDialog::setSourceSystem(const flatlas::domain::SystemInfo &system,
                                           flatlas::domain::SystemDocument *document)
{
    m_sourceSystem = system;
    m_sourceDocument = document;
    m_sourceSystemLabel->setText(systemLabel(system));
    if (m_sourceScene && document) {
        m_sourceScene->loadDocument(document);
        m_sourceView->scheduleInitialFit();
    }
    refreshUniversePreview();
    refreshNicknameSuggestions();
    updateStatus();
}

void JumpConnectionDialog::setSystems(const QVector<flatlas::domain::SystemInfo> &systems)
{
    m_systems = systems;
    m_destinationSystemCombo->clear();
    for (const auto &system : systems)
        m_destinationSystemCombo->addItem(systemLabel(system), system.nickname);

    const int sourceIndex = m_destinationSystemCombo->findData(m_sourceSystem.nickname);
    if (sourceIndex >= 0)
        m_destinationSystemCombo->setCurrentIndex(sourceIndex);
    refreshUniversePreview();
    refreshDestinationSystem();
}

void JumpConnectionDialog::setJumpHoleArchetypes(const QStringList &archetypes)
{
    m_jumpHoleArchetypes = archetypes;
    refreshTypeUi();
}

void JumpConnectionDialog::setGateLoadouts(const QStringList &loadouts)
{
    m_gateLoadouts = loadouts;
    refreshTypeUi();
}

void JumpConnectionDialog::setFactions(const QStringList &factions)
{
    m_factionCombo->clear();
    m_factionCombo->addItems(factions);
    updateStatus();
}

void JumpConnectionDialog::setPilots(const QStringList &pilots)
{
    m_pilotCombo->clear();
    m_pilotCombo->addItems(pilots);
    const int defaultIndex = m_pilotCombo->findText(QStringLiteral("pilot_solar_hardest"), Qt::MatchFixedString);
    m_pilotCombo->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);
    updateStatus();
}

flatlas::domain::JumpConnection JumpConnectionDialog::connection() const
{
    flatlas::domain::JumpConnection value;
    value.fromSystem = m_sourceSystem.nickname;
    value.fromObject = m_sourceObjectEdit->text().trimmed();
    value.toSystem = currentDestinationSystem().nickname;
    value.toObject = m_destinationObjectEdit->text().trimmed();
    value.kind = isJumpGate() ? QStringLiteral("gate") : QStringLiteral("hole");
    return value;
}

bool JumpConnectionDialog::isJumpGate() const
{
    return m_typeCombo->currentData().toString() == QStringLiteral("gate");
}

QString JumpConnectionDialog::selectedArchetype() const
{
    return m_archetypeCombo->currentText().trimmed();
}

QString JumpConnectionDialog::selectedLoadout() const
{
    return m_loadoutCombo->currentText().trimmed();
}

QString JumpConnectionDialog::selectedReputation() const
{
    return m_factionCombo->currentText().trimmed();
}

QString JumpConnectionDialog::selectedPilot() const
{
    return m_pilotCombo->currentText().trimmed();
}

QString JumpConnectionDialog::behavior() const
{
    return m_behaviorEdit->text().trimmed();
}

int JumpConnectionDialog::difficultyLevel() const
{
    return m_difficultySpin->value();
}

QVector3D JumpConnectionDialog::sourcePosition() const
{
    return m_sourcePosition;
}

QVector3D JumpConnectionDialog::destinationPosition() const
{
    return m_destinationPosition;
}

void JumpConnectionDialog::refreshDestinationSystem()
{
    m_destinationPosition = QVector3D();
    if (m_destinationMarker && m_destinationScene) {
        m_destinationScene->removeItem(m_destinationMarker);
        delete m_destinationMarker;
        m_destinationMarker = nullptr;
    }

    const auto system = currentDestinationSystem();
    if (system.filePath.trimmed().isEmpty()) {
        m_destinationDocument.reset();
        m_destinationScene->clear();
        updateStatus();
        return;
    }

    if (QFileInfo(system.filePath) == QFileInfo(m_sourceSystem.filePath)) {
        m_destinationDocument.reset();
        if (m_sourceDocument) {
            m_destinationScene->loadDocument(m_sourceDocument);
            m_destinationView->scheduleInitialFit();
        }
    } else {
        m_destinationDocument = flatlas::editors::SystemPersistence::load(system.filePath);
        if (m_destinationDocument) {
            m_destinationScene->loadDocument(m_destinationDocument.get());
            m_destinationView->scheduleInitialFit();
        } else {
            m_destinationScene->clear();
        }
    }

    m_destinationView->setPlacementMode(true, tr("Klicke auf die Zielkarte, um das Gegenobjekt zu platzieren."));
    refreshUniversePreview();
    updateStatus();
}

void JumpConnectionDialog::refreshTypeUi()
{
    m_archetypeCombo->clear();
    m_loadoutCombo->clear();

    if (isJumpGate()) {
        m_archetypeCombo->addItem(QStringLiteral("jumpgate"));
        m_archetypeCombo->setEnabled(false);
        m_loadoutCombo->addItems(m_gateLoadouts);
    } else {
        QStringList holeArchetypes = m_jumpHoleArchetypes;
        if (holeArchetypes.isEmpty())
            holeArchetypes.append(QStringLiteral("jumphole"));
        m_archetypeCombo->addItems(holeArchetypes);
        m_archetypeCombo->setEnabled(true);
    }

    m_behaviorEdit->parentWidget()->setVisible(isJumpGate());
    if (QGroupBox *gateGroup = qobject_cast<QGroupBox *>(m_behaviorEdit->parentWidget()->parentWidget()))
        gateGroup->setVisible(isJumpGate());
}

QPair<QString, QString> JumpConnectionDialog::nextInnerSystemAliasPair(const QString &systemNick) const
{
    const QString prefix = systemNick.trimmed().toUpper();
    QSet<QString> usedAliases;
    const QRegularExpression pattern(
        QStringLiteral("^%1([a-z]+)_to_%1([a-z]+)(?:_hole)?$")
            .arg(QRegularExpression::escape(prefix)),
        QRegularExpression::CaseInsensitiveOption);

    auto collectAliases = [&](flatlas::domain::SystemDocument *doc) {
        if (!doc)
            return;
        for (const auto &obj : doc->objects()) {
            if (!obj)
                continue;
            const auto match = pattern.match(obj->nickname().trimmed());
            if (!match.hasMatch())
                continue;
            usedAliases.insert(match.captured(1).toLower());
            usedAliases.insert(match.captured(2).toLower());
        }
    };
    collectAliases(m_sourceDocument);
    collectAliases(m_destinationDocument.get());

    for (int pairIndex = 0; pairIndex < 2048; ++pairIndex) {
        const QString firstSuffix = alphaSuffix(pairIndex * 2);
        const QString secondSuffix = alphaSuffix(pairIndex * 2 + 1);
        if (!usedAliases.contains(firstSuffix) && !usedAliases.contains(secondSuffix))
            return {QStringLiteral("%1%2").arg(prefix, firstSuffix), QStringLiteral("%1%2").arg(prefix, secondSuffix)};
    }

    return {prefix + QStringLiteral("a"), prefix + QStringLiteral("b")};
}

QString JumpConnectionDialog::suggestedNickname(bool sourceSide) const
{
    const auto destSystem = currentDestinationSystem();
    const bool hole = !isJumpGate();
    const QString holeSuffix = hole ? QStringLiteral("_hole") : QString();
    if (destSystem.nickname.compare(m_sourceSystem.nickname, Qt::CaseInsensitive) == 0) {
        const auto aliases = nextInnerSystemAliasPair(m_sourceSystem.nickname);
        return sourceSide
            ? QStringLiteral("%1_to_%2%3").arg(aliases.first, aliases.second, holeSuffix)
            : QStringLiteral("%1_to_%2%3").arg(aliases.second, aliases.first, holeSuffix);
    }
    return sourceSide
        ? QStringLiteral("%1_to_%2%3").arg(m_sourceSystem.nickname, destSystem.nickname, holeSuffix)
        : QStringLiteral("%1_to_%2%3").arg(destSystem.nickname, m_sourceSystem.nickname, holeSuffix);
}

void JumpConnectionDialog::refreshNicknameSuggestions()
{
    const QString newSourceSuggestion = suggestedNickname(true);
    const QString newDestSuggestion = suggestedNickname(false);

    if (m_sourceObjectEdit->text().trimmed().isEmpty()
        || m_sourceObjectEdit->text().trimmed() == m_lastSuggestedSourceNickname) {
        m_sourceObjectEdit->setText(newSourceSuggestion);
    }
    if (m_destinationObjectEdit->text().trimmed().isEmpty()
        || m_destinationObjectEdit->text().trimmed() == m_lastSuggestedDestinationNickname) {
        m_destinationObjectEdit->setText(newDestSuggestion);
    }

    m_lastSuggestedSourceNickname = newSourceSuggestion;
    m_lastSuggestedDestinationNickname = newDestSuggestion;
}

void JumpConnectionDialog::refreshUniversePreview()
{
    if (!m_universeScene)
        return;

    m_universeScene->clear();
    m_universeConnectionLine = nullptr;

    if (m_systems.isEmpty() || m_sourceSystem.nickname.trimmed().isEmpty())
        return;

    QHash<QString, QPointF> scenePositions;
    QRectF bounds;
    bool firstPoint = true;
    for (const auto &system : m_systems) {
        const QPointF pos(system.position.x() * 0.03, -system.position.z() * 0.03);
        scenePositions.insert(system.nickname.toUpper(), pos);
        if (firstPoint) {
            bounds = QRectF(pos, QSizeF(1.0, 1.0));
            firstPoint = false;
        } else {
            bounds = bounds.united(QRectF(pos, QSizeF(1.0, 1.0)));
        }

        const bool isSource = system.nickname.compare(m_sourceSystem.nickname, Qt::CaseInsensitive) == 0;
        const bool isTarget = system.nickname.compare(currentDestinationSystem().nickname, Qt::CaseInsensitive) == 0;
        const QColor color = isSource ? QColor(59, 130, 246) : (isTarget ? QColor(245, 158, 11) : QColor(120, 130, 150));
        auto *dot = m_universeScene->addEllipse(pos.x() - 4.0, pos.y() - 4.0, 8.0, 8.0,
                                                QPen(color.darker(130), 1.0), QBrush(color));
        dot->setZValue(2.0);

        const QString labelText = system.displayName.trimmed().isEmpty() ? system.nickname : system.displayName;
        auto *label = m_universeScene->addSimpleText(QStringLiteral("%1 (%2)").arg(labelText, system.nickname));
        label->setBrush(QBrush(isSource || isTarget ? QColor(220, 225, 235) : QColor(160, 170, 185)));
        label->setPos(pos + QPointF(8.0, -10.0));
        label->setZValue(2.0);
    }

    const QPointF sourcePos = scenePositions.value(m_sourceSystem.nickname.toUpper());
    const QPointF targetPos = scenePositions.value(currentDestinationSystem().nickname.toUpper());
    if (!sourcePos.isNull() || !targetPos.isNull()) {
        m_universeConnectionLine = m_universeScene->addLine(QLineF(sourcePos, targetPos),
                                                            QPen(QColor(255, 214, 10), 2.0));
        if (m_universeConnectionLine)
            m_universeConnectionLine->setZValue(1.0);
    }

    if (!bounds.isNull()) {
        bounds.adjust(-60.0, -40.0, 60.0, 40.0);
        m_universeScene->setSceneRect(bounds);
        if (m_universeView)
            m_universeView->fitInView(bounds, Qt::KeepAspectRatio);
    }
}

flatlas::domain::SystemInfo JumpConnectionDialog::currentDestinationSystem() const
{
    const QString nickname = m_destinationSystemCombo->currentData().toString();
    for (const auto &system : m_systems) {
        if (system.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            return system;
    }
    return {};
}

void JumpConnectionDialog::updateStatus()
{
    QStringList issues;
    const auto conn = connection();
    const auto destSystem = currentDestinationSystem();

    if (m_sourceSystem.nickname.trimmed().isEmpty())
        issues.append(tr("Quellsystem fehlt."));
    if (destSystem.nickname.trimmed().isEmpty() || destSystem.filePath.trimmed().isEmpty())
        issues.append(tr("Zielsystem ist ungueltig oder konnte nicht aufgeloest werden."));
    if (conn.fromObject.trimmed().isEmpty() || conn.toObject.trimmed().isEmpty())
        issues.append(tr("Beide Objekt-Nicknames muessen gesetzt sein."));
    if (conn.fromObject.compare(conn.toObject, Qt::CaseInsensitive) == 0
        && conn.fromSystem.compare(conn.toSystem, Qt::CaseInsensitive) == 0) {
        issues.append(tr("Quell- und Zielobjekt duerfen nicht identisch sein."));
    }
    if (m_sourcePosition.isNull())
        issues.append(tr("Quellposition fehlt - bitte auf der linken Karte klicken."));
    if (m_destinationPosition.isNull())
        issues.append(tr("Zielposition fehlt - bitte auf der rechten Karte klicken."));
    if (isJumpGate()) {
        if (m_loadoutCombo->currentText().trimmed().isEmpty())
            issues.append(tr("Jumpgates brauchen ein gueltiges Loadout."));
        if (m_factionCombo->currentText().trimmed().isEmpty())
            issues.append(tr("Jumpgates brauchen eine Reputation."));
        if (m_pilotCombo->currentText().trimmed().isEmpty())
            issues.append(tr("Jumpgates brauchen einen Pilot-Eintrag."));
    }
    if (!isJumpGate() && m_archetypeCombo->currentText().trimmed().isEmpty())
        issues.append(tr("Bitte einen Jump-Hole-Archetype waehlen."));

    m_statusLabel->setText(issues.join(QLatin1Char('\n')));
    if (m_okButton)
        m_okButton->setEnabled(issues.isEmpty());
}

void JumpConnectionDialog::placeMarker(flatlas::rendering::MapScene *scene,
                                       QGraphicsEllipseItem *&marker,
                                       const QVector3D &worldPos)
{
    if (!scene)
        return;
    if (marker) {
        scene->removeItem(marker);
        delete marker;
        marker = nullptr;
    }

    const QPointF scenePos = flatlas::rendering::MapScene::flToQt(worldPos.x(), worldPos.z());
    marker = scene->addEllipse(scenePos.x() - 5.0,
                               scenePos.y() - 5.0,
                               10.0,
                               10.0,
                               QPen(QColor(255, 214, 10), 1.5),
                               QBrush(QColor(255, 214, 10, 180)));
    marker->setZValue(5000.0);
}

} // namespace flatlas::editors
