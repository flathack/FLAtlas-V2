#include "SystemDisplayFilterDialog.h"

#include <QUuid>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using flatlas::domain::SolarObject;
using flatlas::rendering::DisplayFilterAction;
using flatlas::rendering::DisplayFilterField;
using flatlas::rendering::DisplayFilterMatchMode;
using flatlas::rendering::DisplayFilterTarget;
using flatlas::rendering::SystemDisplayFilterRule;
using flatlas::rendering::SystemDisplayFilterSettings;

namespace flatlas::editors {

namespace {

const QList<SolarObject::Type> &filterableTypes()
{
    static const QList<SolarObject::Type> types = {
        SolarObject::Sun,
        SolarObject::Planet,
        SolarObject::Station,
        SolarObject::JumpGate,
        SolarObject::JumpHole,
        SolarObject::TradeLane,
        SolarObject::DockingRing,
        SolarObject::Satellite,
        SolarObject::Waypoint,
        SolarObject::Weapons_Platform,
        SolarObject::Depot,
        SolarObject::Wreck,
        SolarObject::Asteroid,
        SolarObject::Other
    };
    return types;
}

template <typename Enum>
void addEnumItem(QComboBox *combo, const QString &label, Enum value)
{
    combo->addItem(label, static_cast<int>(value));
}

}

SystemDisplayFilterDialog::SystemDisplayFilterDialog(const SystemDisplayFilterSettings &settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("System Display Filters"));
    resize(880, 640);
    buildUi();
    populateQuickToggles();
    populateRulesTable();
    clearRuleEditor();
}

SystemDisplayFilterSettings SystemDisplayFilterDialog::settings() const
{
    SystemDisplayFilterSettings result = m_settings;
    for (auto it = m_objectTypeChecks.begin(); it != m_objectTypeChecks.end(); ++it)
        result.objectVisibilityByType[it.key()] = it.value()->isChecked();
    for (auto it = m_labelTypeChecks.begin(); it != m_labelTypeChecks.end(); ++it)
        result.labelVisibilityByType[it.key()] = it.value()->isChecked();
    return result;
}

void SystemDisplayFilterDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *quickGroup = new QGroupBox(tr("Quick Toggles"), this);
    auto *quickLayout = new QGridLayout(quickGroup);
    quickLayout->addWidget(new QLabel(tr("Object Group"), quickGroup), 0, 0);
    quickLayout->addWidget(new QLabel(tr("Show Objects"), quickGroup), 0, 1);
    quickLayout->addWidget(new QLabel(tr("Show Labels"), quickGroup), 0, 2);

    int row = 1;
    for (SolarObject::Type type : filterableTypes()) {
        auto *nameLabel = new QLabel(displayNameForType(type), quickGroup);
        auto *objectCheck = new QCheckBox(quickGroup);
        auto *labelCheck = new QCheckBox(quickGroup);
        m_objectTypeChecks.insert(static_cast<int>(type), objectCheck);
        m_labelTypeChecks.insert(static_cast<int>(type), labelCheck);

        quickLayout->addWidget(nameLabel, row, 0);
        quickLayout->addWidget(objectCheck, row, 1);
        quickLayout->addWidget(labelCheck, row, 2);
        ++row;
    }
    mainLayout->addWidget(quickGroup);

    auto *rulesGroup = new QGroupBox(tr("Filter Rules"), this);
    auto *rulesLayout = new QVBoxLayout(rulesGroup);

    m_rulesTable = new QTableWidget(rulesGroup);
    m_rulesTable->setColumnCount(6);
    m_rulesTable->setHorizontalHeaderLabels(
        {tr("Active"), tr("Name"), tr("Target"), tr("Field"), tr("Match"), tr("Pattern")});
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rulesLayout->addWidget(m_rulesTable);

    auto *editorLayout = new QFormLayout();
    m_ruleNameEdit = new QLineEdit(rulesGroup);
    editorLayout->addRow(tr("Rule Name"), m_ruleNameEdit);

    m_ruleEnabledCheck = new QCheckBox(tr("Enabled"), rulesGroup);
    editorLayout->addRow(QString(), m_ruleEnabledCheck);

    m_targetCombo = new QComboBox(rulesGroup);
    addEnumItem(m_targetCombo, tr("Object"), DisplayFilterTarget::Object);
    addEnumItem(m_targetCombo, tr("Label"), DisplayFilterTarget::Label);
    addEnumItem(m_targetCombo, tr("Both"), DisplayFilterTarget::Both);
    editorLayout->addRow(tr("Target"), m_targetCombo);

    m_fieldCombo = new QComboBox(rulesGroup);
    addEnumItem(m_fieldCombo, tr("Nickname"), DisplayFilterField::Nickname);
    addEnumItem(m_fieldCombo, tr("Archetype"), DisplayFilterField::Archetype);
    addEnumItem(m_fieldCombo, tr("Type"), DisplayFilterField::Type);
    editorLayout->addRow(tr("Field"), m_fieldCombo);

    m_matchModeCombo = new QComboBox(rulesGroup);
    addEnumItem(m_matchModeCombo, tr("Contains"), DisplayFilterMatchMode::Contains);
    addEnumItem(m_matchModeCombo, tr("Equals"), DisplayFilterMatchMode::Equals);
    addEnumItem(m_matchModeCombo, tr("Starts With"), DisplayFilterMatchMode::StartsWith);
    addEnumItem(m_matchModeCombo, tr("Ends With"), DisplayFilterMatchMode::EndsWith);
    editorLayout->addRow(tr("Match"), m_matchModeCombo);

    m_actionCombo = new QComboBox(rulesGroup);
    addEnumItem(m_actionCombo, tr("Hide"), DisplayFilterAction::Hide);
    addEnumItem(m_actionCombo, tr("Show"), DisplayFilterAction::Show);
    editorLayout->addRow(tr("Action"), m_actionCombo);

    m_patternEdit = new QLineEdit(rulesGroup);
    m_patternEdit->setPlaceholderText(tr("Example: Trade_Lane_Ring"));
    editorLayout->addRow(tr("Pattern"), m_patternEdit);

    rulesLayout->addLayout(editorLayout);

    auto *buttonRow = new QHBoxLayout();
    m_newRuleButton = new QPushButton(tr("New"), rulesGroup);
    m_saveRuleButton = new QPushButton(tr("Save Rule"), rulesGroup);
    m_deleteRuleButton = new QPushButton(tr("Delete"), rulesGroup);
    buttonRow->addWidget(m_newRuleButton);
    buttonRow->addWidget(m_saveRuleButton);
    buttonRow->addWidget(m_deleteRuleButton);
    buttonRow->addStretch(1);
    rulesLayout->addLayout(buttonRow);

    mainLayout->addWidget(rulesGroup, 1);

    auto *dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(dialogButtons);

    connect(m_rulesTable, &QTableWidget::cellClicked, this, [this](int row, int) { loadRuleIntoEditor(row); });
    connect(m_newRuleButton, &QPushButton::clicked, this, &SystemDisplayFilterDialog::clearRuleEditor);
    connect(m_saveRuleButton, &QPushButton::clicked, this, &SystemDisplayFilterDialog::saveRuleFromEditor);
    connect(m_deleteRuleButton, &QPushButton::clicked, this, [this]() {
        const int row = m_rulesTable->currentRow();
        if (row < 0 || row >= m_settings.rules.size())
            return;
        m_settings.rules.removeAt(row);
        populateRulesTable();
        clearRuleEditor();
    });
    connect(dialogButtons, &QDialogButtonBox::accepted, this, [this]() {
        saveRuleFromEditor();
        accept();
    });
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SystemDisplayFilterDialog::populateQuickToggles()
{
    for (SolarObject::Type type : filterableTypes()) {
        const int key = static_cast<int>(type);
        if (m_objectTypeChecks.contains(key))
            m_objectTypeChecks[key]->setChecked(m_settings.objectVisibleForType(type));
        if (m_labelTypeChecks.contains(key))
            m_labelTypeChecks[key]->setChecked(m_settings.labelVisibleForType(type));
    }
}

void SystemDisplayFilterDialog::populateRulesTable()
{
    m_rulesTable->setRowCount(m_settings.rules.size());
    for (int row = 0; row < m_settings.rules.size(); ++row) {
        const SystemDisplayFilterRule &rule = m_settings.rules.at(row);
        m_rulesTable->setItem(row, 0, new QTableWidgetItem(rule.enabled ? tr("Yes") : tr("No")));
        m_rulesTable->setItem(row, 1, new QTableWidgetItem(rule.name));
        m_rulesTable->setItem(row, 2, new QTableWidgetItem(flatlas::rendering::displayFilterTargetToString(rule.target)));
        m_rulesTable->setItem(row, 3, new QTableWidgetItem(flatlas::rendering::displayFilterFieldToString(rule.field)));
        m_rulesTable->setItem(row, 4, new QTableWidgetItem(flatlas::rendering::displayFilterMatchModeToString(rule.matchMode)));
        m_rulesTable->setItem(row, 5, new QTableWidgetItem(rule.pattern));
    }
    m_rulesTable->clearSelection();
}

void SystemDisplayFilterDialog::loadRuleIntoEditor(int row)
{
    if (row < 0 || row >= m_settings.rules.size())
        return;

    const SystemDisplayFilterRule &rule = m_settings.rules.at(row);
    m_editingRuleId = rule.id;
    m_ruleNameEdit->setText(rule.name);
    m_ruleEnabledCheck->setChecked(rule.enabled);
    m_targetCombo->setCurrentIndex(m_targetCombo->findData(static_cast<int>(rule.target)));
    m_fieldCombo->setCurrentIndex(m_fieldCombo->findData(static_cast<int>(rule.field)));
    m_matchModeCombo->setCurrentIndex(m_matchModeCombo->findData(static_cast<int>(rule.matchMode)));
    m_actionCombo->setCurrentIndex(m_actionCombo->findData(static_cast<int>(rule.action)));
    m_patternEdit->setText(rule.pattern);
}

SystemDisplayFilterRule SystemDisplayFilterDialog::buildRuleFromEditor() const
{
    SystemDisplayFilterRule rule;
    rule.id = m_editingRuleId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : m_editingRuleId;
    rule.name = m_ruleNameEdit->text().trimmed();
    rule.enabled = m_ruleEnabledCheck->isChecked();
    rule.target = static_cast<DisplayFilterTarget>(m_targetCombo->currentData().toInt());
    rule.field = static_cast<DisplayFilterField>(m_fieldCombo->currentData().toInt());
    rule.matchMode = static_cast<DisplayFilterMatchMode>(m_matchModeCombo->currentData().toInt());
    rule.action = static_cast<DisplayFilterAction>(m_actionCombo->currentData().toInt());
    rule.pattern = m_patternEdit->text().trimmed();
    return rule;
}

void SystemDisplayFilterDialog::clearRuleEditor()
{
    m_editingRuleId.clear();
    m_ruleNameEdit->clear();
    m_ruleEnabledCheck->setChecked(true);
    m_targetCombo->setCurrentIndex(m_targetCombo->findData(static_cast<int>(DisplayFilterTarget::Label)));
    m_fieldCombo->setCurrentIndex(m_fieldCombo->findData(static_cast<int>(DisplayFilterField::Nickname)));
    m_matchModeCombo->setCurrentIndex(m_matchModeCombo->findData(static_cast<int>(DisplayFilterMatchMode::Contains)));
    m_actionCombo->setCurrentIndex(m_actionCombo->findData(static_cast<int>(DisplayFilterAction::Hide)));
    m_patternEdit->clear();
    m_rulesTable->clearSelection();
}

void SystemDisplayFilterDialog::saveRuleFromEditor()
{
    const SystemDisplayFilterRule rule = buildRuleFromEditor();
    if (rule.name.isEmpty() || rule.pattern.isEmpty())
        return;

    for (int i = 0; i < m_settings.rules.size(); ++i) {
        if (m_settings.rules[i].id == rule.id) {
            m_settings.rules[i] = rule;
            populateRulesTable();
            m_rulesTable->selectRow(i);
            return;
        }
    }

    m_settings.rules.append(rule);
    populateRulesTable();
    m_rulesTable->selectRow(m_settings.rules.size() - 1);
}

QString SystemDisplayFilterDialog::displayNameForType(SolarObject::Type type) const
{
    switch (type) {
    case SolarObject::Sun: return tr("Suns");
    case SolarObject::Planet: return tr("Planets");
    case SolarObject::Station: return tr("Stations");
    case SolarObject::JumpGate: return tr("Jump Gates");
    case SolarObject::JumpHole: return tr("Jump Holes");
    case SolarObject::TradeLane: return tr("Trade Lanes");
    case SolarObject::DockingRing: return tr("Docking Rings");
    case SolarObject::Satellite: return tr("Satellites");
    case SolarObject::Waypoint: return tr("Waypoints");
    case SolarObject::Weapons_Platform: return tr("Weapons Platforms");
    case SolarObject::Depot: return tr("Depots");
    case SolarObject::Wreck: return tr("Wrecks");
    case SolarObject::Asteroid: return tr("Asteroids");
    case SolarObject::Other: return tr("Other");
    }
    return tr("Other");
}

} // namespace flatlas::editors
