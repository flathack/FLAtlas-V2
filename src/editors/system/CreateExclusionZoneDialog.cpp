#include "CreateExclusionZoneDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

namespace flatlas::editors {

CreateExclusionZoneDialog::CreateExclusionZoneDialog(const QString &nicknameSuggestion,
                                                     const QString &fieldZoneNickname,
                                                     const QString &fieldFileRelativePath,
                                                     bool supportsShell,
                                                     const QStringList &shellOptions,
                                                     QWidget *parent)
    : QDialog(parent)
    , m_fieldZoneNickname(fieldZoneNickname)
    , m_supportsShell(supportsShell)
{
    setWindowTitle(tr("Exclusion Zone hinzufuegen"));
    setMinimumWidth(560);

    auto *rootLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(nicknameSuggestion, this);
    form->addRow(tr("Nickname:"), m_nicknameEdit);

    m_fieldZoneEdit = new QLineEdit(fieldZoneNickname, this);
    m_fieldZoneEdit->setReadOnly(true);
    form->addRow(tr("Feld-Zone:"), m_fieldZoneEdit);

    m_fieldFileEdit = new QLineEdit(fieldFileRelativePath, this);
    m_fieldFileEdit->setReadOnly(true);
    form->addRow(tr("Referenzdatei:"), m_fieldFileEdit);

    m_shapeCombo = new QComboBox(this);
    m_shapeCombo->addItems({QStringLiteral("SPHERE"),
                            QStringLiteral("ELLIPSOID"),
                            QStringLiteral("BOX"),
                            QStringLiteral("CYLINDER")});
    form->addRow(tr("Shape:"), m_shapeCombo);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("(optional)"));
    form->addRow(tr("Comment:"), m_commentEdit);

    m_sortSpin = new QSpinBox(this);
    m_sortSpin->setRange(0, 999);
    m_sortSpin->setValue(99);
    form->addRow(tr("Sort:"), m_sortSpin);

    m_linkCheck = new QCheckBox(tr("Mit ausgewaehlter Feld-Zone verknuepfen"), this);
    m_linkCheck->setChecked(true);
    form->addRow(QString(), m_linkCheck);

    m_shellEnabledCheck = new QCheckBox(tr("Optical Shell"), this);
    m_shellEnabledCheck->setChecked(false);
    m_shellEnabledCheck->setEnabled(m_supportsShell);
    form->addRow(QString(), m_shellEnabledCheck);

    m_shellFogFarSpin = new QSpinBox(this);
    m_shellFogFarSpin->setRange(0, 50000);
    m_shellFogFarSpin->setValue(8000);
    form->addRow(tr("Fog Far:"), m_shellFogFarSpin);

    m_shellPathCombo = new QComboBox(this);
    m_shellPathCombo->setEditable(true);
    m_shellPathCombo->setInsertPolicy(QComboBox::NoInsert);
    for (const QString &option : shellOptions)
        m_shellPathCombo->addItem(option, option);
    if (m_shellPathCombo->count() == 0)
        m_shellPathCombo->addItem(QStringLiteral("solar\\nebula\\generic_exclusion.3db"));
    m_shellPathCombo->setCurrentText(QStringLiteral("solar\\nebula\\generic_exclusion.3db"));
    configureSearchCompleter(m_shellPathCombo);
    form->addRow(tr("Shell Mesh:"), m_shellPathCombo);

    m_shellScalarSpin = new QDoubleSpinBox(this);
    m_shellScalarSpin->setRange(0.1, 5.0);
    m_shellScalarSpin->setSingleStep(0.1);
    m_shellScalarSpin->setDecimals(2);
    m_shellScalarSpin->setValue(1.0);
    form->addRow(tr("Shell Scalar:"), m_shellScalarSpin);

    m_shellMaxAlphaSpin = new QDoubleSpinBox(this);
    m_shellMaxAlphaSpin->setRange(0.0, 1.0);
    m_shellMaxAlphaSpin->setSingleStep(0.05);
    m_shellMaxAlphaSpin->setDecimals(2);
    m_shellMaxAlphaSpin->setValue(0.5);
    form->addRow(tr("Max Alpha:"), m_shellMaxAlphaSpin);

    m_shellTintEdit = new QLineEdit(QStringLiteral("40, 120, 120"), this);
    m_shellTintEdit->setPlaceholderText(QStringLiteral("R, G, B"));
    form->addRow(tr("Exclusion Tint:"), m_shellTintEdit);

    rootLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Erstellen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);

    connect(m_shellEnabledCheck, &QCheckBox::toggled,
            this, &CreateExclusionZoneDialog::syncShellControls);
    syncShellControls();
}

void CreateExclusionZoneDialog::accept()
{
    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Der Nickname darf nicht leer sein."));
        return;
    }

    static const QRegularExpression validNickname(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!validNickname.match(nickname).hasMatch()) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Der Nickname darf nur Buchstaben, Zahlen und Unterstriche enthalten."));
        return;
    }

    if (m_linkCheck->isChecked() && m_fieldFileEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Es ist keine gueltige Feld-Referenzdatei ausgewaehlt."));
        return;
    }

    if (m_supportsShell && m_shellEnabledCheck->isChecked() && m_shellPathCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Exclusion Zone"),
                             tr("Bitte waehle ein Shell-Mesh aus oder deaktiviere Optical Shell."));
        return;
    }

    m_result.nickname = nickname;
    m_result.fieldZoneNickname = m_fieldZoneNickname;
    m_result.fieldFileRelativePath = m_fieldFileEdit->text().trimmed();
    m_result.shape = m_shapeCombo->currentText().trimmed().toUpper();
    m_result.comment = m_commentEdit->text().trimmed();
    m_result.sort = m_sortSpin->value();
    m_result.linkToFieldZone = m_linkCheck->isChecked();
    m_result.shellSettings.enabled = m_supportsShell && m_shellEnabledCheck->isChecked();
    m_result.shellSettings.fogFar = m_shellFogFarSpin->value();
    m_result.shellSettings.zoneShell = m_shellPathCombo->currentText().trimmed();
    m_result.shellSettings.shellScalar = m_shellScalarSpin->value();
    m_result.shellSettings.maxAlpha = m_shellMaxAlphaSpin->value();
    m_result.shellSettings.exclusionTint = m_shellTintEdit->text().trimmed();
    QDialog::accept();
}

void CreateExclusionZoneDialog::syncShellControls()
{
    const bool enabled = m_supportsShell && m_shellEnabledCheck->isChecked();
    m_shellFogFarSpin->setEnabled(enabled);
    m_shellPathCombo->setEnabled(enabled);
    m_shellScalarSpin->setEnabled(enabled);
    m_shellMaxAlphaSpin->setEnabled(enabled);
    m_shellTintEdit->setEnabled(enabled);
}

void CreateExclusionZoneDialog::configureSearchCompleter(QComboBox *combo)
{
    if (!combo)
        return;
    auto *completer = new QCompleter(combo->model(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    combo->setCompleter(completer);
}

} // namespace flatlas::editors
