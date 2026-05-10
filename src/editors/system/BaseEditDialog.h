#pragma once

#include "BaseEditService.h"

#include <QDialog>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedLayout;
class QTabWidget;
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

signals:
    void roomActivationRequested(const QString &roomName, const QString &modelPath);

private slots:
    void refreshStartRooms();
    void addRoom();
    void removeSelectedRoom();
    void onRoomSelectionChanged();
    void onRoomItemChanged(QTableWidgetItem *item);
    void onNpcItemChanged(QTableWidgetItem *item);
    void addNpc();
    void removeSelectedNpc();
    void activateRoomFromTable();

private:
    void populateRooms(const QVector<BaseRoomState> &rooms);
    BaseRoomState roomFromRow(int row) const;
    void populateNpcTable(int roomRow);
    void syncNpcStateFromTable();
    QStringList roleChoicesForRoom(const QString &roomName) const;
    void populateSceneCombo(int row, const QString &roomName, const QString &currentScene);
    void refreshPreview();
    void refreshRoomPreview();
    void refreshLoadoutOptionsForArchetype(const QString &preferredLoadout = {});
    void applyTemplateSelection();
    void applyArchetypeDefaults();
    int selectedRoomRow() const;
    QString selectedRoomName() const;
    int activeRoomRow() const;
    QString activeRoomName() const;
    int findRoomRowByName(const QString &roomName) const;
    void setSelectedRoom(const QString &roomName);
    void setActiveRoom(const QString &roomName);
    void updateRoomSelectionUi();
    void updateRoomActivationUi();
    QWidget *createEquipmentShipsTab(const BaseEditState &state);
    QWidget *createCommoditiesTab(const BaseEditState &state);
    void ensureEquipmentShipsTab();
    void ensureCommoditiesTab();
    void addSelectedEquipment();
    void removeSelectedEquipment();
    void addSelectedCommodities();
    void removeSelectedCommodities();
    void onCommoditySelectionChanged();
    void syncCommoditySettingsFromUi();
    void recalcCommodityEndPrice();
    void recalcCommodityFactorFromPrice();
    void enforceShipSlotRules();
    void filterEquipmentLists();
    void filterCommodityList();
    QStringList selectedEquipment() const;
    QStringList selectedCommodities() const;
    QVector<QStringList> selectedCommodityMarketRows() const;
    QStringList selectedShipPackages() const;
    QStringList selectedShipPackageLevels() const;

    BaseEditState m_initialState;
    QHash<QString, QString> m_textOverrides;
    QVector<BaseRoomState> m_roomStates;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_roomsTab = nullptr;
    QWidget *m_equipmentShipsTab = nullptr;
    QWidget *m_commoditiesTab = nullptr;
    bool m_equipmentShipsTabLoaded = false;
    bool m_commoditiesTabLoaded = false;
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
    QComboBox *m_equipmentCombo = nullptr;
    QLineEdit *m_equipmentFilterEdit = nullptr;
    QTabWidget *m_equipmentAvailableTabs = nullptr;
    QHash<QString, QListWidget *> m_equipmentListsByGroup;
    QListWidget *m_equipmentList = nullptr;
    QPushButton *m_addEquipmentButton = nullptr;
    QPushButton *m_removeEquipmentButton = nullptr;
    QLabel *m_equipmentStatusLabel = nullptr;
    QComboBox *m_shipSlotCombos[3] = {nullptr, nullptr, nullptr};
    QSpinBox *m_shipLevelSpins[3] = {nullptr, nullptr, nullptr};
    QLineEdit *m_commodityFilterEdit = nullptr;
    QListWidget *m_availableCommodityList = nullptr;
    QListWidget *m_assignedCommodityList = nullptr;
    QPushButton *m_addCommodityButton = nullptr;
    QPushButton *m_removeCommodityButton = nullptr;
    QGroupBox *m_commoditySettingsGroup = nullptr;
    QSpinBox *m_commodityLevelSpin = nullptr;
    QSpinBox *m_commodityRepSpin = nullptr;
    QSpinBox *m_commodityMinStockSpin = nullptr;
    QSpinBox *m_commodityMaxStockSpin = nullptr;
    QComboBox *m_commodityTradeModeCombo = nullptr;
    QDoubleSpinBox *m_commodityFactorSpin = nullptr;
    QSpinBox *m_commodityBasePriceSpin = nullptr;
    QSpinBox *m_commodityEndPriceSpin = nullptr;
    QHash<QString, int> m_commodityBasePrices;
    bool m_loadingCommoditySettings = false;
    QLabel *m_selectedRoomLabel = nullptr;
    QLabel *m_activeRoomLabel = nullptr;
    flatlas::rendering::ModelViewport3D *m_preview = nullptr;
    QLabel *m_previewFallback = nullptr;
    QStackedLayout *m_previewStack = nullptr;
    flatlas::rendering::ModelViewport3D *m_roomPreview = nullptr;
    QLabel *m_roomPreviewFallback = nullptr;
    QStackedLayout *m_roomPreviewStack = nullptr;
    QString m_lastSuggestedLoadout;
    QString m_lastSuggestedIdsInfo;
    QHash<QString, BaseArchetypeDefaults> m_archetypeDefaultsCache;
    QString m_selectedRoomKey;
    QString m_activeRoomKey;
};

} // namespace flatlas::editors
