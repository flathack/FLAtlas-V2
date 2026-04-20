#include "JumpConnectionDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>

namespace flatlas::editors {

JumpConnectionDialog::JumpConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Jump Connection"));
    setMinimumWidth(450);
    setupUi();
}

void JumpConnectionDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // --- Type selector ---
    auto *typeLayout = new QHBoxLayout;
    typeLayout->addWidget(new QLabel(tr("Type:")));
    m_typeCombo = new QComboBox;
    m_typeCombo->addItem(tr("Jump Gate"));
    m_typeCombo->addItem(tr("Jump Hole"));
    typeLayout->addWidget(m_typeCombo);
    typeLayout->addStretch();
    mainLayout->addLayout(typeLayout);

    // --- From side ---
    auto *fromGroup = new QGroupBox(tr("From (Source)"));
    auto *fromForm = new QFormLayout(fromGroup);
    m_fromSystem = new QComboBox;
    m_fromSystem->setEditable(true);
    fromForm->addRow(tr("System:"), m_fromSystem);
    m_fromObject = new QLineEdit;
    m_fromObject->setPlaceholderText(tr("e.g. Li01_to_Li02_Gate"));
    fromForm->addRow(tr("Object:"), m_fromObject);
    mainLayout->addWidget(fromGroup);

    // --- To side ---
    auto *toGroup = new QGroupBox(tr("To (Destination)"));
    auto *toForm = new QFormLayout(toGroup);
    m_toSystem = new QComboBox;
    m_toSystem->setEditable(true);
    toForm->addRow(tr("System:"), m_toSystem);
    m_toObject = new QLineEdit;
    m_toObject->setPlaceholderText(tr("e.g. Li02_to_Li01_Gate"));
    toForm->addRow(tr("Object:"), m_toObject);
    mainLayout->addWidget(toGroup);

    // --- Options ---
    m_reverseCheck = new QCheckBox(tr("Auto-create reverse side object"));
    m_reverseCheck->setChecked(true);
    m_reverseCheck->setToolTip(tr("Create the matching object in the destination system"));
    mainLayout->addWidget(m_reverseCheck);

    // --- Status ---
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    mainLayout->addWidget(m_statusLabel);

    // --- Buttons ---
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(m_buttonBox);

    // --- Connections ---
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        validate();
        if (m_statusLabel->text().isEmpty())
            accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_fromSystem, &QComboBox::currentTextChanged,
            this, &JumpConnectionDialog::onSystemChanged);
    connect(m_toSystem, &QComboBox::currentTextChanged,
            this, &JumpConnectionDialog::onSystemChanged);
    connect(m_fromObject, &QLineEdit::textChanged,
            this, [this]() { m_statusLabel->clear(); });
    connect(m_toObject, &QLineEdit::textChanged,
            this, [this]() { m_statusLabel->clear(); });
}

void JumpConnectionDialog::setSystems(const QVector<flatlas::domain::SystemInfo> &systems)
{
    m_systems = systems;
    m_fromSystem->clear();
    m_toSystem->clear();
    for (const auto &s : systems) {
        const QString label = s.displayName.isEmpty()
                                  ? s.nickname
                                  : QStringLiteral("%1 (%2)").arg(s.displayName, s.nickname);
        m_fromSystem->addItem(label, s.nickname);
        m_toSystem->addItem(label, s.nickname);
    }
}

void JumpConnectionDialog::setExistingConnections(
    const QVector<flatlas::domain::JumpConnection> &conns)
{
    m_existing = conns;
}

void JumpConnectionDialog::setConnection(const flatlas::domain::JumpConnection &conn)
{
    // Select from-system
    for (int i = 0; i < m_fromSystem->count(); ++i) {
        if (m_fromSystem->itemData(i).toString() == conn.fromSystem) {
            m_fromSystem->setCurrentIndex(i);
            break;
        }
    }
    m_fromObject->setText(conn.fromObject);

    // Select to-system
    for (int i = 0; i < m_toSystem->count(); ++i) {
        if (m_toSystem->itemData(i).toString() == conn.toSystem) {
            m_toSystem->setCurrentIndex(i);
            break;
        }
    }
    m_toObject->setText(conn.toObject);
}

flatlas::domain::JumpConnection JumpConnectionDialog::connection() const
{
    flatlas::domain::JumpConnection c;
    int fromIdx = m_fromSystem->currentIndex();
    c.fromSystem = (fromIdx >= 0 && fromIdx < m_systems.size())
                       ? m_systems[fromIdx].nickname
                       : m_fromSystem->currentText();
    c.fromObject = m_fromObject->text().trimmed();

    int toIdx = m_toSystem->currentIndex();
    c.toSystem = (toIdx >= 0 && toIdx < m_systems.size())
                     ? m_systems[toIdx].nickname
                     : m_toSystem->currentText();
    c.toObject = m_toObject->text().trimmed();
    return c;
}

bool JumpConnectionDialog::createReverseSide() const
{
    return m_reverseCheck->isChecked();
}

bool JumpConnectionDialog::isJumpGate() const
{
    return m_typeCombo->currentIndex() == 0;
}

void JumpConnectionDialog::onSystemChanged()
{
    m_statusLabel->clear();

    // Auto-generate object nicknames when systems change
    const auto conn = connection();
    if (m_fromObject->text().isEmpty() && !conn.fromSystem.isEmpty() && !conn.toSystem.isEmpty()) {
        const QString suffix = isJumpGate() ? QStringLiteral("Gate") : QStringLiteral("Hole");
        m_fromObject->setText(
            QStringLiteral("%1_to_%2_%3").arg(conn.fromSystem, conn.toSystem, suffix));
    }
    if (m_toObject->text().isEmpty() && !conn.fromSystem.isEmpty() && !conn.toSystem.isEmpty()) {
        const QString suffix = isJumpGate() ? QStringLiteral("Gate") : QStringLiteral("Hole");
        m_toObject->setText(
            QStringLiteral("%1_to_%2_%3").arg(conn.toSystem, conn.fromSystem, suffix));
    }
}

void JumpConnectionDialog::validate()
{
    m_statusLabel->clear();

    const auto c = connection();

    // Required fields
    if (c.fromSystem.isEmpty() || c.toSystem.isEmpty()) {
        m_statusLabel->setText(tr("Both systems must be specified."));
        return;
    }
    if (c.fromObject.isEmpty() || c.toObject.isEmpty()) {
        m_statusLabel->setText(tr("Both object nicknames must be specified."));
        return;
    }

    // No self-connection
    if (c.fromSystem == c.toSystem && c.fromObject == c.toObject) {
        m_statusLabel->setText(tr("Cannot connect an object to itself."));
        return;
    }

    // Duplicate check
    for (const auto &ex : m_existing) {
        if (ex.fromSystem == c.fromSystem && ex.fromObject == c.fromObject
            && ex.toSystem == c.toSystem && ex.toObject == c.toObject) {
            m_statusLabel->setText(tr("This connection already exists."));
            return;
        }
        // Also check reverse
        if (ex.fromSystem == c.toSystem && ex.fromObject == c.toObject
            && ex.toSystem == c.fromSystem && ex.toObject == c.fromObject) {
            m_statusLabel->setText(tr("Reverse of this connection already exists."));
            return;
        }
    }
}

} // namespace flatlas::editors
