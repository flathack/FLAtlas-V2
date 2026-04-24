#pragma once

#include "domain/SystemDocument.h"
#include "domain/UniverseData.h"

#include <QDialog>
#include <QVector3D>
#include <memory>

namespace flatlas::rendering { class MapScene; class SystemMapView; }

class QComboBox;
class QGraphicsLineItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QGraphicsEllipseItem;

namespace flatlas::editors {

class JumpConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit JumpConnectionDialog(QWidget *parent = nullptr);

    void setSourceSystem(const flatlas::domain::SystemInfo &system,
                         flatlas::domain::SystemDocument *document);
    void setSystems(const QVector<flatlas::domain::SystemInfo> &systems);
    void setJumpHoleArchetypes(const QStringList &archetypes);
    void setGateLoadouts(const QStringList &loadouts);
    void setFactions(const QStringList &factions);
    void setPilots(const QStringList &pilots);

    flatlas::domain::JumpConnection connection() const;
    bool isJumpGate() const;
    QString selectedArchetype() const;
    QString selectedLoadout() const;
    QString selectedReputation() const;
    QString selectedPilot() const;
    QString behavior() const;
    int difficultyLevel() const;
    QVector3D sourcePosition() const;
    QVector3D destinationPosition() const;

private:
    void setupUi();
    void refreshDestinationSystem();
    void refreshTypeUi();
    void refreshNicknameSuggestions();
    void refreshUniversePreview();
    void updateStatus();
    void placeMarker(flatlas::rendering::MapScene *scene,
                     QGraphicsEllipseItem *&marker,
                     const QVector3D &worldPos);
    QPair<QString, QString> nextInnerSystemAliasPair(const QString &systemNick) const;
    QString suggestedNickname(bool sourceSide) const;

    flatlas::domain::SystemInfo currentDestinationSystem() const;

    QVector<flatlas::domain::SystemInfo> m_systems;
    flatlas::domain::SystemInfo m_sourceSystem;
    flatlas::domain::SystemDocument *m_sourceDocument = nullptr;
    std::unique_ptr<flatlas::domain::SystemDocument> m_destinationDocument;
    QStringList m_jumpHoleArchetypes;
    QStringList m_gateLoadouts;

    QComboBox *m_typeCombo = nullptr;
    QLabel *m_sourceSystemLabel = nullptr;
    QComboBox *m_destinationSystemCombo = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QLineEdit *m_sourceObjectEdit = nullptr;
    QLineEdit *m_destinationObjectEdit = nullptr;
    QLineEdit *m_behaviorEdit = nullptr;
    QSpinBox *m_difficultySpin = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_factionCombo = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_okButton = nullptr;
    flatlas::rendering::MapScene *m_sourceScene = nullptr;
    flatlas::rendering::SystemMapView *m_sourceView = nullptr;
    flatlas::rendering::MapScene *m_destinationScene = nullptr;
    flatlas::rendering::SystemMapView *m_destinationView = nullptr;
    QGraphicsScene *m_universeScene = nullptr;
    QGraphicsView *m_universeView = nullptr;
    QGraphicsEllipseItem *m_sourceMarker = nullptr;
    QGraphicsEllipseItem *m_destinationMarker = nullptr;
    QGraphicsLineItem *m_universeConnectionLine = nullptr;
    QVector3D m_sourcePosition;
    QVector3D m_destinationPosition;
    QString m_lastSuggestedSourceNickname;
    QString m_lastSuggestedDestinationNickname;
};

} // namespace flatlas::editors
