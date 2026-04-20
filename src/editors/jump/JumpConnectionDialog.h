#pragma once
// editors/jump/JumpConnectionDialog.h – Create/edit jump connections between systems
//
// Freelancer jump gates/holes are paired objects in two systems that reference
// each other via goto = <system>, <object>.  This dialog manages both sides.

#include <QDialog>
#include "domain/UniverseData.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QDialogButtonBox;

namespace flatlas::editors {

class JumpConnectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit JumpConnectionDialog(QWidget *parent = nullptr);

    /// Set available systems for the combo boxes.
    void setSystems(const QVector<flatlas::domain::SystemInfo> &systems);

    /// Set existing connections for duplicate validation.
    void setExistingConnections(const QVector<flatlas::domain::JumpConnection> &conns);

    /// Pre-fill the dialog for editing an existing connection.
    void setConnection(const flatlas::domain::JumpConnection &conn);

    /// Return the configured connection.
    flatlas::domain::JumpConnection connection() const;

    /// Whether to auto-create the reverse side object.
    bool createReverseSide() const;

    /// Whether the user selected JumpGate (true) or JumpHole (false).
    bool isJumpGate() const;

private:
    void setupUi();
    void validate();
    void onSystemChanged();

    QComboBox        *m_fromSystem    = nullptr;
    QLineEdit        *m_fromObject    = nullptr;
    QComboBox        *m_toSystem      = nullptr;
    QLineEdit        *m_toObject      = nullptr;
    QComboBox        *m_typeCombo     = nullptr;
    QCheckBox        *m_reverseCheck  = nullptr;
    QLabel           *m_statusLabel   = nullptr;
    QDialogButtonBox *m_buttonBox     = nullptr;

    QVector<flatlas::domain::SystemInfo>      m_systems;
    QVector<flatlas::domain::JumpConnection>  m_existing;
};

} // namespace flatlas::editors
