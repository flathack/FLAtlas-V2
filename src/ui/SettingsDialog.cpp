#include "SettingsDialog.h"
#include "core/Config.h"
#include "core/Theme.h"
#include "core/I18n.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDialogButtonBox>

namespace flatlas::ui {

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(520, 380);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Appearance ---
    auto *appearanceGroup = new QGroupBox(tr("Appearance"), this);
    auto *appearanceForm = new QFormLayout(appearanceGroup);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItems(flatlas::core::Theme::instance().availableThemes());
    m_themeCombo->setCurrentText(flatlas::core::Theme::instance().currentTheme());
    appearanceForm->addRow(tr("Theme:"), m_themeCombo);

    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItems(flatlas::core::I18n::availableLanguages());
    m_languageCombo->setCurrentText(flatlas::core::I18n::instance().currentLanguage());
    appearanceForm->addRow(tr("Language:"), m_languageCombo);

    mainLayout->addWidget(appearanceGroup);

    // --- Paths ---
    auto *pathsGroup = new QGroupBox(tr("Freelancer"), this);
    auto *pathsForm = new QFormLayout(pathsGroup);

    auto *pathRow = new QHBoxLayout;
    m_gamePathEdit = new QLineEdit(this);
    m_gamePathEdit->setText(flatlas::core::Config::instance().getString("gamePath"));
    m_gamePathEdit->setPlaceholderText(tr("Path to Freelancer installation..."));
    pathRow->addWidget(m_gamePathEdit);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    pathRow->addWidget(m_browseButton);
    pathsForm->addRow(tr("Game Path:"), pathRow);

    mainLayout->addWidget(pathsGroup);

    mainLayout->addStretch();

    // --- Buttons ---
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    // --- Connections ---
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowsePath);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        auto &config = flatlas::core::Config::instance();
        
        // Apply theme
        QString theme = m_themeCombo->currentText();
        flatlas::core::Theme::instance().apply(theme);
        config.setString("theme", theme);
        
        // Apply language
        QString lang = m_languageCombo->currentText();
        flatlas::core::I18n::instance().setLanguage(lang);
        config.setString("language", lang);
        
        // Save game path
        config.setString("gamePath", m_gamePathEdit->text());
        
        config.save();
        accept();
    });

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::onBrowsePath()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Freelancer Installation"), m_gamePathEdit->text());
    if (!dir.isEmpty())
        m_gamePathEdit->setText(dir);
}

} // namespace flatlas::ui
