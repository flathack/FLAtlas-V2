#include "SettingsDialog.h"

#include "core/Config.h"
#include "core/I18n.h"
#include "core/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
#include <QUrl>
#include <QVBoxLayout>

namespace flatlas::ui {
namespace {

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
        {QStringLiteral("newsRumorEditor"), QObject::tr("News/Rumor Editor"), false},
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
    m_languageCombo->addItems(flatlas::core::I18n::availableLanguages());
    generalLayout->addRow(tr("Sprache:"), m_languageCombo);

    m_themeCombo = new QComboBox(generalTab);
    m_themeCombo->addItems(flatlas::core::Theme::instance().availableThemes());
    generalLayout->addRow(tr("Design:"), m_themeCombo);

    m_updateCheckBox = new QCheckBox(tr("Update-Check beim Start aktivieren"), generalTab);
    generalLayout->addRow(QString(), m_updateCheckBox);

    m_restoreTabsCheckBox = new QCheckBox(tr("Offene Tabs beim Start wiederherstellen"), generalTab);
    generalLayout->addRow(QString(), m_restoreTabsCheckBox);
    tabs->addTab(generalTab, tr("Allgemein"));

    auto *pinnedTab = new QWidget(tabs);
    auto *pinnedLayout = new QVBoxLayout(pinnedTab);
    auto *pinnedHint = new QLabel(tr("Ausgewaehlte Tools werden dauerhaft als Tabs angezeigt. Der Mod Manager ist immer aktiv."), pinnedTab);
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
        auto *button = new QPushButton(app.websiteUrl.isEmpty() ? tr("Download") : tr("Oeffnen"), row);
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

    auto *resetTab = new QWidget(tabs);
    auto *resetLayout = new QVBoxLayout(resetTab);
    auto *resetHint = new QLabel(tr("Setzt FLAtlas auf Werkseinstellungen zurueck. Mod-Installationen und Spieldaten werden nicht geloescht."), resetTab);
    resetHint->setWordWrap(true);
    resetLayout->addWidget(resetHint);
    auto *resetButton = new QPushButton(tr("Programm auf Werkseinstellung zuruecksetzen"), resetTab);
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
}

void SettingsDialog::saveSettings()
{
    auto &config = flatlas::core::Config::instance();
    const QString oldPinned = config.getStringList(QStringLiteral("pinnedTools"), defaultPinnedTools()).join(QLatin1Char('|'));

    const QString theme = m_themeCombo->currentText();
    const QString lang = m_languageCombo->currentText();
    flatlas::core::Theme::instance().apply(theme);
    flatlas::core::I18n::instance().setLanguage(lang);
    config.setString(QStringLiteral("theme"), theme);
    config.setString(QStringLiteral("language"), lang);
    config.setBool(QStringLiteral("updateCheckEnabled"), m_updateCheckBox->isChecked());
    config.setBool(QStringLiteral("restoreOpenTabs"), m_restoreTabsCheckBox->isChecked());

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
}

void SettingsDialog::resetToDefaults()
{
    const auto answer = QMessageBox::question(this,
                                              tr("Werkseinstellungen"),
                                              tr("FLAtlas wirklich auf Werkseinstellungen zuruecksetzen?"));
    if (answer != QMessageBox::Yes)
        return;
    auto &config = flatlas::core::Config::instance();
    config.clear();
    config.setString(QStringLiteral("theme"), QStringLiteral("dark"));
    config.setString(QStringLiteral("language"), QStringLiteral("en"));
    config.setBool(QStringLiteral("updateCheckEnabled"), true);
    config.setBool(QStringLiteral("restoreOpenTabs"), false);
    config.setStringList(QStringLiteral("pinnedTools"), defaultPinnedTools());
    config.save();
    QSettings().clear();
    flatlas::core::Theme::instance().apply(QStringLiteral("dark"));
    flatlas::core::I18n::instance().setLanguage(QStringLiteral("en"));
    m_resetRequested = true;
    m_pinnedToolsChanged = true;
    loadSettings();
    QMessageBox::information(this, tr("Werkseinstellungen"), tr("Die Einstellungen wurden zurueckgesetzt."));
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
    m_suiteStatusLabel->setText(tr("Suche letztes Release fuer %1...").arg(name));
    QNetworkRequest request{QUrl(repoApiUrl)};
    request.setRawHeader("User-Agent", "FLAtlas-V2");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, name]() {
        const QByteArray bytes = reply->readAll();
        const QString error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        reply->deleteLater();
        if (!error.isEmpty()) {
            m_suiteStatusLabel->setText(tr("Release konnte nicht geladen werden: %1").arg(error));
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
            m_suiteStatusLabel->setText(tr("Kein Release-Asset gefunden. Release-Seite wurde geoeffnet."));
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
    m_suiteStatusLabel->setText(tr("Lade %1 herunter...").arg(displayName));
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "FLAtlas-V2");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network->get(request);
    auto *progress = new QProgressDialog(tr("%1 wird heruntergeladen...").arg(displayName), tr("Abbrechen"), 0, 100, this);
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
            m_suiteStatusLabel->setText(tr("Download fehlgeschlagen: %1").arg(error));
            return;
        }
        QFile file(targetPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_suiteStatusLabel->setText(tr("Datei konnte nicht geschrieben werden: %1").arg(targetPath));
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
            m_suiteStatusLabel->setText(tr("Download gespeichert, aber keine EXE gefunden: %1").arg(targetPath));
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
        button->setText(installedToolExe(app.key).isEmpty() ? tr("Download") : tr("Oeffnen"));
    }
}

void SettingsDialog::openInstalledTool(const QString &key)
{
    const QString exePath = installedToolExe(key);
    if (exePath.isEmpty() || !QFileInfo::exists(exePath)) {
        m_suiteStatusLabel->setText(tr("Tool wurde nicht gefunden. Bitte erneut herunterladen."));
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
