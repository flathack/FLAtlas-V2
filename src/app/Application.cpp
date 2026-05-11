#include "Application.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "core/Theme.h"
#include "core/I18n.h"
#include <QDir>
#include <QStandardPaths>
#include <QStyle>
#include <QIcon>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_currentTheme(QStringLiteral("dark"))
    , m_currentLanguage(QStringLiteral("en"))
{
    setApplicationName(QStringLiteral("FLAtlas"));
    setApplicationVersion(QStringLiteral("0.8.0.2"));
    setOrganizationName(QStringLiteral("FLAtlas"));
    setOrganizationDomain(QStringLiteral("flatlas.flathack.de"));

    QDir logDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    logDir.mkpath(QStringLiteral("."));
    flatlas::core::Logger::init(logDir.filePath(QStringLiteral("FLAtlas-Change.log")));

    // Fusion-Style als Basis (wie im Python-Projekt)
    setStyle(QStringLiteral("Fusion"));

    // App-Icon setzen
    QIcon appIcon(QStringLiteral(":/icons/FLAtlas-V2-ICON.ico"));
    if (!appIcon.isNull())
        setWindowIcon(appIcon);

    parseCommandLine();
    initConfig();
    initTranslations();
}

void Application::applyTheme()
{
    auto &config = flatlas::core::Config::instance();
    QString theme = config.getString("theme", "dark");
    flatlas::core::Theme::instance().apply(theme);
    m_currentTheme = theme;
}

void Application::setTheme(const QString &themeName)
{
    m_currentTheme = themeName;
    flatlas::core::Theme::instance().apply(themeName);
    flatlas::core::Config::instance().setString("theme", themeName);
    flatlas::core::Config::instance().save();
}

void Application::setLanguage(const QString &languageCode)
{
    m_currentLanguage = languageCode;
    flatlas::core::I18n::instance().setLanguage(languageCode);
    flatlas::core::Config::instance().setString("language", languageCode);
    flatlas::core::Config::instance().save();
}

void Application::initConfig()
{
    flatlas::core::Config::instance().load();
}

void Application::initTranslations()
{
    auto &config = flatlas::core::Config::instance();
    QString lang = config.getString("language", "en");
    flatlas::core::I18n::instance().setLanguage(lang);
    m_currentLanguage = lang;
}

void Application::parseCommandLine()
{
    // TODO: --open-system, --test-updater-zip
}
