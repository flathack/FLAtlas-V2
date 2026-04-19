#include "AppConfig.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>

AppConfig &AppConfig::instance()
{
    static AppConfig config;
    return config;
}

QString AppConfig::configFilePath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/config.json");
}

bool AppConfig::load()
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
        return false;

    m_data = doc.object();
    return true;
}

bool AppConfig::save() const
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    return true;
}

QString AppConfig::theme() const
{
    return m_data.value(QStringLiteral("theme")).toString(QStringLiteral("dark"));
}

void AppConfig::setTheme(const QString &theme)
{
    m_data[QStringLiteral("theme")] = theme;
}

QString AppConfig::language() const
{
    return m_data.value(QStringLiteral("language")).toString(QStringLiteral("de"));
}

void AppConfig::setLanguage(const QString &lang)
{
    m_data[QStringLiteral("language")] = lang;
}

QString AppConfig::lastOpenedPath() const
{
    return m_data.value(QStringLiteral("lastOpenedPath")).toString();
}

void AppConfig::setLastOpenedPath(const QString &path)
{
    m_data[QStringLiteral("lastOpenedPath")] = path;
}

QStringList AppConfig::recentFiles() const
{
    QStringList result;
    const auto arr = m_data.value(QStringLiteral("recentFiles")).toArray();
    for (const auto &v : arr)
        result.append(v.toString());
    return result;
}

void AppConfig::addRecentFile(const QString &path)
{
    auto list = recentFiles();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > 10)
        list.removeLast();

    QJsonArray arr;
    for (const auto &f : list)
        arr.append(f);
    m_data[QStringLiteral("recentFiles")] = arr;
}
