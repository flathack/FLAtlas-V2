#pragma once

#include <QJsonObject>
#include <QString>

/// Anwendungskonfiguration – Singleton-Zugriff auf config.json.
class AppConfig
{
public:
    static AppConfig &instance();

    /// Config aus Datei laden.
    bool load();

    /// Config in Datei speichern.
    bool save() const;

    // --- Getter/Setter ---
    QString theme() const;
    void setTheme(const QString &theme);

    QString language() const;
    void setLanguage(const QString &lang);

    QString lastOpenedPath() const;
    void setLastOpenedPath(const QString &path);

    QStringList recentFiles() const;
    void addRecentFile(const QString &path);

    /// Pfad zur Config-Datei.
    static QString configFilePath();

private:
    AppConfig() = default;
    QJsonObject m_data;
};
