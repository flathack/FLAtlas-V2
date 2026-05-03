#pragma once

#include "infrastructure/parser/IniParser.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

class QAction;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QToolBar;
class QLabel;

namespace flatlas::editors {

struct NpcBribe {
    QString faction;
    int price = 0;
    int ids = 0;
};

struct NpcRumorAssignment {
    QString kind;
    QString stateFrom;
    QString stateTo;
    int weight = 1;
    int ids = 0;
};

struct NpcRecord {
    QString nickname;
    QString baseNickname;
    QString room;
    QString baseFaction;
    QString affiliation;
    QString npcType;
    QString body;
    QString head;
    QString leftHand;
    QString rightHand;
    QString voice;
    int individualName = 0;
    QString individualNameText;
    int info = 0;
    QVector<QPair<QString, QString>> preservedEntries;
    QVector<NpcBribe> bribes;
    QVector<NpcRumorAssignment> rumors;
    int sectionIndex = -1;
    bool newlyCreated = false;
};

struct NpcRoomRecord {
    QString nickname;
    QStringList fixtureNpcs;
};

struct NpcBaseRecord {
    QString nickname;
    QString systemNickname;
    QString displayName;
    QString fileRelativePath;
    QVector<NpcRoomRecord> rooms;
    QVector<NpcRecord> npcs;
};

struct NpcSystemRecord {
    QString nickname;
    QString displayName;
};

struct NpcExistingRumor {
    QString kind;
    QString stateFrom;
    QString stateTo;
    int weight = 1;
    int ids = 0;
    QString preview;
};

class NpcEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit NpcEditorPage(QWidget *parent = nullptr);

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    void reloadCurrentContext();
    bool loadGameRoot(const QString &gameRoot, QString *errorMessage = nullptr);
    bool saveCurrentFile(QString *errorMessage = nullptr);
    bool applyIdsNameEdits(QString *errorMessage);

    void populateSelectors();
    void populateBaseSelector();
    void populateRooms();
    void populateNpcTable();
    void populateEditor();
    void populateChoiceLists();
    void populateRumorChoices();
    void clearEditor();
    void setEditorEnabled(bool enabled);
    void saveEditorToCurrentNpc();
    bool validateEditor(QString *errorMessage) const;

    int currentSystemIndex() const;
    int currentBaseIndex() const;
    int currentRoomIndex() const;
    int currentNpcIndex() const;
    NpcBaseRecord *currentBase();
    const NpcBaseRecord *currentBase() const;
    NpcRecord *currentNpc();
    const NpcRecord *currentNpc() const;
    QString resolvedIdsText(int ids) const;
    QString displayLabel(const QString &nickname, const QString &resolved) const;
    QString factionDisplay(const QString &nickname) const;
    QString factionNicknameFromDisplay(const QString &display) const;
    QString currentFactionComboNickname(const QComboBox *combo) const;
    void setFactionComboNickname(QComboBox *combo, const QString &nickname);
    QString bribePreviewText(int row) const;
    QStringList allRoomNames(const NpcBaseRecord &base) const;
    QStringList allFactions() const;
    void refreshSelectedBribeText();
    void refreshSelectedRumorText();

    void onSystemChanged();
    void onBaseChanged();
    void onRoomChanged();
    void onNpcSelectionChanged();
    void onNewNpc();
    void onDeleteNpc();
    void onAddBribe();
    void onRemoveBribe();
    void onAddRumor();
    void onNewRumor();
    void onRemoveRumor();

    QString m_gameRoot;
    QString m_mbasesPath;
    flatlas::infrastructure::IniDocument m_mbasesDoc;
    QVector<NpcSystemRecord> m_systems;
    QVector<NpcBaseRecord> m_bases;
    QVector<NpcExistingRumor> m_existingRumors;
    QHash<QString, QString> m_idsTextByNumber;
    QStringList m_bodyChoices;
    QStringList m_headChoices;
    QStringList m_handChoices;
    QStringList m_voiceChoices;
    QStringList m_factionChoices;
    QHash<QString, QString> m_factionDisplayByNickname;
    int m_modBribePrice = 10000;
    int m_currentNpcRow = -1;
    QString m_editorBaseNickname;
    int m_editorNpcIndex = -1;
    bool m_populating = false;

    QToolBar *m_toolBar = nullptr;
    QAction *m_reloadAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_newNpcAction = nullptr;
    QAction *m_deleteNpcAction = nullptr;
    QComboBox *m_systemCombo = nullptr;
    QComboBox *m_baseCombo = nullptr;
    QListWidget *m_roomList = nullptr;
    QTableWidget *m_npcTable = nullptr;
    QLabel *m_statusLabel = nullptr;

    QLineEdit *m_nicknameEdit = nullptr;
    QComboBox *m_roomCombo = nullptr;
    QComboBox *m_baseFactionCombo = nullptr;
    QComboBox *m_affiliationCombo = nullptr;
    QComboBox *m_bodyCombo = nullptr;
    QComboBox *m_headCombo = nullptr;
    QComboBox *m_leftHandCombo = nullptr;
    QComboBox *m_rightHandCombo = nullptr;
    QComboBox *m_voiceCombo = nullptr;
    QSpinBox *m_individualNameSpin = nullptr;
    QLineEdit *m_individualNameTextEdit = nullptr;
    QSpinBox *m_infoSpin = nullptr;
    QLabel *m_namePreviewLabel = nullptr;
    QLabel *m_infoPreviewLabel = nullptr;
    QTableWidget *m_bribeTable = nullptr;
    QPlainTextEdit *m_bribeTextPreview = nullptr;
    QTableWidget *m_rumorTable = nullptr;
    QPlainTextEdit *m_rumorTextPreview = nullptr;
    QComboBox *m_existingRumorCombo = nullptr;
    QComboBox *m_rumorKindCombo = nullptr;
    QPushButton *m_bottomSaveButton = nullptr;
};

} // namespace flatlas::editors
