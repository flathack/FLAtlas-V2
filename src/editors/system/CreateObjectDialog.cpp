#include "CreateObjectDialog.h"

#include "core/EditingContext.h"
#include "core/PathUtils.h"
#include "domain/SolarObject.h"
#include "domain/SystemDocument.h"
#include "infrastructure/freelancer/IdsStringTable.h"
#include "infrastructure/parser/IniParser.h"
#include "rendering/view3d/ModelViewport3D.h"

#include <QCompleter>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStackedLayout>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace flatlas::editors {

namespace {

using flatlas::infrastructure::IdsStringTable;
using flatlas::infrastructure::IniDocument;
using flatlas::infrastructure::IniParser;
using flatlas::infrastructure::IniSection;

QString trimmed(const QString &value) { return value.trimmed(); }

QString derivePrefixFromNickname(const QString &nickname)
{
    // "Li01" -> "Li01"; "Ku01_system" -> "Ku01"; fallback: whole nickname.
    const QString trimmedNick = nickname.trimmed();
    if (trimmedNick.isEmpty())
        return QStringLiteral("system");
    const int underscore = trimmedNick.indexOf(QLatin1Char('_'));
    if (underscore <= 0)
        return trimmedNick;
    return trimmedNick.left(underscore);
}

QString primaryGamePath()
{
    return flatlas::core::EditingContext::instance().primaryGamePath();
}

QString exeDirForIds(const QString &gamePath)
{
    if (gamePath.isEmpty())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
}

QString dataDirFor(const QString &gamePath)
{
    if (gamePath.isEmpty())
        return {};
    return flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("DATA"));
}

} // namespace

CreateObjectDialog::CreateObjectDialog(flatlas::domain::SystemDocument *document, QWidget *parent)
    : QDialog(parent)
    , m_document(document)
{
    setWindowTitle(tr("Neues Objekt"));
    setMinimumSize(820, 520);

    if (m_document)
        m_systemPrefix = derivePrefixFromNickname(m_document->name());
    if (m_systemPrefix.isEmpty())
        m_systemPrefix = QStringLiteral("system");

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // -- Form (left) --
    auto *formHost = new QWidget(this);
    auto *formLayout = new QFormLayout(formHost);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nicknameEdit = new QLineEdit(formHost);
    m_nicknameEdit->setPlaceholderText(tr("z. B. %1_object_001").arg(m_systemPrefix));
    formLayout->addRow(tr("Nickname:"), m_nicknameEdit);

    m_ingameNameEdit = new QLineEdit(formHost);
    m_ingameNameEdit->setPlaceholderText(tr("Sichtbarer Ingame-Name (optional)"));
    formLayout->addRow(tr("Ingame Name:"), m_ingameNameEdit);

    m_archetypeCombo = new QComboBox(formHost);
    m_archetypeCombo->setEditable(true);
    m_archetypeCombo->setInsertPolicy(QComboBox::NoInsert);
    m_archetypeCombo->setMinimumContentsLength(24);
    formLayout->addRow(tr("Archetype:"), m_archetypeCombo);

    m_archetypeStatus = new QLabel(formHost);
    m_archetypeStatus->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_archetypeStatus->setWordWrap(true);
    formLayout->addRow(QString(), m_archetypeStatus);

    m_loadoutCombo = new QComboBox(formHost);
    m_loadoutCombo->setEditable(true);
    m_loadoutCombo->setInsertPolicy(QComboBox::NoInsert);
    formLayout->addRow(tr("Loadout:"), m_loadoutCombo);

    m_loadoutHintLabel = new QLabel(formHost);
    m_loadoutHintLabel->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    m_loadoutHintLabel->setWordWrap(true);
    formLayout->addRow(QString(), m_loadoutHintLabel);

    m_factionCombo = new QComboBox(formHost);
    m_factionCombo->setEditable(true);
    m_factionCombo->setInsertPolicy(QComboBox::NoInsert);
    formLayout->addRow(tr("Reputation:"), m_factionCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, formHost);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Erstellen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *formContainer = new QVBoxLayout();
    formContainer->addWidget(formHost);
    formContainer->addStretch(1);
    formContainer->addWidget(buttons);

    // -- Preview (right) --
    auto *previewFrame = new QFrame(this);
    previewFrame->setFrameShape(QFrame::StyledPanel);
    previewFrame->setMinimumWidth(360);
    auto *previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(8, 8, 8, 8);
    previewLayout->setSpacing(6);

    auto *previewTitle = new QLabel(tr("3D-Vorschau"), previewFrame);
    previewTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    previewLayout->addWidget(previewTitle);

    auto *previewStack = new QWidget(previewFrame);
    auto *stack = new QStackedLayout(previewStack);
    stack->setContentsMargins(0, 0, 0, 0);

    m_preview = new flatlas::rendering::ModelViewport3D(previewStack);
    stack->addWidget(m_preview);

    m_previewFallback = new QLabel(tr("Waehle einen Archetype um die Vorschau zu laden."), previewStack);
    m_previewFallback->setAlignment(Qt::AlignCenter);
    m_previewFallback->setWordWrap(true);
    m_previewFallback->setStyleSheet(QStringLiteral("color:#9ca3af;"));
    stack->addWidget(m_previewFallback);
    stack->setCurrentWidget(m_previewFallback);

    previewLayout->addWidget(previewStack, 1);

    auto *formHolder = new QWidget(this);
    formHolder->setLayout(formContainer);
    root->addWidget(formHolder, 3);
    root->addWidget(previewFrame, 4);

    loadDataSources();
    populateArchetypeCombo();
    populateLoadoutCombo();
    populateFactionCombo();
    suggestNickname();

    connect(m_archetypeCombo, &QComboBox::currentTextChanged,
            this, [this](const QString &) {
        schedulePreviewRefresh();
        applyDefaultLoadoutForCurrentArchetype();
    });
    // Flag any explicit user interaction so the default-loadout suggestion
    // stops overriding a manually chosen value.
    connect(m_loadoutCombo, &QComboBox::activated,
            this, [this](int) { m_loadoutTouchedByUser = true; });
    if (auto *loadoutLineEdit = m_loadoutCombo->lineEdit()) {
        connect(loadoutLineEdit, &QLineEdit::textEdited,
                this, [this](const QString &) { m_loadoutTouchedByUser = true; });
    }
    schedulePreviewRefresh();
    applyDefaultLoadoutForCurrentArchetype();
}

void CreateObjectDialog::loadDataSources()
{
    const QString gamePath = primaryGamePath();
    const QString dataDir = dataDirFor(gamePath);
    const QString exeDir = exeDirForIds(gamePath);

    IdsStringTable ids;
    if (!exeDir.isEmpty())
        ids.loadFromFreelancerDir(exeDir);

    // ---- Archetypes (solararch.ini) ----
    if (!dataDir.isEmpty()) {
        const QString solarArchPath =
            flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("SOLAR/solararch.ini"));
        if (!solarArchPath.isEmpty()) {
            const IniDocument doc = IniParser::parseFile(solarArchPath);
            QSet<QString> seen;
            for (const IniSection &section : doc) {
                const QString nickname = trimmed(section.value(QStringLiteral("nickname")));
                if (nickname.isEmpty() || seen.contains(nickname.toLower()))
                    continue;
                seen.insert(nickname.toLower());

                ArchetypeEntry entry;
                entry.nickname = nickname;
                const int idsName = trimmed(section.value(QStringLiteral("ids_name"))).toInt();
                if (idsName > 0)
                    entry.displayName = ids.getString(idsName).trimmed();
                const QString relativeModel = trimmed(section.value(QStringLiteral("DA_archetype")));
                if (!relativeModel.isEmpty())
                    entry.modelPath = flatlas::core::PathUtils::ciResolvePath(dataDir, relativeModel);
                entry.defaultLoadout = trimmed(section.value(QStringLiteral("loadout")));
                m_archetypes.append(entry);
            }
        }
    }
    std::sort(m_archetypes.begin(), m_archetypes.end(),
              [](const ArchetypeEntry &a, const ArchetypeEntry &b) {
        return a.nickname.compare(b.nickname, Qt::CaseInsensitive) < 0;
    });

    // ---- Loadouts (DATA/SHIPS/loadouts.ini + DATA/SOLAR/loadouts.ini) ----
    if (!dataDir.isEmpty()) {
        const QStringList loadoutSources = {
            QStringLiteral("SHIPS/loadouts.ini"),
            QStringLiteral("SOLAR/loadouts.ini"),
        };
        QSet<QString> seen;
        for (const QString &relative : loadoutSources) {
            const QString resolved = flatlas::core::PathUtils::ciResolvePath(dataDir, relative);
            if (resolved.isEmpty())
                continue;
            const IniDocument doc = IniParser::parseFile(resolved);
            for (const IniSection &section : doc) {
                if (section.name.compare(QStringLiteral("Loadout"), Qt::CaseInsensitive) != 0)
                    continue;
                const QString nickname = trimmed(section.value(QStringLiteral("nickname")));
                if (nickname.isEmpty() || seen.contains(nickname.toLower()))
                    continue;
                seen.insert(nickname.toLower());
                m_loadouts.append(nickname);
            }
        }
        std::sort(m_loadouts.begin(), m_loadouts.end(),
                  [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
    }

    // ---- Factions (initialworld.ini) ----
    if (!dataDir.isEmpty()) {
        const QString iwPath =
            flatlas::core::PathUtils::ciResolvePath(dataDir, QStringLiteral("initialworld.ini"));
        if (!iwPath.isEmpty()) {
            const IniDocument doc = IniParser::parseFile(iwPath);
            QSet<QString> seen;
            for (const IniSection &section : doc) {
                if (section.name.compare(QStringLiteral("Group"), Qt::CaseInsensitive) != 0)
                    continue;
                const QString nickname = trimmed(section.value(QStringLiteral("nickname")));
                if (nickname.isEmpty() || seen.contains(nickname.toLower()))
                    continue;
                seen.insert(nickname.toLower());
                const int idsName = trimmed(section.value(QStringLiteral("ids_name"))).toInt();
                const QString ingame = idsName > 0 ? ids.getString(idsName).trimmed() : QString();
                FactionEntry entry;
                entry.nickname = nickname;
                entry.displayText = ingame.isEmpty()
                    ? nickname
                    : QStringLiteral("%1 - %2").arg(nickname, ingame);
                m_factions.append(entry);
            }
        }
    }
    std::sort(m_factions.begin(), m_factions.end(),
              [](const FactionEntry &a, const FactionEntry &b) {
        return a.displayText.compare(b.displayText, Qt::CaseInsensitive) < 0;
    });
}

void CreateObjectDialog::populateArchetypeCombo()
{
    m_archetypeCombo->clear();
    for (const ArchetypeEntry &entry : m_archetypes) {
        const QString displayText = entry.displayName.isEmpty()
            ? entry.nickname
            : QStringLiteral("%1 - %2").arg(entry.nickname, entry.displayName);
        // userData stores the pure nickname so the written ini never leaks the
        // ingame-name suffix.
        m_archetypeCombo->addItem(displayText, entry.nickname);
    }
    configureSearchCompleter(m_archetypeCombo);
    if (m_archetypes.isEmpty()) {
        m_archetypeStatus->setText(tr("Keine Archetypes in solararch.ini gefunden."));
    } else {
        m_archetypeCombo->setCurrentIndex(0);
        m_archetypeStatus->setText(tr("%1 Archetypes verfuegbar.").arg(m_archetypes.size()));
    }
}

void CreateObjectDialog::populateLoadoutCombo()
{
    m_loadoutCombo->clear();
    m_loadoutCombo->addItem(QString(), QString()); // "(kein Loadout)"
    for (const QString &loadout : m_loadouts)
        m_loadoutCombo->addItem(loadout, loadout);
    configureSearchCompleter(m_loadoutCombo);
    m_loadoutCombo->setCurrentIndex(0);
}

void CreateObjectDialog::populateFactionCombo()
{
    m_factionCombo->clear();
    m_factionCombo->addItem(QString(), QString()); // keine Reputation
    for (const FactionEntry &entry : m_factions)
        m_factionCombo->addItem(entry.displayText, entry.nickname);
    configureSearchCompleter(m_factionCombo);
    m_factionCombo->setCurrentIndex(0);
}

void CreateObjectDialog::suggestNickname()
{
    if (!m_nicknameEdit->text().isEmpty())
        return;
    const QString base = QStringLiteral("%1_object_").arg(m_systemPrefix);

    if (!m_document) {
        m_nicknameEdit->setText(base + QStringLiteral("001"));
        return;
    }

    // Collect existing object nicknames for collision checking.
    QSet<QString> existing;
    for (const auto &obj : m_document->objects()) {
        if (obj)
            existing.insert(obj->nickname().trimmed().toLower());
    }

    // 3-digit running counter starting at 001. Falls back to 4-digit padding
    // if the system somehow already has 999 objects with this pattern.
    for (int counter = 1; counter < 1000; ++counter) {
        const QString candidate = base + QStringLiteral("%1").arg(counter, 3, 10, QLatin1Char('0'));
        if (!existing.contains(candidate.toLower())) {
            m_nicknameEdit->setText(candidate);
            return;
        }
    }
    for (int counter = 1000; counter < 10000; ++counter) {
        const QString candidate = base + QStringLiteral("%1").arg(counter, 4, 10, QLatin1Char('0'));
        if (!existing.contains(candidate.toLower())) {
            m_nicknameEdit->setText(candidate);
            return;
        }
    }
    m_nicknameEdit->setText(base + QStringLiteral("001"));
}

void CreateObjectDialog::configureSearchCompleter(QComboBox *combo)
{
    if (!combo)
        return;
    auto *completer = new QCompleter(combo->model(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    combo->setCompleter(completer);
}

void CreateObjectDialog::schedulePreviewRefresh()
{
    // Debounce so rapid typing in the editable archetype combo does not
    // thrash the Qt3D model loader.
    const int generation = ++m_previewLoadGeneration;
    QTimer::singleShot(180, this, [this, generation]() {
        if (generation != m_previewLoadGeneration)
            return;
        refreshPreview();
    });
}

void CreateObjectDialog::refreshPreview()
{
    if (!m_preview || !m_previewFallback)
        return;
    auto *stack = qobject_cast<QStackedLayout *>(m_preview->parentWidget()->layout());
    auto showFallback = [&](const QString &message) {
        m_previewFallback->setText(message);
        if (stack)
            stack->setCurrentWidget(m_previewFallback);
        m_preview->clearModel();
    };

    const QString currentText = m_archetypeCombo->currentText().trimmed();
    if (currentText.isEmpty()) {
        showFallback(tr("Waehle einen Archetype um die Vorschau zu laden."));
        return;
    }

    const ArchetypeEntry *match = resolveCurrentArchetype();
    if (!match) {
        showFallback(tr("Archetype \"%1\" nicht in solararch.ini gefunden.").arg(currentText));
        return;
    }
    if (match->modelPath.isEmpty() || !QFileInfo::exists(match->modelPath)) {
        showFallback(tr("Kein 3D-Modell fuer %1 vorhanden.").arg(match->nickname));
        return;
    }

    QString errorMessage;
    if (!m_preview->loadModelFile(match->modelPath, &errorMessage)) {
        showFallback(tr("Modell konnte nicht geladen werden: %1")
                     .arg(errorMessage.isEmpty() ? tr("unbekannter Fehler") : errorMessage));
        return;
    }
    if (stack)
        stack->setCurrentWidget(m_preview);
}

const CreateObjectDialog::ArchetypeEntry *CreateObjectDialog::resolveCurrentArchetype() const
{
    // Users may type the nickname manually even when the combo popup is
    // closed, so match against both the stored userData and the nickname part
    // of the display text ("li01_01_base - Manhattan").
    const QString currentText = m_archetypeCombo->currentText().trimmed();
    QString nickname = m_archetypeCombo->currentData().toString().trimmed();
    if (nickname.isEmpty()) {
        const int dashIndex = currentText.indexOf(QStringLiteral(" - "));
        nickname = dashIndex >= 0 ? currentText.left(dashIndex).trimmed() : currentText;
    }
    if (nickname.isEmpty())
        return nullptr;
    for (const ArchetypeEntry &entry : m_archetypes) {
        if (entry.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            return &entry;
    }
    return nullptr;
}

void CreateObjectDialog::applyDefaultLoadoutForCurrentArchetype()
{
    if (!m_loadoutCombo || !m_loadoutHintLabel)
        return;

    const ArchetypeEntry *match = resolveCurrentArchetype();
    const QString defaultLoadout = match ? match->defaultLoadout : QString();

    if (defaultLoadout.isEmpty()) {
        m_loadoutHintLabel->setText(tr("Kein Default-Loadout in solararch.ini hinterlegt."));
    } else {
        m_loadoutHintLabel->setText(tr("Default-Loadout (solararch.ini): %1").arg(defaultLoadout));
    }

    // Only auto-fill when the user has not manually edited the field yet.
    // Switching archetypes re-enables the suggestion so the convenience keeps
    // working while the user explores options.
    if (m_loadoutTouchedByUser)
        return;
    if (defaultLoadout.isEmpty()) {
        m_loadoutCombo->setCurrentIndex(0); // "(kein Loadout)"
        return;
    }

    const int idx = m_loadoutCombo->findData(defaultLoadout, Qt::UserRole, Qt::MatchFixedString);
    if (idx >= 0)
        m_loadoutCombo->setCurrentIndex(idx);
    else
        m_loadoutCombo->setEditText(defaultLoadout);
}

void CreateObjectDialog::accept()
{
    static const QRegularExpression kNicknamePattern(
        QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_\\-]*$"));

    const QString nickname = m_nicknameEdit->text().trimmed();
    if (nickname.isEmpty() || !kNicknamePattern.match(nickname).hasMatch()) {
        QMessageBox::warning(this, tr("Ungueltiger Nickname"),
                             tr("Bitte einen gueltigen Nickname eingeben (Buchstaben, Ziffern, _ und -)."));
        m_nicknameEdit->setFocus();
        return;
    }
    if (m_document) {
        for (const auto &obj : m_document->objects()) {
            if (obj && obj->nickname().compare(nickname, Qt::CaseInsensitive) == 0) {
                QMessageBox::warning(this, tr("Nickname bereits vergeben"),
                                     tr("Im aktuellen System existiert bereits ein Objekt mit diesem Nickname."));
                m_nicknameEdit->setFocus();
                return;
            }
        }
    }

    // Archetype: prefer the combobox userData (pure nickname). Fall back to the
    // typed text so advanced users can still enter a custom archetype.
    QString archetype = m_archetypeCombo->currentData().toString().trimmed();
    if (archetype.isEmpty()) {
        const QString typed = m_archetypeCombo->currentText().trimmed();
        const int dashIndex = typed.indexOf(QStringLiteral(" - "));
        archetype = dashIndex >= 0 ? typed.left(dashIndex).trimmed() : typed;
    }
    if (archetype.isEmpty()) {
        QMessageBox::warning(this, tr("Kein Archetype gewaehlt"),
                             tr("Bitte einen Archetype auswaehlen."));
        m_archetypeCombo->setFocus();
        return;
    }

    m_result.nickname = nickname;
    m_result.ingameName = m_ingameNameEdit->text().trimmed();
    m_result.archetype = archetype;

    // Loadout stores the pure nickname even when the user typed a free value.
    QString loadout = m_loadoutCombo->currentData().toString().trimmed();
    if (loadout.isEmpty())
        loadout = m_loadoutCombo->currentText().trimmed();
    m_result.loadout = loadout;

    // Reputation: the dialog shows "nickname - ingame" but must only write the
    // nickname. Prefer userData, fall back to parsing the typed text so a
    // manual "li_n_grp" entry still works.
    QString factionNick = m_factionCombo->currentData().toString().trimmed();
    if (factionNick.isEmpty()) {
        const QString typed = m_factionCombo->currentText().trimmed();
        const int dashIndex = typed.indexOf(QStringLiteral(" - "));
        factionNick = dashIndex >= 0 ? typed.left(dashIndex).trimmed() : typed;
    }
    m_result.reputationNickname = factionNick;

    QDialog::accept();
}

} // namespace flatlas::editors
