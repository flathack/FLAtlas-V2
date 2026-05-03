#pragma once
// editors/base/BaseEditorPage.h – Basis-Editor (Phase 10)

#include <QWidget>
#include <QHash>
#include <memory>
#include "domain/BaseData.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QSplitter;
class QTabWidget;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

namespace flatlas::editors {

class RoomEditor;

class BaseEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit BaseEditorPage(QWidget *parent = nullptr);
    ~BaseEditorPage() override;

    /// Load a base from INI file by nickname.
    bool loadBase(const QString &filePath, const QString &baseNickname);

    /// Load directly from a BaseData object (e.g. from builder).
    void setBase(std::unique_ptr<flatlas::domain::BaseData> base);

    /// Save the base to the given file path.
    bool save(const QString &filePath);

    flatlas::domain::BaseData *data() const;
    QString baseNickname() const;
    QString filePath() const { return m_filePath; }
    bool isDirty() const { return m_dirty; }

signals:
    void titleChanged(const QString &title);

private:
    void setupUi();
    void setupToolBar();
    QWidget *createGeneralTab();
    QWidget *createEquipmentShipsTab();
    void populateFromData();
    void applyToData();
    void populateEquipmentOptions();
    void refreshEquipmentList();
    void filterEquipmentLists();
    void addSelectedEquipment();
    void removeSelectedEquipment();
    void onNewBase();
    void markDirty();
    void setDirty(bool dirty);
    void refreshTitle();

    std::unique_ptr<flatlas::domain::BaseData> m_data;
    QString m_filePath;
    bool m_dirty = false;
    bool m_loadingUi = false;

    QToolBar *m_toolBar = nullptr;
    QSplitter *m_splitter = nullptr;
    QTabWidget *m_tabs = nullptr;

    // Properties panel
    QLineEdit *m_nicknameEdit = nullptr;
    QLineEdit *m_archetypeEdit = nullptr;
    QLineEdit *m_systemEdit = nullptr;
    QSpinBox *m_idsNameSpin = nullptr;
    QSpinBox *m_idsInfoSpin = nullptr;
    QLineEdit *m_posEdit = nullptr;
    QLineEdit *m_dockWithEdit = nullptr;

    // Room editor
    RoomEditor *m_roomEditor = nullptr;

    // Equipment & Ships
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
};

} // namespace flatlas::editors
