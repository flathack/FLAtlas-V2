#include "SystemSettingsDialog.h"

#include "domain/SystemDocument.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace flatlas::editors {

SystemSettingsDialog::SystemSettingsDialog(const flatlas::domain::SystemDocument *document,
                                           bool hasNonStandardSectionOrder,
                                           QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("System-Einstellungen"));
    setMinimumWidth(460);

    auto *rootLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(document ? document->name().trimmed() : QString(), this);
    form->addRow(tr("System-Nickname:"), m_nicknameEdit);

    m_navMapScaleSpin = new QDoubleSpinBox(this);
    m_navMapScaleSpin->setRange(0.01, 100.0);
    m_navMapScaleSpin->setDecimals(6);
    m_navMapScaleSpin->setSingleStep(0.01);
    m_navMapScaleSpin->setValue(document ? document->navMapScale() : 1.36);
    form->addRow(tr("NavMapScale:"), m_navMapScaleSpin);

    rootLayout->addLayout(form);

    auto *infoLabel = new QLabel(
        hasNonStandardSectionOrder
            ? tr("Die geladene System-INI weicht in der Section-Reihenfolge vom ueblichen Freelancer-Aufbau ab.")
            : tr("Die geladene System-INI entspricht bereits einer standardnahen Section-Reihenfolge."),
        this);
    infoLabel->setWordWrap(true);
    rootLayout->addWidget(infoLabel);

    m_normalizeSectionsCheck = new QCheckBox(tr("Section-Reihenfolge in Datei standardisieren"), this);
    m_normalizeSectionsCheck->setChecked(false);
    rootLayout->addWidget(m_normalizeSectionsCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

QString SystemSettingsDialog::systemNickname() const
{
    return m_nicknameEdit ? m_nicknameEdit->text() : QString();
}

double SystemSettingsDialog::navMapScale() const
{
    return m_navMapScaleSpin ? m_navMapScaleSpin->value() : 1.36;
}

bool SystemSettingsDialog::shouldNormalizeSectionOrder() const
{
    return m_normalizeSectionsCheck && m_normalizeSectionsCheck->isChecked();
}

} // namespace flatlas::editors
