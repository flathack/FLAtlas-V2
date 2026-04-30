#include "NewSystemDialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace flatlas::editors {

NewSystemDialog::NewSystemDialog(const NewSystemDialogOptions &options, QWidget *parent)
    : QDialog(parent)
    , m_options(options)
{
    setWindowTitle(tr("Neues System erstellen"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("z. B. Taharka"));
    form->addRow(tr("System Name:"), m_nameEdit);

    m_prefixEdit = new QLineEdit(this);
    m_prefixEdit->setPlaceholderText(tr("z. B. Li"));
    m_prefixEdit->setMaxLength(4);
    form->addRow(tr("System Prefix:"), m_prefixEdit);

    m_sizeSpin = new QSpinBox(this);
    m_sizeSpin->setRange(1000, 10000000);
    m_sizeSpin->setValue(100000);
    form->addRow(tr("Systemgröße:"), m_sizeSpin);

    form->addRow(tr("Space Color:"),
                 createColorRow(m_spaceColorLabel, QStringLiteral("0, 0, 0"), tr("Space Color")));

    m_musicSpaceCombo = createEditableCombo(options.musicSpaceOptions, QStringLiteral("music_br_space"));
    form->addRow(tr("Music Space:"), m_musicSpaceCombo);

    m_musicDangerCombo = createEditableCombo(options.musicDangerOptions, QStringLiteral("music_br_danger"));
    form->addRow(tr("Music Danger:"), m_musicDangerCombo);

    m_musicBattleCombo = createEditableCombo(options.musicBattleOptions, QStringLiteral("music_br_battle"));
    form->addRow(tr("Music Battle:"), m_musicBattleCombo);

    form->addRow(tr("Ambient Color:"),
                 createColorRow(m_ambientColorLabel, QStringLiteral("60, 20, 10"), tr("Ambient Color")));

    m_basicStarsCombo = createEditableCombo(options.basicStarsOptions,
                                            QStringLiteral("solar\\starsphere\\starsphere_stars_basic.cmp"));
    form->addRow(tr("Basic Stars:"), m_basicStarsCombo);

    m_complexStarsCombo = createEditableCombo(options.complexStarsOptions,
                                              QStringLiteral("solar\\starsphere\\starsphere_br01_stars.cmp"));
    form->addRow(tr("Complex Stars:"), m_complexStarsCombo);

    m_nebulaeCombo = createEditableCombo(options.nebulaeOptions,
                                         QStringLiteral("solar\\starsphere\\starsphere_br01.cmp"));
    form->addRow(tr("Nebulae:"), m_nebulaeCombo);

    form->addRow(tr("Light Source Color:"),
                 createColorRow(m_lightColorLabel, QStringLiteral("253, 230, 180"), tr("Light Source Color")));

    m_localFactionCombo = new QComboBox(this);
    m_localFactionCombo->setEditable(false);
    for (const auto &option : options.factionOptions)
        m_localFactionCombo->addItem(option.displayText, option.nickname);
    const int liIndex = m_localFactionCombo->findData(QStringLiteral("li_n_grp"));
    if (liIndex >= 0)
        m_localFactionCombo->setCurrentIndex(liIndex);
    form->addRow(tr("Local Faction:"), m_localFactionCombo);

    m_sectorCombo = new QComboBox(this);
    for (const auto &sector : options.sectors) {
        const QString display = sector.displayName.trimmed().isEmpty() ? sector.key : sector.displayName;
        m_sectorCombo->addItem(display, sector.key);
    }
    if (m_sectorCombo->count() == 0)
        m_sectorCombo->addItem(QStringLiteral("Sirius"), QStringLiteral("universe"));
    const int sectorIndex = m_sectorCombo->findData(options.activeSectorKey.trimmed().isEmpty()
                                                       ? QStringLiteral("universe")
                                                       : options.activeSectorKey.trimmed());
    if (sectorIndex >= 0)
        m_sectorCombo->setCurrentIndex(sectorIndex);
    form->addRow(tr("Sektor:"), m_sectorCombo);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

NewSystemRequest NewSystemDialog::request() const
{
    NewSystemRequest req;
    req.systemName = m_nameEdit->text().trimmed();
    req.systemPrefix = m_prefixEdit->text().trimmed().toUpper();
    req.systemSize = m_sizeSpin->value();
    req.spaceColor = m_spaceColorLabel->text().trimmed();
    req.musicSpace = m_musicSpaceCombo->currentText().trimmed();
    req.musicDanger = m_musicDangerCombo->currentText().trimmed();
    req.musicBattle = m_musicBattleCombo->currentText().trimmed();
    req.ambientColor = m_ambientColorLabel->text().trimmed();
    req.basicStars = m_basicStarsCombo->currentText().trimmed();
    req.complexStars = m_complexStarsCombo->currentText().trimmed();
    req.nebulae = m_nebulaeCombo->currentText().trimmed();
    req.lightSourceColor = m_lightColorLabel->text().trimmed();
    req.localFactionNickname = m_localFactionCombo->currentData().toString().trimmed();
    req.localFactionDisplay = m_localFactionCombo->currentText().trimmed();
    req.sectorKey = m_sectorCombo->currentData().toString().trimmed();
    if (req.sectorKey.isEmpty())
        req.sectorKey = QStringLiteral("universe");
    return req;
}

QComboBox *NewSystemDialog::createEditableCombo(const QStringList &items, const QString &defaultValue) const
{
    auto *combo = new QComboBox(const_cast<NewSystemDialog *>(this));
    combo->setEditable(true);
    combo->addItems(items);
    combo->setCurrentText(defaultValue);
    return combo;
}

QWidget *NewSystemDialog::createColorRow(QLabel *&valueLabel, const QString &initialValue,
                                         const QString &dialogTitle)
{
    auto *row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    auto *button = new QPushButton(tr("Farbe wählen"), row);
    valueLabel = new QLabel(initialValue, row);

    connect(button, &QPushButton::clicked, this, [this, valueLabel, dialogTitle]() {
        const QColor picked = QColorDialog::getColor(Qt::white, this, dialogTitle);
        if (!picked.isValid())
            return;
        valueLabel->setText(QStringLiteral("%1, %2, %3")
                                .arg(picked.red())
                                .arg(picked.green())
                                .arg(picked.blue()));
    });

    rowLayout->addWidget(button);
    rowLayout->addWidget(valueLabel, 1);
    return row;
}

} // namespace flatlas::editors
