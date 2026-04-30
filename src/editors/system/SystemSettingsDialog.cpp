#include "SystemSettingsDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace flatlas::editors {

namespace {

QComboBox *createEditableCombo(const QStringList &options, const QString &currentText, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setEditable(true);
    combo->addItems(options);
    combo->setCurrentText(currentText.trimmed());
    return combo;
}

QWidget *createColorEditor(QLineEdit **outEdit, QPushButton **outButton, const QString &value, QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *edit = new QLineEdit(value.trimmed(), container);
    auto *button = new QPushButton(QObject::tr("Farbe waehlen"), container);
    layout->addWidget(edit, 1);
    layout->addWidget(button);

    if (outEdit)
        *outEdit = edit;
    if (outButton)
        *outButton = button;
    return container;
}

} // namespace

SystemSettingsDialog::SystemSettingsDialog(const SystemSettingsState &current,
                                           const SystemSettingsOptions &options,
                                           bool hasNonStandardSectionOrder,
                                           QWidget *parent)
    : QDialog(parent)
    , m_systemNickname(current.systemNickname.trimmed())
{
    setWindowTitle(tr("System-Einstellungen"));
    setMinimumWidth(560);

    auto *rootLayout = new QVBoxLayout(this);
    m_systemLabel = new QLabel(
        tr("Bearbeitet die V1-System-Metadaten fuer %1. Universe-Werte wie Visit oder NavMapScale bleiben im Universe-Editor.")
            .arg(m_systemNickname.isEmpty() ? tr("das aktuelle System") : m_systemNickname),
        this);
    m_systemLabel->setWordWrap(true);
    rootLayout->addWidget(m_systemLabel);

    auto *audioGroup = new QGroupBox(tr("Audio"), this);
    auto *audioForm = new QFormLayout(audioGroup);
    audioForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_musicSpaceCombo = createEditableCombo(options.musicSpaceOptions, current.musicSpace, audioGroup);
    audioForm->addRow(tr("Music Space:"), m_musicSpaceCombo);
    m_musicDangerCombo = createEditableCombo(options.musicDangerOptions, current.musicDanger, audioGroup);
    audioForm->addRow(tr("Music Danger:"), m_musicDangerCombo);
    m_musicBattleCombo = createEditableCombo(options.musicBattleOptions, current.musicBattle, audioGroup);
    audioForm->addRow(tr("Music Battle:"), m_musicBattleCombo);
    rootLayout->addWidget(audioGroup);

    auto *environmentGroup = new QGroupBox(tr("Umgebung"), this);
    auto *environmentForm = new QFormLayout(environmentGroup);
    environmentForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    environmentForm->addRow(tr("Space Color:"),
                            createColorEditor(&m_spaceColorEdit, &m_spaceColorButton, current.spaceColor, environmentGroup));
    m_localFactionCombo = createEditableCombo(options.factionDisplayOptions,
                                              SystemSettingsService::factionDisplayForNickname(current.localFaction,
                                                                                               options.factionDisplayOptions),
                                              environmentGroup);
    environmentForm->addRow(tr("Local Faction:"), m_localFactionCombo);
    environmentForm->addRow(tr("Ambient Color:"),
                            createColorEditor(&m_ambientColorEdit, &m_ambientColorButton, current.ambientColor, environmentGroup));
    m_dustCombo = createEditableCombo(options.dustOptions, current.dust, environmentGroup);
    environmentForm->addRow(tr("Dust:"), m_dustCombo);
    rootLayout->addWidget(environmentGroup);

    auto *backgroundGroup = new QGroupBox(tr("Background"), this);
    auto *backgroundForm = new QFormLayout(backgroundGroup);
    backgroundForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_backgroundBasicCombo = createEditableCombo(options.backgroundBasicStarsOptions, current.backgroundBasicStars, backgroundGroup);
    backgroundForm->addRow(tr("Basic Stars:"), m_backgroundBasicCombo);
    m_backgroundComplexCombo = createEditableCombo(options.backgroundComplexStarsOptions, current.backgroundComplexStars, backgroundGroup);
    backgroundForm->addRow(tr("Complex Stars:"), m_backgroundComplexCombo);
    m_backgroundNebulaeCombo = createEditableCombo(options.backgroundNebulaeOptions, current.backgroundNebulae, backgroundGroup);
    backgroundForm->addRow(tr("Nebulae:"), m_backgroundNebulaeCombo);
    rootLayout->addWidget(backgroundGroup);

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

    connect(m_spaceColorButton, &QPushButton::clicked, this, [this]() { pickColor(m_spaceColorEdit); });
    connect(m_ambientColorButton, &QPushButton::clicked, this, [this]() { pickColor(m_ambientColorEdit); });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

SystemSettingsState SystemSettingsDialog::result() const
{
    SystemSettingsState state;
    state.systemNickname = m_systemNickname;
    state.musicSpace = m_musicSpaceCombo ? m_musicSpaceCombo->currentText().trimmed() : QString();
    state.musicDanger = m_musicDangerCombo ? m_musicDangerCombo->currentText().trimmed() : QString();
    state.musicBattle = m_musicBattleCombo ? m_musicBattleCombo->currentText().trimmed() : QString();
    state.spaceColor = m_spaceColorEdit ? m_spaceColorEdit->text().trimmed() : QString();
    state.localFaction = m_localFactionCombo ? m_localFactionCombo->currentText().trimmed() : QString();
    state.ambientColor = m_ambientColorEdit ? m_ambientColorEdit->text().trimmed() : QString();
    state.dust = m_dustCombo ? m_dustCombo->currentText().trimmed() : QString();
    state.backgroundBasicStars = m_backgroundBasicCombo ? m_backgroundBasicCombo->currentText().trimmed() : QString();
    state.backgroundComplexStars = m_backgroundComplexCombo ? m_backgroundComplexCombo->currentText().trimmed() : QString();
    state.backgroundNebulae = m_backgroundNebulaeCombo ? m_backgroundNebulaeCombo->currentText().trimmed() : QString();
    return state;
}

bool SystemSettingsDialog::shouldNormalizeSectionOrder() const
{
    return m_normalizeSectionsCheck && m_normalizeSectionsCheck->isChecked();
}

void SystemSettingsDialog::accept()
{
    const SystemSettingsState state = result();
    QString normalized;
    QString errorMessage;
    if (!SystemSettingsService::normalizeRgbText(state.spaceColor, &normalized, &errorMessage)
        || !SystemSettingsService::normalizeRgbText(state.ambientColor, &normalized, &errorMessage)) {
        QMessageBox::warning(this,
                             tr("System-Einstellungen"),
                             errorMessage.trimmed().isEmpty()
                                 ? tr("Mindestens ein Farbwert ist ungueltig.")
                                 : errorMessage);
        return;
    }
    QDialog::accept();
}

void SystemSettingsDialog::pickColor(QLineEdit *edit)
{
    if (!edit)
        return;

    QString normalized;
    QColor initialColor;
    if (SystemSettingsService::normalizeRgbText(edit->text(), &normalized)) {
        const QStringList parts = normalized.split(QStringLiteral(", "));
        if (parts.size() == 3)
            initialColor = QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
    }

    const QColor color = QColorDialog::getColor(initialColor, this, tr("Farbe auswaehlen"));
    if (!color.isValid())
        return;
    edit->setText(QStringLiteral("%1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue()));
}

} // namespace flatlas::editors
