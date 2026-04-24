#pragma once

#include "BaseEditService.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedLayout;
class QTableWidget;
class QTableWidgetItem;

namespace flatlas::rendering {
class ModelViewport3D;
}

namespace flatlas::editors {

class BaseEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit BaseEditDialog(const BaseEditState &state,
                            const QHash<QString, QString> &textOverrides = {},
                            QWidget *parent = nullptr);

    BaseEditState state() const;

private slots:
    void refreshStartRooms();
    void addRoom();
    void removeSelectedRoom();
    void onRoomSelectionChanged();
    void onRoomItemChanged(QTableWidgetItem *item);
    void onNpcItemChanged(QTableWidgetItem *item);
    void addNpc();
    void removeSelectedNpc();

private:
    void populateRooms(const QVector<BaseRoomState> &rooms);
    BaseRoomState roomFromRow(int row) const;
    void populateNpcTable(int roomRow);
    void syncNpcStateFromTable();
    QStringList roleChoicesForRoom(const QString &roomName) const;
    void populateSceneCombo(int row, const QString &roomName, const QString &currentScene);
    void refreshPreview();
    void applyTemplateSelection();
    void applyArchetypeDefaults();

    BaseEditState m_initialState;
    QHash<QString, QString> m_textOverrides;
    QVector<BaseRoomState> m_roomStates;
    QLineEdit *m_objectNicknameEdit = nullptr;
    QLineEdit *m_baseNicknameEdit = nullptr;
    QComboBox *m_archetypeCombo = nullptr;
    QComboBox *m_loadoutCombo = nullptr;
    QComboBox *m_reputationCombo = nullptr;
    QComboBox *m_pilotCombo = nullptr;
    QComboBox *m_voiceCombo = nullptr;
    QComboBox *m_headCombo = nullptr;
    QComboBox *m_bodyCombo = nullptr;
    QLineEdit *m_bgcsEdit = nullptr;
    QLineEdit *m_displayNameEdit = nullptr;
    QComboBox *m_startRoomCombo = nullptr;
    QDoubleSpinBox *m_priceVarianceSpin = nullptr;
    QPlainTextEdit *m_infocardEdit = nullptr;
    QComboBox *m_templateCombo = nullptr;
    QCheckBox *m_copyNpcsCheck = nullptr;
    QCheckBox *m_randomNpcAppearanceCheck = nullptr;
    QLabel *m_templateInfoLabel = nullptr;
    QTableWidget *m_roomTable = nullptr;
    QTableWidget *m_npcTable = nullptr;
    QPushButton *m_addRoomButton = nullptr;
    QPushButton *m_removeRoomButton = nullptr;
    QPushButton *m_addNpcButton = nullptr;
    QPushButton *m_removeNpcButton = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    QStackedLayout *m_previewStack = nullptr;
    QString m_lastSuggestedLoadout;
    QString m_lastSuggestedIdsInfo;
    QHash<QString, BaseArchetypeDefaults> m_archetypeDefaultsCache;
};

} // namespace flatlas::editors