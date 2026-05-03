// editors/base/BaseEditorPage.cpp – Basis-Editor (Phase 10)

#include "BaseEditorPage.h"
#include "BasePersistence.h"
#include "BaseEquipmentService.h"
#include "BaseBuilder.h"
#include "RoomEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QToolBar>
#include <QTabWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QSignalBlocker>
#include <QSet>

#include <algorithm>

using namespace flatlas::domain;

namespace flatlas::editors {

BaseEditorPage::BaseEditorPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

BaseEditorPage::~BaseEditorPage() = default;

// ─── UI Setup ────────────────────────────────────────────

void BaseEditorPage::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs, 1);

    m_tabs->addTab(createGeneralTab(), tr("General"));

    m_roomEditor = new RoomEditor(this);
    connect(m_roomEditor, &RoomEditor::roomsChanged, this, [this]() {
        if (m_data && !m_loadingUi)
            markDirty();
    });
    m_tabs->addTab(m_roomEditor, tr("Room Editor"));
    m_tabs->addTab(createEquipmentShipsTab(), tr("Equipment & Ships"));
    m_tabs->addTab(createCommoditiesTab(), tr("Commodities"));

    auto wireLineEdit = [this](QLineEdit *edit) {
        connect(edit, &QLineEdit::textChanged, this, [this]() {
            if (!m_loadingUi)
                markDirty();
        });
    };
    wireLineEdit(m_nicknameEdit);
    wireLineEdit(m_archetypeEdit);
    wireLineEdit(m_systemEdit);
    wireLineEdit(m_posEdit);
    wireLineEdit(m_dockWithEdit);

    connect(m_idsNameSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_loadingUi)
            markDirty();
    });
    connect(m_idsInfoSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_loadingUi)
            markDirty();
    });
}

QWidget *BaseEditorPage::createGeneralTab()
{
    auto *propsWidget = new QWidget(this);
    auto *propsLayout = new QVBoxLayout(propsWidget);

    auto *propsGroup = new QGroupBox(tr("Base Properties"), propsWidget);
    auto *form = new QFormLayout(propsGroup);

    m_nicknameEdit = new QLineEdit(propsGroup);
    form->addRow(tr("Nickname:"), m_nicknameEdit);

    m_archetypeEdit = new QLineEdit(propsGroup);
    form->addRow(tr("Archetype:"), m_archetypeEdit);

    m_systemEdit = new QLineEdit(propsGroup);
    form->addRow(tr("System:"), m_systemEdit);

    m_idsNameSpin = new QSpinBox(propsGroup);
    m_idsNameSpin->setRange(0, 999999999);
    form->addRow(tr("IDS Name:"), m_idsNameSpin);

    m_idsInfoSpin = new QSpinBox(propsGroup);
    m_idsInfoSpin->setRange(0, 999999999);
    form->addRow(tr("IDS Info:"), m_idsInfoSpin);

    m_posEdit = new QLineEdit(propsGroup);
    m_posEdit->setPlaceholderText(tr("x, y, z"));
    form->addRow(tr("Position:"), m_posEdit);

    m_dockWithEdit = new QLineEdit(propsGroup);
    m_dockWithEdit->setPlaceholderText(tr("comma-separated nicknames"));
    form->addRow(tr("Dock With:"), m_dockWithEdit);

    propsLayout->addWidget(propsGroup);
    propsLayout->addStretch();
    return propsWidget;
}

QWidget *BaseEditorPage::createEquipmentShipsTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *equipmentGroup = new QGroupBox(tr("Base Equipment"), tab);
    auto *equipmentLayout = new QVBoxLayout(equipmentGroup);
    m_equipmentCombo = new QComboBox(equipmentGroup);
    m_equipmentCombo->hide();

    auto *equipmentColumns = new QHBoxLayout();
    auto *availableColumn = new QVBoxLayout();
    availableColumn->addWidget(new QLabel(tr("Available"), equipmentGroup));
    m_equipmentFilterEdit = new QLineEdit(equipmentGroup);
    m_equipmentFilterEdit->setPlaceholderText(tr("Search equipment..."));
    availableColumn->addWidget(m_equipmentFilterEdit);
    m_equipmentAvailableTabs = new QTabWidget(equipmentGroup);
    availableColumn->addWidget(m_equipmentAvailableTabs, 1);
    equipmentColumns->addLayout(availableColumn, 1);

    auto *moveButtons = new QVBoxLayout();
    moveButtons->addStretch(1);
    m_addEquipmentButton = new QPushButton(QStringLiteral(">"), equipmentGroup);
    m_addEquipmentButton->setToolTip(tr("Add"));
    m_addEquipmentButton->setFixedWidth(44);
    moveButtons->addWidget(m_addEquipmentButton);
    m_removeEquipmentButton = new QPushButton(QStringLiteral("<"), equipmentGroup);
    m_removeEquipmentButton->setToolTip(tr("Remove Selected"));
    m_removeEquipmentButton->setFixedWidth(44);
    moveButtons->addWidget(m_removeEquipmentButton);
    moveButtons->addStretch(1);
    equipmentColumns->addLayout(moveButtons);

    auto *assignedColumn = new QVBoxLayout();
    assignedColumn->addWidget(new QLabel(tr("On This Base"), equipmentGroup));
    m_equipmentList = new QListWidget(equipmentGroup);
    m_equipmentList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    assignedColumn->addWidget(m_equipmentList, 1);
    equipmentColumns->addLayout(assignedColumn, 1);

    equipmentLayout->addLayout(equipmentColumns, 1);
    m_equipmentStatusLabel = new QLabel(equipmentGroup);
    m_equipmentStatusLabel->setWordWrap(true);
    equipmentLayout->addWidget(m_equipmentStatusLabel);
    layout->addWidget(equipmentGroup, 1);

    auto *shipsGroup = new QGroupBox(tr("Ships (maximum 3)"), tab);
    auto *shipsForm = new QFormLayout(shipsGroup);
    for (int i = 0; i < BaseEquipmentService::MaxShipsPerBase; ++i) {
        m_shipSlotCombos[i] = new QComboBox(shipsGroup);
        m_shipSlotCombos[i]->setMinimumWidth(360);
        m_shipLevelSpins[i] = new QSpinBox(shipsGroup);
        m_shipLevelSpins[i]->setRange(0, 100);
        m_shipLevelSpins[i]->setValue(1);
        auto *slotRow = new QHBoxLayout();
        slotRow->addWidget(m_shipSlotCombos[i], 1);
        slotRow->addWidget(new QLabel(tr("Level:"), shipsGroup));
        slotRow->addWidget(m_shipLevelSpins[i]);
        shipsForm->addRow(tr("Ship Slot %1:").arg(i + 1), slotRow);
        connect(m_shipSlotCombos[i], &QComboBox::currentIndexChanged, this, [this](int) {
            if (!m_loadingUi)
                markDirty();
        });
        connect(m_shipLevelSpins[i], qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
            if (!m_loadingUi)
                markDirty();
        });
    }
    auto *limitHint = new QLabel(tr("Freelancer supports a maximum of 3 ships per base."), shipsGroup);
    limitHint->setWordWrap(true);
    shipsForm->addRow(QString(), limitHint);
    layout->addWidget(shipsGroup, 0);

    connect(m_addEquipmentButton, &QPushButton::clicked, this, &BaseEditorPage::addSelectedEquipment);
    connect(m_removeEquipmentButton, &QPushButton::clicked, this, &BaseEditorPage::removeSelectedEquipment);
    connect(m_equipmentFilterEdit, &QLineEdit::textChanged, this, &BaseEditorPage::filterEquipmentLists);

    return tab;
}

QWidget *BaseEditorPage::createCommoditiesTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    auto *availableColumn = new QVBoxLayout();
    availableColumn->addWidget(new QLabel(tr("Available"), tab));
    m_commodityFilterEdit = new QLineEdit(tab);
    m_commodityFilterEdit->setPlaceholderText(tr("Search commodities..."));
    availableColumn->addWidget(m_commodityFilterEdit);
    m_availableCommodityList = new QListWidget(tab);
    m_availableCommodityList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    availableColumn->addWidget(m_availableCommodityList, 1);
    layout->addLayout(availableColumn, 1);

    auto *moveButtons = new QVBoxLayout();
    moveButtons->addStretch(1);
    m_addCommodityButton = new QPushButton(QStringLiteral(">"), tab);
    m_addCommodityButton->setToolTip(tr("Add"));
    m_addCommodityButton->setFixedWidth(44);
    moveButtons->addWidget(m_addCommodityButton);
    m_removeCommodityButton = new QPushButton(QStringLiteral("<"), tab);
    m_removeCommodityButton->setToolTip(tr("Remove Selected"));
    m_removeCommodityButton->setFixedWidth(44);
    moveButtons->addWidget(m_removeCommodityButton);
    moveButtons->addStretch(1);
    layout->addLayout(moveButtons);

    auto *assignedColumn = new QVBoxLayout();
    assignedColumn->addWidget(new QLabel(tr("On This Base"), tab));
    m_assignedCommodityList = new QListWidget(tab);
    m_assignedCommodityList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    assignedColumn->addWidget(m_assignedCommodityList, 1);
    layout->addLayout(assignedColumn, 1);

    connect(m_commodityFilterEdit, &QLineEdit::textChanged, this, &BaseEditorPage::filterCommodityList);
    connect(m_addCommodityButton, &QPushButton::clicked, this, &BaseEditorPage::addSelectedCommodities);
    connect(m_removeCommodityButton, &QPushButton::clicked, this, &BaseEditorPage::removeSelectedCommodities);
    connect(m_availableCommodityList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        addSelectedCommodities();
    });
    return tab;
}

void BaseEditorPage::populateEquipmentOptions()
{
    if (!m_equipmentCombo)
        return;

    const QString baseNickname = m_data ? m_data->nickname : QString();
    const BaseEquipmentState state = BaseEquipmentService::load(m_filePath, baseNickname);

    m_equipmentCombo->clear();
    if (m_equipmentAvailableTabs) {
        m_equipmentAvailableTabs->clear();
        m_equipmentListsByGroup.clear();
    }
    QSet<QString> assigned;
    if (m_data) {
        for (const QString &nickname : std::as_const(m_data->equipment))
            assigned.insert(nickname.trimmed().toLower());
    }
    for (const auto &option : state.equipmentOptions) {
        m_equipmentCombo->addItem(option.displayLabel, option.nickname);
        if (!m_equipmentAvailableTabs || assigned.contains(option.nickname.trimmed().toLower()))
            continue;
        const QString group = option.groupLabel.trimmed().isEmpty() ? tr("General") : option.groupLabel.trimmed();
        QListWidget *list = m_equipmentListsByGroup.value(group, nullptr);
        if (!list) {
            list = new QListWidget(m_equipmentAvailableTabs);
            list->setSelectionMode(QAbstractItemView::ExtendedSelection);
            connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { addSelectedEquipment(); });
            m_equipmentListsByGroup.insert(group, list);
            m_equipmentAvailableTabs->addTab(list, group);
        }
        auto *item = new QListWidgetItem(option.displayLabel, list);
        item->setData(Qt::UserRole, option.nickname);
    }
    filterEquipmentLists();

    for (QComboBox *combo : m_shipSlotCombos) {
        if (!combo)
            continue;
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItem(tr("Empty"), QString());
        for (const auto &option : state.shipPackageOptions)
            combo->addItem(option.displayLabel, option.nickname);
    }

    if (m_equipmentStatusLabel) {
        if (!state.warningMessage.trimmed().isEmpty())
            m_equipmentStatusLabel->setText(state.warningMessage);
        else
            m_equipmentStatusLabel->setText(tr("Ship packages are shown as nickname - ingamename. Empty ship slots are allowed."));
    }
}

void BaseEditorPage::refreshEquipmentList()
{
    if (!m_equipmentList || !m_data)
        return;
    m_equipmentList->clear();
    for (const QString &nickname : std::as_const(m_data->equipment)) {
        QString label = nickname;
        const int index = m_equipmentCombo ? m_equipmentCombo->findData(nickname) : -1;
        if (index >= 0)
            label = m_equipmentCombo->itemText(index);
        auto *item = new QListWidgetItem(label, m_equipmentList);
        item->setData(Qt::UserRole, nickname);
    }
}

void BaseEditorPage::filterEquipmentLists()
{
    const QString filter = m_equipmentFilterEdit ? m_equipmentFilterEdit->text().trimmed().toLower() : QString();
    for (QListWidget *list : std::as_const(m_equipmentListsByGroup)) {
        if (!list)
            continue;
        for (int row = 0; row < list->count(); ++row) {
            QListWidgetItem *item = list->item(row);
            const QString nickname = item->data(Qt::UserRole).toString().toLower();
            const QString label = item->text().toLower();
            item->setHidden(!filter.isEmpty() && !nickname.contains(filter) && !label.contains(filter));
        }
    }
}

void BaseEditorPage::addSelectedEquipment()
{
    if (!m_data || !m_equipmentCombo)
        return;
    QListWidget *currentList = m_equipmentAvailableTabs
        ? qobject_cast<QListWidget *>(m_equipmentAvailableTabs->currentWidget())
        : nullptr;
    QStringList nicknames;
    if (currentList) {
        for (QListWidgetItem *item : currentList->selectedItems()) {
            const QString nick = item->data(Qt::UserRole).toString().trimmed();
            if (!nick.isEmpty())
                nicknames.append(nick);
        }
    }
    if (nicknames.isEmpty())
        return;
    for (const QString &nick : nicknames) {
        if (!m_data->equipment.contains(nick, Qt::CaseInsensitive))
            m_data->equipment.append(nick);
    }
    populateEquipmentOptions();
    refreshEquipmentList();
    if (!m_loadingUi)
        markDirty();
}

void BaseEditorPage::removeSelectedEquipment()
{
    if (!m_data || !m_equipmentList)
        return;
    const auto selected = m_equipmentList->selectedItems();
    for (QListWidgetItem *item : selected)
        m_data->equipment.removeAll(item->data(Qt::UserRole).toString());
    populateEquipmentOptions();
    refreshEquipmentList();
    if (!selected.isEmpty() && !m_loadingUi)
        markDirty();
}

void BaseEditorPage::populateCommodityOptions()
{
    if (!m_availableCommodityList)
        return;
    const QString baseNickname = m_data ? m_data->nickname : QString();
    const BaseEquipmentState state = BaseEquipmentService::load(m_filePath, baseNickname);
    QSet<QString> assigned;
    if (m_data) {
        for (const QString &nickname : std::as_const(m_data->commodities))
            assigned.insert(nickname.trimmed().toLower());
    }
    m_availableCommodityList->clear();
    for (const BaseEquipmentOption &option : state.commodityOptions) {
        if (assigned.contains(option.nickname.trimmed().toLower()))
            continue;
        auto *item = new QListWidgetItem(option.displayLabel, m_availableCommodityList);
        item->setData(Qt::UserRole, option.nickname);
    }
    filterCommodityList();
}

void BaseEditorPage::refreshCommodityList()
{
    if (!m_assignedCommodityList || !m_data)
        return;
    const BaseEquipmentState state = BaseEquipmentService::load(m_filePath, m_data->nickname);
    m_assignedCommodityList->clear();
    for (const QString &nickname : std::as_const(m_data->commodities)) {
        QString label = nickname;
        for (const BaseEquipmentOption &option : state.commodityOptions) {
            if (option.nickname.compare(nickname, Qt::CaseInsensitive) == 0) {
                label = option.displayLabel;
                break;
            }
        }
        auto *item = new QListWidgetItem(label, m_assignedCommodityList);
        item->setData(Qt::UserRole, nickname);
    }
}

void BaseEditorPage::filterCommodityList()
{
    if (!m_availableCommodityList)
        return;
    const QString filter = m_commodityFilterEdit ? m_commodityFilterEdit->text().trimmed().toLower() : QString();
    for (int row = 0; row < m_availableCommodityList->count(); ++row) {
        QListWidgetItem *item = m_availableCommodityList->item(row);
        const QString nickname = item->data(Qt::UserRole).toString().toLower();
        const QString label = item->text().toLower();
        item->setHidden(!filter.isEmpty() && !nickname.contains(filter) && !label.contains(filter));
    }
}

void BaseEditorPage::addSelectedCommodities()
{
    if (!m_data || !m_availableCommodityList)
        return;
    const auto selected = m_availableCommodityList->selectedItems();
    for (QListWidgetItem *item : selected) {
        const QString nickname = item->data(Qt::UserRole).toString().trimmed();
        if (!nickname.isEmpty() && !m_data->commodities.contains(nickname, Qt::CaseInsensitive))
            m_data->commodities.append(nickname);
    }
    populateCommodityOptions();
    refreshCommodityList();
    if (!selected.isEmpty() && !m_loadingUi)
        markDirty();
}

void BaseEditorPage::removeSelectedCommodities()
{
    if (!m_data || !m_assignedCommodityList)
        return;
    const auto selected = m_assignedCommodityList->selectedItems();
    for (QListWidgetItem *item : selected)
        m_data->commodities.removeAll(item->data(Qt::UserRole).toString());
    populateCommodityOptions();
    refreshCommodityList();
    if (!selected.isEmpty() && !m_loadingUi)
        markDirty();
}

void BaseEditorPage::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));
    m_toolBar->setMovable(false);

    m_toolBar->addAction(tr("New Base..."), this, &BaseEditorPage::onNewBase);
    m_toolBar->addAction(tr("Apply Changes"), this, [this]() {
        applyToData();
        if (m_data)
            markDirty();
    });
}

// ─── Load / Save ─────────────────────────────────────────

bool BaseEditorPage::loadBase(const QString &filePath, const QString &baseNickname)
{
    auto data = BasePersistence::loadFromIni(filePath, baseNickname);
    if (!data) return false;

    m_data = std::move(data);
    m_filePath = filePath;

    populateFromData();
    setDirty(false);
    return true;
}

void BaseEditorPage::setBase(std::unique_ptr<BaseData> base)
{
    m_data = std::move(base);
    populateFromData();
    setDirty(false);
}

bool BaseEditorPage::save(const QString &filePath)
{
    if (!m_data) return false;
    applyToData();
    if (!BasePersistence::save(*m_data, filePath))
        return false;
    m_filePath = filePath;
    setDirty(false);
    return true;
}

BaseData *BaseEditorPage::data() const
{
    return m_data.get();
}

QString BaseEditorPage::baseNickname() const
{
    return m_data ? m_data->nickname : QString();
}

// ─── Data ↔ UI ──────────────────────────────────────────

void BaseEditorPage::populateFromData()
{
    if (!m_data) return;

    m_loadingUi = true;

    m_nicknameEdit->setText(m_data->nickname);
    m_archetypeEdit->setText(m_data->archetype);
    m_systemEdit->setText(m_data->system);
    m_idsNameSpin->setValue(m_data->idsName);
    m_idsInfoSpin->setValue(m_data->idsInfo);

    m_posEdit->setText(QStringLiteral("%1, %2, %3")
        .arg(m_data->position.x(), 0, 'f', 0)
        .arg(m_data->position.y(), 0, 'f', 0)
        .arg(m_data->position.z(), 0, 'f', 0));

    m_dockWithEdit->setText(m_data->dockWith.join(QStringLiteral(", ")));

    m_roomEditor->setBaseNickname(m_data->nickname);
    m_roomEditor->setRooms(m_data->rooms);

    populateEquipmentOptions();
    refreshEquipmentList();
    populateCommodityOptions();
    refreshCommodityList();
    for (int i = 0; i < BaseEquipmentService::MaxShipsPerBase; ++i) {
        QComboBox *combo = m_shipSlotCombos[i];
        if (!combo)
            continue;
        const QSignalBlocker blocker(combo);
        const QString package = m_data->shipPackages.value(i);
        const int index = combo->findData(package);
        combo->setCurrentIndex(index >= 0 ? index : 0);
        if (m_shipLevelSpins[i])
            m_shipLevelSpins[i]->setValue(m_data->shipPackageLevels.value(i, QStringLiteral("1")).toInt());
    }
    m_loadingUi = false;
}

void BaseEditorPage::applyToData()
{
    if (!m_data) return;

    m_data->nickname = m_nicknameEdit->text().trimmed();
    m_data->archetype = m_archetypeEdit->text().trimmed();
    m_data->system = m_systemEdit->text().trimmed();
    m_data->idsName = m_idsNameSpin->value();
    m_data->idsInfo = m_idsInfoSpin->value();

    // Parse position
    auto parts = m_posEdit->text().split(',');
    if (parts.size() >= 3) {
        m_data->position = QVector3D(
            parts[0].trimmed().toFloat(),
            parts[1].trimmed().toFloat(),
            parts[2].trimmed().toFloat());
    }

    // Parse dock_with
    m_data->dockWith.clear();
    for (const auto &d : m_dockWithEdit->text().split(',')) {
        QString trimmed = d.trimmed();
        if (!trimmed.isEmpty())
            m_data->dockWith.append(trimmed);
    }

    m_data->rooms = m_roomEditor->rooms();
    m_roomEditor->setBaseNickname(m_data->nickname);
    m_data->commodities.clear();
    if (m_assignedCommodityList) {
        for (int row = 0; row < m_assignedCommodityList->count(); ++row) {
            const QString nickname = m_assignedCommodityList->item(row)->data(Qt::UserRole).toString().trimmed();
            if (!nickname.isEmpty() && !m_data->commodities.contains(nickname, Qt::CaseInsensitive))
                m_data->commodities.append(nickname);
        }
    }
    QStringList ships;
    QStringList shipLevels;
    bool removedDuplicate = false;
    for (QComboBox *combo : m_shipSlotCombos) {
        if (!combo)
            continue;
        const QString nickname = combo->currentData().toString().trimmed();
        if (nickname.isEmpty())
            continue;
        if (ships.contains(nickname, Qt::CaseInsensitive)) {
            const QSignalBlocker blocker(combo);
            combo->setCurrentIndex(0);
            removedDuplicate = true;
            continue;
        }
        ships.append(nickname);
        const int slot = static_cast<int>(std::distance(std::begin(m_shipSlotCombos), std::find(std::begin(m_shipSlotCombos), std::end(m_shipSlotCombos), combo)));
        shipLevels.append(QString::number(slot >= 0 && slot < BaseEquipmentService::MaxShipsPerBase && m_shipLevelSpins[slot]
                                             ? m_shipLevelSpins[slot]->value()
                                             : 1));
        if (ships.size() >= BaseEquipmentService::MaxShipsPerBase)
            break;
    }
    m_data->shipPackages = ships;
    m_data->shipPackageLevels = shipLevels;
    if (removedDuplicate && m_equipmentStatusLabel)
        m_equipmentStatusLabel->setText(tr("Duplicate ship assignments were removed. Freelancer supports each ship package only once per base."));
}

// ─── New Base (from template) ────────────────────────────

void BaseEditorPage::onNewBase()
{
    bool ok;
    QString nickname = QInputDialog::getText(this, tr("New Base"),
                                              tr("Base nickname:"),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok || nickname.trimmed().isEmpty()) return;
    nickname = nickname.trimmed();

    QStringList templates = BaseBuilder::templateNames();
    QString tmplName = QInputDialog::getItem(this, tr("Base Template"),
                                              tr("Select template:"),
                                              templates, 0, false, &ok);
    if (!ok) return;

    auto tmpl = BaseBuilder::templateFromName(tmplName);
    auto base = BaseBuilder::create(tmpl, nickname, QString());

    m_data = std::make_unique<BaseData>(std::move(base));
    populateFromData();
    setDirty(false);
}

void BaseEditorPage::markDirty()
{
    setDirty(true);
}

void BaseEditorPage::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        refreshTitle();
        return;
    }
    m_dirty = dirty;
    refreshTitle();
}

void BaseEditorPage::refreshTitle()
{
    if (!m_data)
        return;

    QString title = m_data->nickname.trimmed();
    if (title.isEmpty())
        title = tr("Base Editor");
    if (m_dirty)
        title += QLatin1Char('*');
    emit titleChanged(title);
}

} // namespace flatlas::editors
