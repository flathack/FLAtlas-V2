#include "Application.h"
#include <QStyle>
#include <QIcon>

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_currentTheme(QStringLiteral("dark"))
    , m_currentLanguage(QStringLiteral("de"))
{
    setApplicationName(QStringLiteral("FLAtlas"));
    setApplicationVersion(QStringLiteral("2.0.0"));
    setOrganizationName(QStringLiteral("FLAtlas"));
    setOrganizationDomain(QStringLiteral("flatlas.flathack.de"));

    // Fusion-Style als Basis (wie im Python-Projekt)
    setStyle(QStringLiteral("Fusion"));

    // App-Icon setzen
    QIcon appIcon;
    appIcon.addFile(QStringLiteral(":/icons/flatlas_16.png"), QSize(16, 16));
    appIcon.addFile(QStringLiteral(":/icons/flatlas_32.png"), QSize(32, 32));
    appIcon.addFile(QStringLiteral(":/icons/flatlas_48.png"), QSize(48, 48));
    appIcon.addFile(QStringLiteral(":/icons/flatlas_256.png"), QSize(256, 256));
    setWindowIcon(appIcon);

    parseCommandLine();
    initConfig();
    initTranslations();
}

void Application::applyTheme()
{
    // TODO Phase 1: Theme aus Config laden und QSS anwenden
    // Platzhalter: Dark-Theme-Palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    setPalette(darkPalette);
}

void Application::setTheme(const QString &themeName)
{
    m_currentTheme = themeName;
    applyTheme();
}

void Application::setLanguage(const QString &languageCode)
{
    m_currentLanguage = languageCode;
    // TODO Phase 1: QTranslator austauschen, retranslateUi() aufrufen
}

void Application::initConfig()
{
    // TODO Phase 1: Config aus JSON laden
}

void Application::initTranslations()
{
    // TODO Phase 1: QTranslator mit .qm-Datei laden
}

void Application::parseCommandLine()
{
    // TODO: --open-system, --test-updater-zip
}
