#include "CreateFieldZoneDialog.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "domain/ZoneItem.h"
#include "infrastructure/parser/IniParser.h"

#include <QComboBox>
#include <QCheckBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStringListModel>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

namespace {

using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString primaryGamePath()
{
    return flatlas::core::EditingContext::instance().primaryGamePath();
}

QString dataDirFor(const QString &gamePath)
{
    if (gamePath.isEmpty())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
}

QStringList listIniFilesInDirectory(const QString &directoryPath)
{
    if (directoryPath.isEmpty())
        return {};

    QDir dir(directoryPath);
    if (!dir.exists())
        return {};

    QStringList files = dir.entryList({QStringLiteral("*.ini")}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    files.removeDuplicates();
    return files;
}

void appendUniqueSorted(QStringList &target, const QString &value)
{
    if (value.trimmed().isEmpty())
        return;

    for (const QString &existing : std::as_const(target)) {
        if (existing.compare(value, Qt::CaseInsensitive) == 0)
            return;
    }
    target.append(value);
}

QVector<CreateFieldZoneDialog::ReferenceEntry> collectReferenceEntries(const QString &solarDirName)
{
    QVector<CreateFieldZoneDialog::ReferenceEntry> entries;
    const QString dataDir = dataDirFor(primaryGamePath());
    if (dataDir.isEmpty())
        return entries;

    const QString solarRoot = flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR"));
    if (solarRoot.isEmpty())
        return entries;

    const QStringList variants = {solarDirName.toLower(), solarDirName.toUpper()};
    QSet<QString> seen;

    for (const QString &variant : variants) {
        const QString dirPath = flatlas::core::PathUtils::ciResolvePath(solarRoot, variant);
        for (const QString &fileName : listIniFilesInDirectory(dirPath)) {
            const QString key = fileName.toLower();
            if (seen.contains(key))
                continue;
            seen.insert(key);

            CreateFieldZoneDialog::ReferenceEntry entry;
            entry.fileName = fileName;
            entry.absolutePath = QDir(dirPath).absoluteFilePath(fileName);
            entries.append(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.fileName.compare(rhs.fileName, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

QStringList collectZoneMusicOptions()
{
    QStringList options = {
        QStringLiteral("zone_field_asteroid_rock"),
        QStringLiteral("zone_field_asteroid_ice"),
        QStringLiteral("zone_field_asteroid_mine"),
        QStringLiteral("zone_field_asteroid_lava"),
        QStringLiteral("zone_field_asteroid_nomad"),
        QStringLiteral("zone_field_debris"),
        QStringLiteral("zone_field_mine"),
        QStringLiteral("zone_field_ice"),
        QStringLiteral("zone_badlands"),
        QStringLiteral("zone_nebula_crow"),
        QStringLiteral("zone_nebula_barrier"),
        QStringLiteral("zone_nebula_walker"),
        QStringLiteral("zone_nebula_dmatter"),
        QStringLiteral("zone_nebula_nomad"),
        QStringLiteral("zone_nebula_edge"),
    };

    const QString dataDir = dataDirFor(primaryGamePath());
    if (dataDir.isEmpty())
        return options;

    const QString ambiencePath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("AUDIO/ambience_sounds.ini"));
    if (ambiencePath.isEmpty())
        return options;

    const IniDocument doc = IniParser::parseFile(ambiencePath);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Sound"), Qt::CaseInsensitive) != 0)
            continue;
        const QString nickname = section.value(QStringLiteral("nickname")).trimmed();
        if (nickname.startsWith(QStringLiteral("zone_"), Qt::CaseInsensitive))
            appendUniqueSorted(options, nickname);
    }

    std::sort(options.begin(), options.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return options;
}

QStringList collectSpaceDustOptions()
{
    QStringList options = {
        QStringLiteral("icedust"),
        QStringLiteral("leedsdust"),
        QStringLiteral("asteroiddust"),
        QStringLiteral("debrisdust"),
        QStringLiteral("atmosphere_gray"),
        QStringLiteral("radioactivedust"),
        QStringLiteral("radioactivedust_red"),
        QStringLiteral("lavaashdust"),
        QStringLiteral("snowdust"),
        QStringLiteral("waterdropdust"),
        QStringLiteral("touristdust"),
        QStringLiteral("touristdust_pink"),
        QStringLiteral("touristdust_sienna"),
        QStringLiteral("organismdust"),
        QStringLiteral("attractdust"),
        QStringLiteral("attractdust_orange"),
        QStringLiteral("attractdust_green"),
        QStringLiteral("attractdust_purple"),
        QStringLiteral("oxygendust"),
        QStringLiteral("radioactivedust_blue"),
        QStringLiteral("golddust"),
        QStringLiteral("special_attractdust"),
    };

    const QString dataDir = dataDirFor(primaryGamePath());
    if (dataDir.isEmpty())
        return options;

    const QString effectsPath =
        flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("FX/effects.ini"));
    if (effectsPath.isEmpty())
        return options;

    const IniDocument doc = IniParser::parseFile(effectsPath);
    for (const IniSection &section : doc) {
        if (section.name.compare(QStringLiteral("Effect"), Qt::CaseInsensitive) != 0)
            continue;
        const QString effectType = section.value(QStringLiteral("effect_type")).trimmed();
        if (effectType.compare(QStringLiteral("EFT_MISC_DUST"), Qt::CaseInsensitive) != 0)
            continue;
        appendUniqueSorted(options, section.value(QStringLiteral("nickname")).trimmed());
    }

    std::sort(options.begin(), options.end(), [](const QString &lhs, const QString &rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return options;
}

QVector<CreateFieldZoneDialog::NumericOption> asteroidPropertyFlags()
{
    return {
        {QStringLiteral("66 - Standard rock field"), 66},
        {QStringLiteral("65 - Standard ice field"), 65},
        {QStringLiteral("130 - Standard debris field"), 130},
        {QStringLiteral("129 - Debris / hidden helper variant"), 129},
        {QStringLiteral("4128 - Minefield / dangerous field variant"), 4128},
        {QStringLiteral("16466 - Gas pocket / special field variant"), 16466},
        {QStringLiteral("131072 - Hidden / helper zone"), 131072},
        {QStringLiteral("0 - No flag"), 0},
    };
}

QVector<CreateFieldZoneDialog::NumericOption> nebulaPropertyFlags()
{
    return {
        {QStringLiteral("32768 - Standard nebula flag"), 32768},
        {QStringLiteral("49152 - Vanilla variant"), 49152},
        {QStringLiteral("16384 - Rare vanilla variant"), 16384},
        {QStringLiteral("0 - No flag"), 0},
    };
}

QVector<CreateFieldZoneDialog::NumericOption> asteroidVisitFlags()
{
    return {
        {QStringLiteral("32 - Standard asteroid field"), 32},
        {QStringLiteral("36 - Standard variant, often debris / alt field"), 36},
        {QStringLiteral("128 - Hidden / helper zone"), 128},
        {QStringLiteral("0 - No special visit flags"), 0},
    };
}

QVector<CreateFieldZoneDialog::NumericOption> nebulaVisitFlags()
{
    return {
        {QStringLiteral("32 - Standard nebula"), 32},
        {QStringLiteral("36 - Vanilla variant"), 36},
        {QStringLiteral("0 - No special visit flags"), 0},
        {QStringLiteral("128 - Hidden / helper zone"), 128},
    };
}

QString extractLeadingIntegerToken(const QString &value)
{
    static const QRegularExpression numericPrefix(QStringLiteral("^\\s*([+-]?\\d+)"));
    const QRegularExpressionMatch match = numericPrefix.match(value);
    return match.hasMatch() ? match.captured(1) : QString();
}

} // namespace

CreateFieldZoneDialog::CreateFieldZoneDialog(flatlas::domain::SystemDocument *document, QWidget *parent)
    : QDialog(parent)
    , m_document(document)
{
    setWindowTitle(tr("Zone erstellen"));
    setMinimumWidth(620);

    if (m_document)
        m_systemToken = normalizedNameToken(m_document->name());
    if (m_systemToken.isEmpty())
        m_systemToken = QStringLiteral("system");

    auto *rootLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Asteroid"), static_cast<int>(CreateFieldZoneResult::Type::Asteroid));
    m_typeCombo->addItem(tr("Nebula"), static_cast<int>(CreateFieldZoneResult::Type::Nebula));
    form->addRow(tr("Typ:"), m_typeCombo);

    m_nicknameEdit = new QLineEdit(this);
    form->addRow(tr("Zonenname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(this);
    m_ingameNameEdit->setPlaceholderText(tr("Ingame Name (optional)"));
    form->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_referenceCombo = new QComboBox(this);
    m_referenceCombo->setEditable(true);
    m_referenceCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Referenzdatei:"), m_referenceCombo);

    m_musicCombo = new QComboBox(this);
    m_musicCombo->setEditable(true);
    m_musicCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Music:"), m_musicCombo);

    m_damageSpin = new QSpinBox(this);
    m_damageSpin->setRange(0, 2000000);
    form->addRow(tr("Damage:"), m_damageSpin);

    m_propertyFlagsCombo = new QComboBox(this);
    m_propertyFlagsCombo->setEditable(true);
    m_propertyFlagsCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Property Flags:"), m_propertyFlagsCombo);

    m_visitCombo = new QComboBox(this);
    m_visitCombo->setEditable(true);
    m_visitCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Visit:"), m_visitCombo);

    m_sortSpin = new QSpinBox(this);
    m_sortSpin->setRange(0, 999);
    m_sortSpin->setValue(99);
    form->addRow(tr("Sort:"), m_sortSpin);

    m_interferenceSpin = new QDoubleSpinBox(this);
    m_interferenceSpin->setRange(0.0, 1.0);
    m_interferenceSpin->setDecimals(2);
    m_interferenceSpin->setSingleStep(0.05);
    m_interferenceSpin->setValue(0.0);
    m_interferenceSpin->setEnabled(false);

    auto *interferenceHost = new QWidget(this);
    auto *interferenceLayout = new QHBoxLayout(interferenceHost);
    interferenceLayout->setContentsMargins(0, 0, 0, 0);
    interferenceLayout->setSpacing(8);

    m_interferenceCheck = new QCheckBox(tr("setzen"), interferenceHost);
    interferenceLayout->addWidget(m_interferenceCheck);
    interferenceLayout->addWidget(m_interferenceSpin, 1);
    form->addRow(tr("Interference:"), interferenceHost);

    m_spaceDustCombo = new QComboBox(this);
    m_spaceDustCombo->setEditable(true);
    m_spaceDustCombo->setInsertPolicy(QComboBox::NoInsert);
    form->addRow(tr("Space Dust:"), m_spaceDustCombo);

    m_spaceDustParticlesSpin = new QSpinBox(this);
    m_spaceDustParticlesSpin->setRange(0, 500);
    m_spaceDustParticlesSpin->setValue(50);
    form->addRow(tr("Dust Max Particles:"), m_spaceDustParticlesSpin);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("z. B. Devon Field"));
    form->addRow(tr("Comment:"), m_commentEdit);

    rootLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Erstellen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);

    configureSearchCompleter(m_referenceCombo);
    configureSearchCompleter(m_musicCombo);
    configureSearchCompleter(m_propertyFlagsCombo);
    configureSearchCompleter(m_visitCombo);
    configureSearchCompleter(m_spaceDustCombo);

    loadDataSources();
    updateTypeDependentFields();
    suggestNickname();

    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateTypeDependentFields();
        suggestNickname();
    });
    connect(m_interferenceCheck, &QCheckBox::toggled,
            m_interferenceSpin, &QWidget::setEnabled);
    connect(m_nicknameEdit, &QLineEdit::textEdited, this, [this](const QString &) {
        m_nameEditedByUser = true;
    });
}

void CreateFieldZoneDialog::loadDataSources()
{
    m_asteroidReferences = collectReferenceEntries(QStringLiteral("asteroids"));
    m_nebulaReferences = collectReferenceEntries(QStringLiteral("nebula"));
    m_zoneMusicOptions = collectZoneMusicOptions();
    m_spaceDustOptions = collectSpaceDustOptions();
}

void CreateFieldZoneDialog::populateReferenceCombo()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    const QVector<ReferenceEntry> &entries =
        (type == CreateFieldZoneResult::Type::Asteroid) ? m_asteroidReferences : m_nebulaReferences;
    const QString currentText = canonicalOptionText(m_referenceCombo);

    QSignalBlocker blocker(m_referenceCombo);
    m_referenceCombo->clear();
    for (const ReferenceEntry &entry : entries)
        m_referenceCombo->addItem(entry.fileName, entry.absolutePath);

    if (!currentText.isEmpty()) {
        const int index = m_referenceCombo->findText(currentText, Qt::MatchFixedString);
        if (index >= 0)
            m_referenceCombo->setCurrentIndex(index);
    }
    if (m_referenceCombo->currentIndex() < 0 && m_referenceCombo->count() > 0)
        m_referenceCombo->setCurrentIndex(0);
}

void CreateFieldZoneDialog::populateMusicCombo()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    QStringList preferred = (type == CreateFieldZoneResult::Type::Asteroid)
        ? QStringList{
              QStringLiteral("zone_field_asteroid_rock"),
              QStringLiteral("zone_field_asteroid_ice"),
              QStringLiteral("zone_field_asteroid_mine"),
              QStringLiteral("zone_field_asteroid_lava"),
              QStringLiteral("zone_field_asteroid_nomad"),
              QStringLiteral("zone_field_debris"),
              QStringLiteral("zone_field_mine"),
              QStringLiteral("zone_field_ice"),
              QStringLiteral("zone_badlands"),
          }
        : QStringList{
              QStringLiteral("zone_nebula_crow"),
              QStringLiteral("zone_nebula_barrier"),
              QStringLiteral("zone_nebula_walker"),
              QStringLiteral("zone_nebula_dmatter"),
              QStringLiteral("zone_nebula_nomad"),
              QStringLiteral("zone_nebula_edge"),
          };

    for (const QString &option : std::as_const(m_zoneMusicOptions))
        appendUniqueSorted(preferred, option);

    const QString currentText = canonicalOptionText(m_musicCombo);
    QSignalBlocker blocker(m_musicCombo);
    m_musicCombo->clear();
    m_musicCombo->addItem(QString(), QString());
    for (const QString &option : std::as_const(preferred))
        m_musicCombo->addItem(option, option);

    if (!currentText.isEmpty()) {
        const int index = m_musicCombo->findText(currentText, Qt::MatchFixedString);
        if (index >= 0)
            m_musicCombo->setCurrentIndex(index);
        else
            m_musicCombo->setEditText(currentText);
    } else {
        m_musicCombo->setCurrentIndex(0);
    }
}

void CreateFieldZoneDialog::populatePropertyFlagsCombo()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    const QVector<NumericOption> options =
        (type == CreateFieldZoneResult::Type::Asteroid) ? asteroidPropertyFlags() : nebulaPropertyFlags();
    const QString currentText = canonicalOptionText(m_propertyFlagsCombo);

    QSignalBlocker blocker(m_propertyFlagsCombo);
    m_propertyFlagsCombo->clear();
    for (const NumericOption &option : options)
        m_propertyFlagsCombo->addItem(option.label, option.value);

    if (!currentText.isEmpty()) {
        const int index = m_propertyFlagsCombo->findText(currentText, Qt::MatchFixedString);
        if (index >= 0)
            m_propertyFlagsCombo->setCurrentIndex(index);
        else
            m_propertyFlagsCombo->setEditText(currentText);
    } else if (m_propertyFlagsCombo->count() > 0) {
        m_propertyFlagsCombo->setCurrentIndex(0);
    }
}

void CreateFieldZoneDialog::populateVisitCombo()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    const QVector<NumericOption> options =
        (type == CreateFieldZoneResult::Type::Asteroid) ? asteroidVisitFlags() : nebulaVisitFlags();
    const QString currentText = canonicalOptionText(m_visitCombo);

    QSignalBlocker blocker(m_visitCombo);
    m_visitCombo->clear();
    for (const NumericOption &option : options)
        m_visitCombo->addItem(option.label, option.value);

    if (!currentText.isEmpty()) {
        const int index = m_visitCombo->findText(currentText, Qt::MatchFixedString);
        if (index >= 0)
            m_visitCombo->setCurrentIndex(index);
        else
            m_visitCombo->setEditText(currentText);
    } else if (m_visitCombo->count() > 0) {
        m_visitCombo->setCurrentIndex(0);
    }
}

void CreateFieldZoneDialog::populateSpaceDustCombo()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    const QString defaultValue = (type == CreateFieldZoneResult::Type::Asteroid)
        ? QStringLiteral("asteroiddust")
        : QStringLiteral("attractdust_purple");
    const QString currentText = canonicalOptionText(m_spaceDustCombo);

    QSignalBlocker blocker(m_spaceDustCombo);
    m_spaceDustCombo->clear();
    for (const QString &option : std::as_const(m_spaceDustOptions))
        m_spaceDustCombo->addItem(option, option);

    if (!currentText.isEmpty()) {
        const int index = m_spaceDustCombo->findText(currentText, Qt::MatchFixedString);
        if (index >= 0)
            m_spaceDustCombo->setCurrentIndex(index);
        else
            m_spaceDustCombo->setEditText(currentText);
    } else {
        const int defaultIndex = m_spaceDustCombo->findText(defaultValue, Qt::MatchFixedString);
        if (defaultIndex >= 0)
            m_spaceDustCombo->setCurrentIndex(defaultIndex);
        else
            m_spaceDustCombo->setEditText(defaultValue);
    }
}

void CreateFieldZoneDialog::updateTypeDependentFields()
{
    populateReferenceCombo();
    populateMusicCombo();
    populatePropertyFlagsCombo();
    populateVisitCombo();
    populateSpaceDustCombo();
}

void CreateFieldZoneDialog::suggestNickname()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());
    const QString suggested = suggestedNicknameForType(type);
    if (!m_nameEditedByUser || m_nicknameEdit->text().trimmed().isEmpty() || m_nicknameEdit->text().trimmed() == m_lastAutoNickname)
        m_nicknameEdit->setText(suggested);
    m_lastAutoNickname = suggested;
}

QString CreateFieldZoneDialog::suggestedNicknameForType(CreateFieldZoneResult::Type type) const
{
    const QString artToken = (type == CreateFieldZoneResult::Type::Asteroid)
        ? QStringLiteral("asteroid")
        : QStringLiteral("nebula");
    const QString prefix = QStringLiteral("Zone_%1_%2_").arg(m_systemToken, artToken);

    QSet<int> usedNumbers;
    static const QRegularExpression patternTemplate(QStringLiteral("^(.*?)(\\d+)$"));
    const QRegularExpression pattern(QStringLiteral("^%1(\\d+)$").arg(QRegularExpression::escape(prefix)),
                                     QRegularExpression::CaseInsensitiveOption);

    if (m_document) {
        for (const auto &zone : m_document->zones()) {
            const QString nickname = zone ? zone->nickname().trimmed() : QString();
            const QRegularExpressionMatch match = pattern.match(nickname);
            if (!match.hasMatch())
                continue;
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok)
                usedNumbers.insert(value);
        }
    }

    int next = 1;
    while (usedNumbers.contains(next))
        ++next;
    return prefix + QStringLiteral("%1").arg(next, 3, 10, QLatin1Char('0'));
}

QString CreateFieldZoneDialog::normalizedNameToken(const QString &value) const
{
    QString token = value.trimmed();
    token.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    token.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    token.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (token.isEmpty())
        return {};
    return token;
}

void CreateFieldZoneDialog::configureSearchCompleter(QComboBox *combo)
{
    if (!combo)
        return;
    auto *completer = new QCompleter(combo->model(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    combo->setCompleter(completer);
}

int CreateFieldZoneDialog::numericComboValue(const QComboBox *combo, bool *ok)
{
    bool parsedOk = false;
    int value = 0;
    if (combo && combo->currentData().isValid()) {
        value = combo->currentData().toInt(&parsedOk);
    }
    if (!parsedOk && combo) {
        const QString token = extractLeadingIntegerToken(combo->currentText());
        value = token.toInt(&parsedOk);
    }
    if (ok)
        *ok = parsedOk;
    return parsedOk ? value : 0;
}

QString CreateFieldZoneDialog::canonicalOptionText(const QComboBox *combo)
{
    return combo ? combo->currentText().trimmed() : QString();
}

void CreateFieldZoneDialog::accept()
{
    const CreateFieldZoneResult::Type type =
        static_cast<CreateFieldZoneResult::Type>(m_typeCombo->currentData().toInt());

    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, tr("Zone erstellen"), tr("Bitte gib einen Zonennamen an."));
        return;
    }

    static const QRegularExpression validNickname(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!validNickname.match(nickname).hasMatch()) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Der Zonenname darf nur Buchstaben, Zahlen und Unterstriche enthalten."));
        return;
    }

    if (m_document) {
        for (const auto &zone : m_document->zones()) {
            if (zone && zone->nickname().compare(nickname, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, tr("Zone erstellen"),
                                     tr("Eine Zone mit diesem Nickname existiert bereits."));
                return;
            }
        }
        for (const auto &obj : m_document->objects()) {
            if (obj && obj->nickname().compare(nickname, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, tr("Zone erstellen"),
                                     tr("Ein Objekt mit diesem Nickname existiert bereits."));
                return;
            }
        }
    }

    const QVector<ReferenceEntry> &references =
        (type == CreateFieldZoneResult::Type::Asteroid) ? m_asteroidReferences : m_nebulaReferences;
    const QString referenceText = m_referenceCombo->currentText().trimmed();
    ReferenceEntry selectedReference;
    bool foundReference = false;
    for (const ReferenceEntry &entry : references) {
        if (entry.fileName.compare(referenceText, Qt::CaseInsensitive) == 0) {
            selectedReference = entry;
            foundReference = true;
            break;
        }
    }
    if (!foundReference) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Bitte wähle eine gültige Referenzdatei für den gewählten Typ aus."));
        return;
    }

    const QString music = m_musicCombo->currentText().trimmed();
    if (!music.isEmpty()) {
        bool knownMusic = false;
        for (const QString &option : std::as_const(m_zoneMusicOptions)) {
            if (option.compare(music, Qt::CaseInsensitive) == 0) {
                knownMusic = true;
                break;
            }
        }
        if (!knownMusic) {
            QMessageBox::warning(this, tr("Zone erstellen"),
                                 tr("Bitte wähle einen gültigen Music-Eintrag aus der Liste."));
            return;
        }
    }

    bool propertyOk = false;
    const int propertyFlags = numericComboValue(m_propertyFlagsCombo, &propertyOk);
    if (!propertyOk) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Property Flags muss eine gültige Zahl sein."));
        return;
    }

    bool visitOk = false;
    const int visit = numericComboValue(m_visitCombo, &visitOk);
    if (!visitOk) {
        QMessageBox::warning(this, tr("Zone erstellen"),
                             tr("Visit muss eine gültige Zahl sein."));
        return;
    }

    const QString spacedust = m_spaceDustCombo->currentText().trimmed();
    if (!spacedust.isEmpty()) {
        bool knownDust = false;
        for (const QString &option : std::as_const(m_spaceDustOptions)) {
            if (option.compare(spacedust, Qt::CaseInsensitive) == 0) {
                knownDust = true;
                break;
            }
        }
        if (!knownDust) {
            QMessageBox::warning(this, tr("Zone erstellen"),
                                 tr("Bitte wähle einen gültigen Space-Dust-Eintrag aus der Liste."));
            return;
        }
    }

    m_result.type = type;
    m_result.nickname = nickname;
    m_result.ingameName = m_ingameNameEdit->text().trimmed();
    m_result.referenceFileName = selectedReference.fileName;
    m_result.referenceAbsolutePath = selectedReference.absolutePath;
    m_result.music = music;
    m_result.damage = m_damageSpin->value();
    m_result.propertyFlags = propertyFlags;
    m_result.visit = visit;
    m_result.sort = m_sortSpin->value();
    m_result.hasInterference = m_interferenceCheck->isChecked();
    m_result.interference = m_interferenceSpin->value();
    m_result.spacedust = spacedust;
    m_result.spacedustMaxParticles = m_spaceDustParticlesSpin->value();
    m_result.comment = m_commentEdit->text().trimmed();

    QDialog::accept();
}

} // namespace flatlas::editors
