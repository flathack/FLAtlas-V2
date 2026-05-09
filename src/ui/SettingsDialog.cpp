#include "SettingsDialog.h"

#include "core/Config.h"
#include "core/EditingContext.h"
#include "core/I18n.h"
#include "core/PathUtils.h"
#include "core/Theme.h"
#include "core/ThemeColors.h"
#include "infrastructure/freelancer/IdsDataService.h"
#include "infrastructure/freelancer/ResourceDllWriter.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace flatlas::ui {
namespace {

using flatlas::infrastructure::IdsDataService;
using flatlas::infrastructure::IdsDataset;
using flatlas::infrastructure::IdsEntryRecord;
using flatlas::infrastructure::ResourceDllWriter;

QStringList defaultPinnedTools()
{
    return {QStringLiteral("modManager"), QStringLiteral("universe")};
}

struct ToolChoice {
    QString key;
    QString label;
    bool mandatory = false;
};

QVector<ToolChoice> toolChoices()
{
    return {
        {QStringLiteral("modManager"), QObject::tr("Mod Manager"), true},
        {QStringLiteral("universe"), QObject::tr("Universe"), false},
        {QStringLiteral("tradeRoutes"), QObject::tr("Trade Routes"), false},
        {QStringLiteral("idsEditor"), QObject::tr("IDS Editor"), false},
        {QStringLiteral("modSettings"), QObject::tr("Mod Settings"), false},
        {QStringLiteral("npcEditor"), QObject::tr("NPC Editor"), false},
        {QStringLiteral("factionEditor"), QObject::tr("Faction Editor"), false},
        {QStringLiteral("newsRumorEditor"), QObject::tr("News Editor"), false},
        {QStringLiteral("modelViewer"), QObject::tr("3D Model Viewer"), false},
    };
}

struct SuiteApp {
    QString key;
    QString name;
    QString repoApiUrl;
    QString websiteUrl;
};

QVector<SuiteApp> suiteApps()
{
    return {
        {QStringLiteral("savegameEditor"), QObject::tr("Savegame Editor"), QStringLiteral("https://api.github.com/repos/flathack/FLAtlas---Save-Game-Editor/releases/latest"), {}},
        {QStringLiteral("flLingo"), QObject::tr("FL Lingo"), QStringLiteral("https://api.github.com/repos/flathack/FL-Lingo/releases/latest"), {}},
        {QStringLiteral("flAtlasLauncher"), QObject::tr("FL Atlas Launcher"), QStringLiteral("https://api.github.com/repos/flathack/FL-Atlas-Launcher/releases/latest"), {}},
        {QStringLiteral("webTools"), QObject::tr("Web Tools"), {}, QStringLiteral("https://flathack.github.io/")},
    };
}

QString fileNameFromUrl(const QUrl &url)
{
    const QString name = QFileInfo(url.path()).fileName();
    return name.isEmpty() ? QStringLiteral("download.bin") : name;
}

QString installedToolExe(const QString &key)
{
    const QJsonObject tools = flatlas::core::Config::instance().getJsonObject(QStringLiteral("externalTools"));
    return tools.value(key).toObject().value(QStringLiteral("exePath")).toString();
}

QString firstExeInDirectory(const QString &dirPath)
{
    QDirIterator it(dirPath, {QStringLiteral("*.exe")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString exe = it.next();
        const QString name = QFileInfo(exe).fileName().toLower();
        if (!name.contains(QStringLiteral("unins")) && !name.contains(QStringLiteral("setup")))
            return exe;
    }
    return {};
}

QString activeGamePath()
{
    const auto &ctx = flatlas::core::EditingContext::instance();
    return ctx.hasContext() ? ctx.primaryGamePath() : QString();
}

QString activeGameExeDir()
{
    const QString gamePath = activeGamePath();
    if (gamePath.trimmed().isEmpty())
        return {};

    const QFileInfo info(gamePath);
    if (info.isFile() && info.fileName().compare(QStringLiteral("freelancer.ini"), Qt::CaseInsensitive) == 0)
        return info.absolutePath();

    QString exeDir = flatlas::core::PathUtils::ciResolvePath(gamePath, QStringLiteral("EXE"));
    if (!exeDir.isEmpty())
        return exeDir;

    const QString fallback = QDir(gamePath).absoluteFilePath(QStringLiteral("EXE"));
    return QFileInfo::exists(fallback) ? fallback : QString();
}

QString activeFreelancerIniPath()
{
    const QString exeDir = activeGameExeDir();
    if (exeDir.trimmed().isEmpty())
        return {};

    QString freelancerIni = flatlas::core::PathUtils::ciResolvePath(exeDir, QStringLiteral("freelancer.ini"));
    if (!freelancerIni.isEmpty())
        return freelancerIni;

    const QString fallback = QDir(exeDir).absoluteFilePath(QStringLiteral("freelancer.ini"));
    return QFileInfo::exists(fallback) ? fallback : QString();
}

QString idsEntrySummary(const IdsEntryRecord &entry)
{
    const QString kind = entry.hasHtmlValue ? QStringLiteral("ids_info") : QStringLiteral("string");
    QString text = entry.hasHtmlValue ? entry.plainText : entry.stringValue;
    text = text.simplified();
    if (text.size() > 90)
        text = text.left(87) + QStringLiteral("...");
    if (text.isEmpty())
        text = QStringLiteral("-");
    return QStringLiteral("%1 | %2 | %3").arg(entry.globalId).arg(kind, text);
}

QString languageCacheDirectory()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData.isEmpty() ? QCoreApplication::applicationDirPath() : appData)
        .absoluteFilePath(QStringLiteral("languages"));
}

constexpr const char *kLanguageCatalogUrl =
    "https://raw.githubusercontent.com/flathack/FLAtlas-V2/master/resources/languages/index.json";

}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setupUi();
    loadSettings();
}

bool SettingsDialog::requiresPinnedToolRefresh() const
{
    return m_pinnedToolsChanged;
}

bool SettingsDialog::resetRequested() const
{
    return m_resetRequested;
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("FLAtlas Settings"));
    setMinimumSize(620, 520);

    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    auto *generalTab = new QWidget(tabs);
    auto *generalLayout = new QFormLayout(generalTab);
    m_languageCombo = new QComboBox(generalTab);
    auto *languageRow = new QWidget(generalTab);
    auto *languageLayout = new QHBoxLayout(languageRow);
    languageLayout->setContentsMargins(0, 0, 0, 0);
    languageLayout->addWidget(m_languageCombo, 1);
    m_languageUpdateButton = new QPushButton(tr("Update Languages"), languageRow);
    languageLayout->addWidget(m_languageUpdateButton);
    generalLayout->addRow(tr("Language:"), languageRow);
    m_languageStatusLabel = new QLabel(generalTab);
    m_languageStatusLabel->setWordWrap(true);
    generalLayout->addRow(QString(), m_languageStatusLabel);
    refreshLanguageCombo();
    connect(m_languageUpdateButton, &QPushButton::clicked, this, &SettingsDialog::updateLanguagesFromGitHub);

    m_themeCombo = new QComboBox(generalTab);
    m_themeCombo->addItems(flatlas::core::Theme::instance().availableThemes());
    generalLayout->addRow(tr("Design:"), m_themeCombo);

    m_updateCheckBox = new QCheckBox(tr("Update-Check beim Start aktivieren"), generalTab);
    generalLayout->addRow(QString(), m_updateCheckBox);

    m_restoreTabsCheckBox = new QCheckBox(tr("Restore open tabs on startup"), generalTab);
    generalLayout->addRow(QString(), m_restoreTabsCheckBox);
    tabs->addTab(generalTab, tr("Allgemein"));

    auto *themeColorsTab = new QWidget(tabs);
    auto *themeColorsRoot = new QVBoxLayout(themeColorsTab);
    auto *uiColorGroup = new QGroupBox(tr("UI Color"), themeColorsTab);
    auto *uiColorLayout = new QFormLayout(uiColorGroup);
    auto *systemViewGroup = new QGroupBox(tr("2D / 3D System View"), themeColorsTab);
    auto *systemViewLayout = new QFormLayout(systemViewGroup);
    for (const flatlas::core::ThemeColorChoice &choice : flatlas::core::ThemeColors::choices()) {
        auto *button = new QPushButton(themeColorsTab);
        button->setProperty("themeColorKey", choice.key);
        m_themeColorButtons.insert(choice.key, button);
        connect(button, &QPushButton::clicked, this, [this, key = choice.key]() {
            const QColor stored(m_themeColorButtons.value(key)->property("themeColorValue").toString());
            const QColor current = stored.isValid() ? stored : flatlas::core::ThemeColors::color(key);
            const QColor selected = QColorDialog::getColor(current, this, tr("Theme Color"));
            if (!selected.isValid())
                return;
            QPushButton *button = m_themeColorButtons.value(key);
            if (!button)
                return;
            const QString colorName = selected.name(QColor::HexRgb);
            button->setProperty("themeColorValue", colorName);
            button->setText(colorName.toUpper());
            button->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: %1;"
                "  color: %2;"
                "  border: 1px solid rgba(0,0,0,90);"
                "  min-width: 92px;"
                "  padding: 5px 10px;"
                "}"
            ).arg(colorName, selected.lightness() > 145 ? QStringLiteral("#101418") : QStringLiteral("#ffffff")));
        });
        QFormLayout *targetLayout = choice.key == QStringLiteral("uiAccent") ? uiColorLayout : systemViewLayout;
        targetLayout->addRow(choice.label + QLatin1Char(':'), button);
    }
    themeColorsRoot->addWidget(uiColorGroup);
    themeColorsRoot->addWidget(systemViewGroup);
    themeColorsRoot->addStretch();
    tabs->addTab(themeColorsTab, tr("Theme"));

    auto *pinnedTab = new QWidget(tabs);
    auto *pinnedLayout = new QVBoxLayout(pinnedTab);
    auto *pinnedHint = new QLabel(tr("Selected tools are permanently shown as tabs. Mod Manager is always active."), pinnedTab);
    pinnedHint->setWordWrap(true);
    pinnedLayout->addWidget(pinnedHint);
    for (const ToolChoice &tool : toolChoices()) {
        auto *check = new QCheckBox(tool.label, pinnedTab);
        check->setProperty("toolKey", tool.key);
        if (tool.mandatory) {
            check->setChecked(true);
            check->setEnabled(false);
        }
        m_toolChecks.insert(tool.key, check);
        pinnedLayout->addWidget(check);
    }
    pinnedLayout->addStretch();
    tabs->addTab(pinnedTab, tr("Pinned Tools"));

    auto *suiteTab = new QWidget(tabs);
    auto *suiteLayout = new QVBoxLayout(suiteTab);
    for (const SuiteApp &app : suiteApps()) {
        auto *row = new QWidget(suiteTab);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto *name = new QLabel(app.name, row);
        rowLayout->addWidget(name, 1);
        auto *button = new QPushButton(app.websiteUrl.isEmpty() ? tr("Download") : tr("Open"), row);
        rowLayout->addWidget(button);
        suiteLayout->addWidget(row);
        if (app.websiteUrl.isEmpty()) {
            m_suiteButtons.insert(app.key, button);
            connect(button, &QPushButton::clicked, this, [this, app]() {
                if (!installedToolExe(app.key).isEmpty())
                    openInstalledTool(app.key);
                else
                    startSuiteDownload(app.key, app.name, app.repoApiUrl);
            });
        } else {
            connect(button, &QPushButton::clicked, this, [app]() {
                QDesktopServices::openUrl(QUrl(app.websiteUrl));
            });
        }
    }
    m_suiteStatusLabel = new QLabel(suiteTab);
    m_suiteStatusLabel->setWordWrap(true);
    suiteLayout->addWidget(m_suiteStatusLabel);
    suiteLayout->addStretch();
    tabs->addTab(suiteTab, tr("FL Suite Apps"));

    auto *configTab = new QWidget(tabs);
    auto *configLayout = new QVBoxLayout(configTab);
    auto *pathRow = new QWidget(configTab);
    auto *pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    m_configPathEdit = new QLineEdit(pathRow);
    m_configPathEdit->setReadOnly(true);
    pathLayout->addWidget(m_configPathEdit, 1);
    auto *choosePathButton = new QPushButton(tr("Choose Location"), pathRow);
    pathLayout->addWidget(choosePathButton);
    configLayout->addWidget(pathRow);

    auto *actionsRow = new QWidget(configTab);
    auto *actionsLayout = new QHBoxLayout(actionsRow);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *importButton = new QPushButton(tr("Import"), actionsRow);
    auto *exportButton = new QPushButton(tr("Export"), actionsRow);
    auto *backupButton = new QPushButton(tr("Create Backup"), actionsRow);
    auto *applyJsonButton = new QPushButton(tr("Apply JSON"), actionsRow);
    actionsLayout->addWidget(importButton);
    actionsLayout->addWidget(exportButton);
    actionsLayout->addWidget(backupButton);
    actionsLayout->addStretch();
    actionsLayout->addWidget(applyJsonButton);
    configLayout->addWidget(actionsRow);

    m_configJsonEdit = new QTextEdit(configTab);
    m_configJsonEdit->setAcceptRichText(false);
    m_configJsonEdit->setLineWrapMode(QTextEdit::NoWrap);
    configLayout->addWidget(m_configJsonEdit, 1);

    m_configStatusLabel = new QLabel(configTab);
    m_configStatusLabel->setWordWrap(true);
    configLayout->addWidget(m_configStatusLabel);
    tabs->addTab(configTab, tr("Config"));
    connect(choosePathButton, &QPushButton::clicked, this, &SettingsDialog::chooseConfigPath);
    connect(importButton, &QPushButton::clicked, this, &SettingsDialog::importConfig);
    connect(exportButton, &QPushButton::clicked, this, &SettingsDialog::exportConfig);
    connect(backupButton, &QPushButton::clicked, this, &SettingsDialog::createConfigBackup);
    connect(applyJsonButton, &QPushButton::clicked, this, &SettingsDialog::applyConfigJson);

    auto *idsTab = new QWidget(tabs);
    auto *idsLayout = new QVBoxLayout(idsTab);
    auto *idsHint = new QLabel(tr("New IDS strings and infocards are created in the FLAtlas DLL by default. You can set a mod-owned resource DLL as the target here."), idsTab);
    idsHint->setWordWrap(true);
    idsLayout->addWidget(idsHint);

    auto *idsPathRow = new QWidget(idsTab);
    auto *idsPathLayout = new QHBoxLayout(idsPathRow);
    idsPathLayout->setContentsMargins(0, 0, 0, 0);
    m_idsTargetDllEdit = new QLineEdit(idsPathRow);
    m_idsTargetDllEdit->setPlaceholderText(ResourceDllWriter::preferredFlatlasDllName());
    idsPathLayout->addWidget(m_idsTargetDllEdit, 1);
    auto *chooseIdsDllButton = new QPushButton(tr("Choose DLL"), idsPathRow);
    idsPathLayout->addWidget(chooseIdsDllButton);
    idsLayout->addWidget(idsPathRow);

    auto *idsActionsRow = new QWidget(idsTab);
    auto *idsActionsLayout = new QHBoxLayout(idsActionsRow);
    idsActionsLayout->setContentsMargins(0, 0, 0, 0);
    auto *saveIdsTargetButton = new QPushButton(tr("Save Target"), idsActionsRow);
    auto *resetIdsTargetButton = new QPushButton(tr("Use FLAtlas DLL"), idsActionsRow);
    auto *migrateIdsButton = new QPushButton(tr("Import FLAtlas Entries"), idsActionsRow);
    idsActionsLayout->addWidget(saveIdsTargetButton);
    idsActionsLayout->addWidget(resetIdsTargetButton);
    idsActionsLayout->addStretch();
    idsActionsLayout->addWidget(migrateIdsButton);
    idsLayout->addWidget(idsActionsRow);

    m_idsTargetStatusLabel = new QLabel(idsTab);
    m_idsTargetStatusLabel->setWordWrap(true);
    idsLayout->addWidget(m_idsTargetStatusLabel);
    idsLayout->addStretch();
    tabs->addTab(idsTab, tr("IDS/Infocards"));
    connect(chooseIdsDllButton, &QPushButton::clicked, this, &SettingsDialog::chooseIdsTargetDll);
    connect(saveIdsTargetButton, &QPushButton::clicked, this, [this]() { saveIdsTargetDllSettings(); });
    connect(resetIdsTargetButton, &QPushButton::clicked, this, &SettingsDialog::resetIdsTargetDll);
    connect(migrateIdsButton, &QPushButton::clicked, this, &SettingsDialog::migrateFlatlasIdsEntries);

    auto *resetTab = new QWidget(tabs);
    auto *resetLayout = new QVBoxLayout(resetTab);
    auto *resetHint = new QLabel(tr("Resets FLAtlas to factory settings. Mod installations and game data are not deleted."), resetTab);
    resetHint->setWordWrap(true);
    resetLayout->addWidget(resetHint);
    auto *resetButton = new QPushButton(tr("Reset Application to Defaults"), resetTab);
    resetLayout->addWidget(resetButton, 0, Qt::AlignLeft);
    resetLayout->addStretch();
    tabs->addTab(resetTab, tr("Reset"));
    connect(resetButton, &QPushButton::clicked, this, &SettingsDialog::resetToDefaults);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateSuiteButtons();
}

void SettingsDialog::loadSettings()
{
    auto &config = flatlas::core::Config::instance();
    m_themeCombo->setCurrentText(config.getString(QStringLiteral("theme"), flatlas::core::Theme::instance().currentTheme()));
    m_languageCombo->setCurrentText(config.getString(QStringLiteral("language"), flatlas::core::I18n::instance().currentLanguage()));
    m_updateCheckBox->setChecked(config.getBool(QStringLiteral("updateCheckEnabled"), true));
    m_restoreTabsCheckBox->setChecked(config.getBool(QStringLiteral("restoreOpenTabs"), false));

    const QStringList pinned = config.getStringList(QStringLiteral("pinnedTools"), defaultPinnedTools());
    for (auto it = m_toolChecks.begin(); it != m_toolChecks.end(); ++it)
        it.value()->setChecked(it.key() == QStringLiteral("modManager") || pinned.contains(it.key()));

    for (auto it = m_themeColorButtons.constBegin(); it != m_themeColorButtons.constEnd(); ++it)
        updateThemeColorButton(it.key());

    refreshConfigManager();
    refreshIdsTargetDllSettings();
}

void SettingsDialog::saveSettings()
{
    auto &config = flatlas::core::Config::instance();
    const QString oldPinned = config.getStringList(QStringLiteral("pinnedTools"), defaultPinnedTools()).join(QLatin1Char('|'));

    const QString theme = m_themeCombo->currentText();
    const QString lang = m_languageCombo->currentText();
    config.setString(QStringLiteral("theme"), theme);
    config.setString(QStringLiteral("language"), lang);
    config.setBool(QStringLiteral("updateCheckEnabled"), m_updateCheckBox->isChecked());
    config.setBool(QStringLiteral("restoreOpenTabs"), m_restoreTabsCheckBox->isChecked());
    for (auto it = m_themeColorButtons.constBegin(); it != m_themeColorButtons.constEnd(); ++it) {
        const QColor color(it.value()->property("themeColorValue").toString());
        if (color.isValid())
            flatlas::core::ThemeColors::setColor(it.key(), color);
    }
    flatlas::core::Theme::instance().apply(theme);
    flatlas::core::I18n::instance().setLanguage(lang);

    QStringList pinned;
    for (auto it = m_toolChecks.constBegin(); it != m_toolChecks.constEnd(); ++it) {
        if (it.value()->isChecked())
            pinned.append(it.key());
    }
    if (!pinned.contains(QStringLiteral("modManager")))
        pinned.prepend(QStringLiteral("modManager"));
    config.setStringList(QStringLiteral("pinnedTools"), pinned);
    m_pinnedToolsChanged = oldPinned != pinned.join(QLatin1Char('|'));
    config.save();
    refreshConfigManager();
}

void SettingsDialog::resetToDefaults()
{
    const auto answer = QMessageBox::question(this,
                                              tr("Factory Defaults"),
                                              tr("Really reset FLAtlas to factory defaults?"));
    if (answer != QMessageBox::Yes)
        return;
    auto &config = flatlas::core::Config::instance();
    config.clear();
    config.setString(QStringLiteral("theme"), QStringLiteral("dark"));
    config.setString(QStringLiteral("language"), QStringLiteral("en"));
    config.setBool(QStringLiteral("updateCheckEnabled"), true);
    config.setBool(QStringLiteral("restoreOpenTabs"), false);
    config.setStringList(QStringLiteral("pinnedTools"), defaultPinnedTools());
    flatlas::core::ThemeColors::resetToDefaults();
    config.save();
    QSettings().clear();
    flatlas::core::Theme::instance().apply(QStringLiteral("dark"));
    flatlas::core::I18n::instance().setLanguage(QStringLiteral("en"));
    m_resetRequested = true;
    m_pinnedToolsChanged = true;
    loadSettings();
    QMessageBox::information(this, tr("Factory Defaults"), tr("Settings have been reset."));
}

void SettingsDialog::refreshConfigManager()
{
    const auto &config = flatlas::core::Config::instance();
    if (m_configPathEdit)
        m_configPathEdit->setText(config.filePath());
    if (m_configJsonEdit)
        m_configJsonEdit->setPlainText(QString::fromUtf8(
            QJsonDocument(config.data()).toJson(QJsonDocument::Indented)));
}

void SettingsDialog::chooseConfigPath()
{
    auto &config = flatlas::core::Config::instance();
    const QString current = config.filePath().isEmpty() ? flatlas::core::Config::defaultConfigPath() : config.filePath();
    const QString path = QFileDialog::getSaveFileName(this,
                                                      tr("Choose Config Location"),
                                                      current,
                                                      tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;

    if (!config.setConfigPath(path)) {
        QMessageBox::warning(this, tr("Config"), tr("The new location could not be written."));
        return;
    }
    m_configStatusLabel->setText(tr("Config location updated."));
    refreshConfigManager();
}

void SettingsDialog::importConfig()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Config importieren"),
                                                      {},
                                                      tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!flatlas::core::Config::instance().importFrom(path)) {
        QMessageBox::warning(this, tr("Config"), tr("The config could not be imported."));
        return;
    }
    loadSettings();
    m_configStatusLabel->setText(tr("Config importiert."));
}

void SettingsDialog::exportConfig()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      tr("Config exportieren"),
                                                      flatlas::core::Config::instance().filePath(),
                                                      tr("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!flatlas::core::Config::instance().exportTo(path)) {
        QMessageBox::warning(this, tr("Config"), tr("The config could not be exported."));
        return;
    }
    m_configStatusLabel->setText(tr("Config exportiert."));
}

void SettingsDialog::createConfigBackup()
{
    QString backupPath;
    if (!flatlas::core::Config::instance().createBackup(&backupPath)) {
        QMessageBox::warning(this, tr("Config"), tr("Could not create a backup."));
        return;
    }
    m_configStatusLabel->setText(tr("Backup angelegt: %1").arg(backupPath));
}

void SettingsDialog::applyConfigJson()
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(m_configJsonEdit->toPlainText().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Config"), tr("The JSON is invalid or not an object."));
        return;
    }
    auto &config = flatlas::core::Config::instance();
    config.setData(doc.object());
    if (!config.save()) {
        QMessageBox::warning(this, tr("Config"), tr("The config could not be saved."));
        return;
    }
    loadSettings();
    m_configStatusLabel->setText(tr("JSON uebernommen."));
}

void SettingsDialog::refreshIdsTargetDllSettings()
{
    if (!m_idsTargetDllEdit || !m_idsTargetStatusLabel)
        return;

    auto &config = flatlas::core::Config::instance();
    const QString configured = config.getString(QStringLiteral("idsCreationTargetDll")).trimmed();
    QString displayed = configured;
    QString status = tr("Current target: FLAtlas default (%1).").arg(ResourceDllWriter::preferredFlatlasDllName());

    const QString gamePath = activeGamePath();
    if (!gamePath.isEmpty()) {
        const QString freelancerIni = activeFreelancerIniPath();
        const QString effective = configured.isEmpty() && !freelancerIni.isEmpty()
            ? ResourceDllWriter::targetFlatlasResourceDll(freelancerIni)
            : (configured.isEmpty() ? ResourceDllWriter::preferredFlatlasDllName() : QFileInfo(configured).fileName());
        if (displayed.isEmpty())
            displayed = effective;
        status = tr("Active context: %1\nEffective target DLL: %2").arg(gamePath, effective);
    } else if (!configured.isEmpty()) {
        status = tr("Effective target DLL: %1. No active Freelancer context loaded.").arg(configured);
    }

    m_idsTargetDllEdit->setText(displayed);
    m_idsTargetStatusLabel->setText(status);
}

void SettingsDialog::chooseIdsTargetDll()
{
    const QString startDir = activeGameExeDir();

    const QString path = QFileDialog::getOpenFileName(this,
                                                      tr("Choose Target DLL for IDS/Infocards"),
                                                      startDir,
                                                      tr("DLL Files (*.dll);;All Files (*.*)"));
    if (path.isEmpty())
        return;
    m_idsTargetDllEdit->setText(QFileInfo(path).fileName());
}

void SettingsDialog::saveIdsTargetDllSettings()
{
    saveIdsTargetDllSettings(true);
}

bool SettingsDialog::saveIdsTargetDllSettings(bool offerMigration)
{
    const QString dllName = QFileInfo(m_idsTargetDllEdit->text().trimmed()).fileName();
    if (dllName.isEmpty()) {
        resetIdsTargetDll();
        return true;
    }
    if (!dllName.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive)) {
        QMessageBox::warning(this, tr("IDS/Infocards"), tr("Please enter a DLL file as target."));
        return false;
    }

    auto &config = flatlas::core::Config::instance();
    const QString previous = QFileInfo(config.getString(QStringLiteral("idsCreationTargetDll")).trimmed()).fileName();
    const bool targetChanged = previous.compare(dllName, Qt::CaseInsensitive) != 0;
    config.setString(QStringLiteral("idsCreationTargetDll"), dllName);
    config.save();
    refreshIdsTargetDllSettings();
    m_idsTargetStatusLabel->setText(tr("IDS/Infocard target saved: %1").arg(dllName));
    if (offerMigration && targetChanged && !ResourceDllWriter::isFlatlasResourceDll(dllName))
        migrateFlatlasIdsEntriesToTarget(dllName, false);
    return true;
}

void SettingsDialog::resetIdsTargetDll()
{
    auto &config = flatlas::core::Config::instance();
    config.setString(QStringLiteral("idsCreationTargetDll"), QString());
    config.save();
    refreshIdsTargetDllSettings();
    m_idsTargetStatusLabel->setText(tr("New entries will use the FLAtlas DLL again."));
}

void SettingsDialog::migrateFlatlasIdsEntries()
{
    const QString gamePath = activeGamePath();
    if (gamePath.isEmpty()) {
        QMessageBox::information(this, tr("IDS/Infocards"), tr("No Freelancer context is loaded."));
        return;
    }

    if (!saveIdsTargetDllSettings(false))
        return;
    const QString targetDll = QFileInfo(m_idsTargetDllEdit->text().trimmed()).fileName();
    if (targetDll.isEmpty())
        return;
    if (ResourceDllWriter::isFlatlasResourceDll(targetDll)) {
        QMessageBox::information(this, tr("IDS/Infocards"), tr("The FLAtlas DLL is already selected as target."));
        return;
    }

    migrateFlatlasIdsEntriesToTarget(targetDll, true);
}

void SettingsDialog::migrateFlatlasIdsEntriesToTarget(const QString &targetDll, bool showNoEntriesMessage)
{
    const QString gamePath = activeGamePath();
    if (gamePath.isEmpty()) {
        if (showNoEntriesMessage)
            QMessageBox::information(this, tr("IDS/Infocards"), tr("No Freelancer context is loaded."));
        else
            m_idsTargetStatusLabel->setText(tr("Target DLL changed. No active Freelancer context loaded for migration."));
        return;
    }

    const IdsDataset dataset = IdsDataService::loadFromGameRoot(gamePath);
    QVector<IdsEntryRecord> flatlasEntries;
    for (const IdsEntryRecord &entry : dataset.entries) {
        if (ResourceDllWriter::isFlatlasResourceDll(entry.dllName))
            flatlasEntries.append(entry);
    }

    if (flatlasEntries.isEmpty()) {
        if (showNoEntriesMessage)
            QMessageBox::information(this, tr("IDS/Infocards"), tr("No entries were found in the FLAtlas DLL."));
        else
            m_idsTargetStatusLabel->setText(tr("Target DLL changed. No entries were found in the FLAtlas DLL."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Import FLAtlas Entries"));
    dialog.resize(760, 480);
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(tr("Select the entries that should be created in %1. Existing FLAtlas entries and freelancer.ini lines remain unchanged.").arg(targetDll), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *list = new QListWidget(&dialog);
    for (int i = 0; i < flatlasEntries.size(); ++i) {
        auto *item = new QListWidgetItem(idsEntrySummary(flatlasEntries.at(i)), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, i);
    }
    layout->addWidget(list, 1);

    auto *buttons = new QDialogButtonBox(&dialog);
    auto *copyButton = buttons->addButton(tr("Import Selection"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Skip"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);
    connect(copyButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        m_idsTargetStatusLabel->setText(tr("Target DLL changed, migration skipped."));
        return;
    }

    int copied = 0;
    QString errorMessage;
    for (int row = 0; row < list->count(); ++row) {
        QListWidgetItem *item = list->item(row);
        if (item->checkState() != Qt::Checked)
            continue;
        const int entryIndex = item->data(Qt::UserRole).toInt();
        if (entryIndex < 0 || entryIndex >= flatlasEntries.size())
            continue;

        const IdsEntryRecord &entry = flatlasEntries.at(entryIndex);
        int newGlobalId = 0;
        if (entry.hasStringValue) {
            if (!IdsDataService::writeStringEntry(dataset, targetDll, 0, entry.stringValue, &newGlobalId, &errorMessage)) {
                QMessageBox::warning(this, tr("IDS/Infocards"), errorMessage);
                return;
            }
            ++copied;
        }
        if (entry.hasHtmlValue) {
            if (!IdsDataService::writeInfocardEntry(dataset, targetDll, 0, entry.htmlValue, &newGlobalId, &errorMessage)) {
                QMessageBox::warning(this, tr("IDS/Infocards"), errorMessage);
                return;
            }
            ++copied;
        }
    }

    m_idsTargetStatusLabel->setText(tr("%1 entries were created in %2.").arg(copied).arg(targetDll));
}

void SettingsDialog::refreshLanguageCombo()
{
    if (!m_languageCombo)
        return;

    const QString current = m_languageCombo->currentText().trimmed().isEmpty()
        ? flatlas::core::Config::instance().getString(QStringLiteral("language"), flatlas::core::I18n::instance().currentLanguage())
        : m_languageCombo->currentText();
    m_languageCombo->clear();
    m_languageCombo->addItems(flatlas::core::I18n::availableLanguages());
    const int index = m_languageCombo->findText(current);
    if (index >= 0)
        m_languageCombo->setCurrentIndex(index);
}

void SettingsDialog::updateLanguagesFromGitHub()
{
    if (!m_network)
        return;

    m_languageUpdateButton->setEnabled(false);
    m_languageStatusLabel->setText(tr("Downloading language catalog..."));

    QNetworkRequest request(QUrl(QString::fromLatin1(kLanguageCatalogUrl)));
    request.setRawHeader("User-Agent", "FLAtlas-V2");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray indexBytes = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        if (!error.isEmpty()) {
            m_languageUpdateButton->setEnabled(true);
            m_languageStatusLabel->setText(tr("Language update failed: %1").arg(error));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(indexBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            m_languageUpdateButton->setEnabled(true);
            m_languageStatusLabel->setText(tr("Language catalog is invalid."));
            return;
        }

        const QJsonArray languages = doc.object().value(QStringLiteral("languages")).toArray();
        if (languages.isEmpty()) {
            m_languageUpdateButton->setEnabled(true);
            m_languageStatusLabel->setText(tr("Language catalog does not contain any languages."));
            return;
        }

        QDir().mkpath(languageCacheDirectory());
        QFile indexFile(QDir(languageCacheDirectory()).absoluteFilePath(QStringLiteral("index.json")));
        if (indexFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            indexFile.write(indexBytes);

        auto *remaining = new int(0);
        auto *failed = new bool(false);
        const QUrl baseUrl(QString::fromLatin1(kLanguageCatalogUrl));

        for (const QJsonValue &value : languages) {
            const QString fileName = QFileInfo(value.toObject().value(QStringLiteral("file")).toString()).fileName();
            if (fileName.isEmpty() || !fileName.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
                continue;

            ++(*remaining);
            QNetworkRequest fileRequest(baseUrl.resolved(QUrl(fileName)));
            fileRequest.setRawHeader("User-Agent", "FLAtlas-V2");
            fileRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            auto *fileReply = m_network->get(fileRequest);
            connect(fileReply, &QNetworkReply::finished, this, [this, fileReply, fileName, remaining, failed]() {
                const QByteArray bytes = fileReply->readAll();
                const QString fileError = fileReply->error() == QNetworkReply::NoError ? QString() : fileReply->errorString();
                fileReply->deleteLater();

                if (!fileError.isEmpty()) {
                    *failed = true;
                } else {
                    QJsonParseError fileParseError;
                    const QJsonDocument languageDoc = QJsonDocument::fromJson(bytes, &fileParseError);
                    if (fileParseError.error != QJsonParseError::NoError || !languageDoc.isObject()) {
                        *failed = true;
                    } else {
                        QFile target(QDir(languageCacheDirectory()).absoluteFilePath(fileName));
                        if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                            *failed = true;
                        } else {
                            target.write(bytes);
                        }
                    }
                }

                --(*remaining);
                if (*remaining > 0)
                    return;

                m_languageUpdateButton->setEnabled(true);
                refreshLanguageCombo();
                m_languageStatusLabel->setText(*failed ? tr("Languages updated with errors.") : tr("Languages updated."));
                delete remaining;
                delete failed;
            });
        }

        if (*remaining == 0) {
            m_languageUpdateButton->setEnabled(true);
            m_languageStatusLabel->setText(tr("Language catalog does not contain any downloadable files."));
            delete remaining;
            delete failed;
        }
    });
}

void SettingsDialog::updateThemeColorButton(const QString &key)
{
    QPushButton *button = m_themeColorButtons.value(key);
    if (!button)
        return;

    const QColor color = flatlas::core::ThemeColors::color(key);
    const QString colorName = color.name(QColor::HexRgb);
    button->setProperty("themeColorValue", colorName);
    button->setText(colorName.toUpper());
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid rgba(0,0,0,90);"
        "  min-width: 92px;"
        "  padding: 5px 10px;"
        "}"
    ).arg(colorName, color.lightness() > 145 ? QStringLiteral("#101418") : QStringLiteral("#ffffff")));
}

QString SettingsDialog::toolsDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::home().absoluteFilePath(QStringLiteral("AppData/Local"));
    return QDir(base).absoluteFilePath(QStringLiteral("FLAtlas-Tools"));
}

void SettingsDialog::startSuiteDownload(const QString &key, const QString &name, const QString &repoApiUrl)
{
    m_suiteStatusLabel->setText(tr("Searching latest release for %1...").arg(name));
    QNetworkRequest request{QUrl(repoApiUrl)};
    request.setRawHeader("User-Agent", "FLAtlas-V2");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, name]() {
        const QByteArray bytes = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        if (!error.isEmpty()) {
            m_suiteStatusLabel->setText(tr("Release could not be loaded: %1").arg(error));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        const QJsonArray assets = doc.object().value(QStringLiteral("assets")).toArray();
        QUrl assetUrl;
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
            const QString assetName = asset.value(QStringLiteral("name")).toString();
            if (!url.isEmpty() && !assetName.endsWith(QStringLiteral(".sha256"), Qt::CaseInsensitive)) {
                assetUrl = QUrl(url);
                break;
            }
        }
        if (!assetUrl.isValid()) {
            const QString htmlUrl = doc.object().value(QStringLiteral("html_url")).toString();
            if (!htmlUrl.isEmpty())
                QDesktopServices::openUrl(QUrl(htmlUrl));
            m_suiteStatusLabel->setText(tr("No release asset found. Release page was opened."));
            return;
        }
        downloadReleaseAsset(key, assetUrl);
    });
}

void SettingsDialog::downloadReleaseAsset(const QString &name, const QUrl &url)
{
    QDir().mkpath(toolsDirectory());
    const QString targetPath = QDir(toolsDirectory()).absoluteFilePath(fileNameFromUrl(url));
    QString displayName = name;
    for (const SuiteApp &app : suiteApps()) {
        if (app.key == name) {
            displayName = app.name;
            break;
        }
    }
    m_suiteStatusLabel->setText(tr("Downloading %1...").arg(displayName));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "FLAtlas-V2");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    auto *progress = new QProgressDialog(tr("Downloading %1...").arg(displayName), tr("Cancel"), 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::downloadProgress, progress, [progress](qint64 received, qint64 total) {
        if (total <= 0)
            return;
        progress->setValue(static_cast<int>((received * 100) / total));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, progress, targetPath, name, displayName]() {
        progress->close();
        progress->deleteLater();
        const QByteArray bytes = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        if (!error.isEmpty()) {
            m_suiteStatusLabel->setText(tr("Download failed: %1").arg(error));
            return;
        }
        QFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_suiteStatusLabel->setText(tr("File could not be written: %1").arg(targetPath));
            return;
        }
        file.write(bytes);
        file.close();
        const QFileInfo downloaded(targetPath);
        const QString installDir = QDir(toolsDirectory()).absoluteFilePath(name);
        QDir().mkpath(installDir);
        QString exePath;
        if (downloaded.suffix().compare(QStringLiteral("zip"), Qt::CaseInsensitive) == 0) {
            QDir install(installDir);
            install.removeRecursively();
            QDir().mkpath(installDir);
            const QString escapedArchive = QString(targetPath).replace(QLatin1Char('\''), QStringLiteral("''"));
            const QString escapedInstallDir = QString(installDir).replace(QLatin1Char('\''), QStringLiteral("''"));
            const QStringList args = {
                QStringLiteral("-NoProfile"),
                QStringLiteral("-ExecutionPolicy"),
                QStringLiteral("Bypass"),
                QStringLiteral("-Command"),
                QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                    .arg(escapedArchive, escapedInstallDir)
            };
            const int exitCode = QProcess::execute(QStringLiteral("powershell"), args);
            if (exitCode != 0) {
                m_suiteStatusLabel->setText(tr("Entpacken fehlgeschlagen: %1").arg(targetPath));
                return;
            }
            exePath = firstExeInDirectory(installDir);
        } else if (downloaded.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0) {
            exePath = targetPath;
        }
        if (exePath.isEmpty()) {
            m_suiteStatusLabel->setText(tr("Download saved, but no EXE found: %1").arg(targetPath));
            return;
        }
        registerInstalledTool(name, displayName, installDir, exePath);
        updateSuiteButtons();
        m_suiteStatusLabel->setText(tr("%1 installiert: %2").arg(displayName, installDir));
    });
}

void SettingsDialog::updateSuiteButtons()
{
    for (const SuiteApp &app : suiteApps()) {
        QPushButton *button = m_suiteButtons.value(app.key);
        if (!button)
            continue;
        button->setText(installedToolExe(app.key).isEmpty() ? tr("Download") : tr("Open"));
    }
}

void SettingsDialog::openInstalledTool(const QString &key)
{
    const QString exePath = installedToolExe(key);
    if (exePath.isEmpty() || !QFileInfo::exists(exePath)) {
        m_suiteStatusLabel->setText(tr("Tool was not found. Please download it again."));
        return;
    }
    QProcess::startDetached(exePath, {}, QFileInfo(exePath).absolutePath());
}

void SettingsDialog::registerInstalledTool(const QString &key, const QString &name, const QString &installDir, const QString &exePath)
{
    auto &config = flatlas::core::Config::instance();
    QJsonObject tools = config.getJsonObject(QStringLiteral("externalTools"));
    QJsonObject tool;
    tool.insert(QStringLiteral("name"), name);
    tool.insert(QStringLiteral("installDir"), installDir);
    tool.insert(QStringLiteral("exePath"), exePath);
    tools.insert(key, tool);
    config.setJsonObject(QStringLiteral("externalTools"), tools);
    config.save();
}

} // namespace flatlas::ui
