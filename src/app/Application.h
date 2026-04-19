#pragma once

#include <QApplication>
#include <QString>

/// QApplication-Subklasse mit Startup-Logik, Theme-Management und globaler Konfiguration.
class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application() override = default;

    /// Aktuelles Theme anwenden (aus Config geladen).
    void applyTheme();

    /// Theme wechseln und anwenden.
    void setTheme(const QString &themeName);

    /// Sprache wechseln.
    void setLanguage(const QString &languageCode);

private:
    void initConfig();
    void initTranslations();
    void parseCommandLine();

    QString m_currentTheme;
    QString m_currentLanguage;
};
