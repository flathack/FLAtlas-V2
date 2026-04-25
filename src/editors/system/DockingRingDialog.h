#pragma once

#include <QDialog>
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QVBoxLayout;

namespace flatlas::editors {

struct DockingRingCreateRequest {
    QString nickname;
    QString archetype;
    QString loadout;
    QString factionDisplay;
    QString voice;
    QString costume;
    QString pilot;
    int difficulty = 1;
    QString idsNameText;
    QString idsInfoValue;
    int ringIdsName = 0;
    int ringIdsInfo = 66141;
    bool createFixture = false;
    bool needsBase = false;
    QString baseNickname;
    int stridName = 0;
    QStringList roomNames;
    QString startRoom;
    double priceVariance = 0.15;
    QString templateBase;
};

class DockingRingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DockingRingDialog(QWidget *parent,
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
                               int stridNameValue,
                               bool initialCreateFixture = false,
                               const QString &windowTitle = QString(),
                               const DockingRingCreateRequest *initialRequest = nullptr);

    DockingRingCreateRequest result() const;

private:
    void buildUi(const QString &planetNickname,
                 const QString &baseNickname,
                 const QStringList &loadouts,
                 const QStringList &factions,
                 const QVector<QPair<QString, QString>> &existingBases,
                 const QStringList &pilots,
                 const QStringList &voices,
                 const QString &defaultFactionDisplay,
                 const QString &idsNameText,
                 const QString &idsInfoValue,
                 int stridNameValue);
    void ensureRoomCheckbox(const QString &roomName, bool checkedByDefault);
    void refreshStartRoomChoices(const QString &preferredRoom = QString());
    void applyTemplateRooms();

    bool m_needsBase = false;
    QHash<QString, QStringList> m_templateRoomsByBase;
    QHash<QString, QCheckBox *> m_roomChecks;
    QVBoxLayout *m_roomsLayout = nullptr;
    QLineEdit *m_nicknameEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_factionCombo = nullptr;
    QComboBox *m_voiceCombo = nullptr;
    QLineEdit *m_costumeEdit = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QSpinBox *m_difficultySpin = nullptr;
    QLineEdit *m_idsNameEdit = nullptr;
    QLineEdit *m_idsInfoEdit = nullptr;
    QLineEdit *m_baseNicknameEdit = nullptr;
    QSpinBox *m_stridNameSpin = nullptr;
    QComboBox *m_startRoomCombo = nullptr;
    QDoubleSpinBox *m_priceVarianceSpin = nullptr;
    QComboBox *m_templateBaseCombo = nullptr;
    QCheckBox *m_createFixtureCheck = nullptr;
};

} // namespace flatlas::editors