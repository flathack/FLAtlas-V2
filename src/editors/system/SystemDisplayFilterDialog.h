#pragma once

#include "rendering/view2d/SystemDisplayFilter.h"

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace flatlas::editors {

class SystemDisplayFilterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SystemDisplayFilterDialog(const flatlas::rendering::SystemDisplayFilterSettings &settings,
                                       QWidget *parent = nullptr);

    flatlas::rendering::SystemDisplayFilterSettings settings() const;

private:
    void buildUi();
    void populateQuickToggles();
    void populateRulesTable();
    void loadRuleIntoEditor(int row);
    flatlas::rendering::SystemDisplayFilterRule buildRuleFromEditor() const;
    void clearRuleEditor();
    void saveRuleFromEditor();
    QString displayNameForType(flatlas::domain::SolarObject::Type type) const;

    flatlas::rendering::SystemDisplayFilterSettings m_settings;
    QTableWidget *m_rulesTable = nullptr;
    QLineEdit *m_ruleNameEdit = nullptr;
    QCheckBox *m_ruleEnabledCheck = nullptr;
    QComboBox *m_targetCombo = nullptr;
    QComboBox *m_fieldCombo = nullptr;
    QComboBox *m_matchModeCombo = nullptr;
    QComboBox *m_actionCombo = nullptr;
    QLineEdit *m_patternEdit = nullptr;
    QPushButton *m_newRuleButton = nullptr;
    QPushButton *m_saveRuleButton = nullptr;
    QPushButton *m_deleteRuleButton = nullptr;
    QMap<int, QCheckBox *> m_objectTypeChecks;
    QMap<int, QCheckBox *> m_labelTypeChecks;
    QString m_editingRuleId;
};

} // namespace flatlas::editors
