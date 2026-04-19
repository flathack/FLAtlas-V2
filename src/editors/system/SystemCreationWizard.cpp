#include "SystemCreationWizard.h"
#include "../../domain/SystemDocument.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QWizardPage>

namespace flatlas::editors {

SystemCreationWizard::SystemCreationWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("New System"));
    setMinimumSize(400, 250);

    auto *page = new QWizardPage(this);
    page->setTitle(tr("System Properties"));

    auto *layout = new QFormLayout(page);

    m_nicknameEdit = new QLineEdit(page);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("li01"));
    layout->addRow(tr("System Nickname:"), m_nicknameEdit);

    m_displayNameEdit = new QLineEdit(page);
    m_displayNameEdit->setPlaceholderText(QStringLiteral("New York"));
    layout->addRow(tr("Display Name:"), m_displayNameEdit);

    m_regionCombo = new QComboBox(page);
    m_regionCombo->addItems({
        QStringLiteral("Liberty"),
        QStringLiteral("Bretonia"),
        QStringLiteral("Kusari"),
        QStringLiteral("Rheinland"),
        QStringLiteral("Border Worlds"),
        QStringLiteral("Independent"),
        QStringLiteral("Unknown")
    });
    layout->addRow(tr("Region:"), m_regionCombo);

    addPage(page);
}

std::unique_ptr<flatlas::domain::SystemDocument> SystemCreationWizard::createDocument() const
{
    auto doc = std::make_unique<flatlas::domain::SystemDocument>();
    doc->setName(systemDisplayName());
    doc->setProperty("nickname", systemNickname());
    doc->setDirty(true);
    return doc;
}

QString SystemCreationWizard::systemNickname() const
{
    return m_nicknameEdit->text().trimmed();
}

QString SystemCreationWizard::systemDisplayName() const
{
    return m_displayNameEdit->text().trimmed();
}

}
