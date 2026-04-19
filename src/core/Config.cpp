#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

namespace flatlas::core {

Config &Config::instance()
{
    static Config config;
    return config;
}

QString Config::defaultConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/config.json");
}

bool Config::load(const QString &path)
{
    m_filePath = path.isEmpty() ? defaultConfigPath() : path;

    QFile file(m_filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();
        if (err.error == QJsonParseError::NoError && doc.isObject())
            m_data = doc.object();
    }

    bool merged = mergeLegacy();
    if (merged)
        save();

    return true;
}

bool Config::mergeLegacy()
{
#ifndef Q_OS_WIN
    return false;
#else
    const QStringList candidates = {
        QDir::homePath() + QStringLiteral("/AppData/Roaming/fl_editor/config.json"),
        qEnvironmentVariable("APPDATA") + QStringLiteral("/fl_editor/config.json"),
    };

    bool merged = false;
    for (const auto &legacyPath : candidates) {
        if (legacyPath == m_filePath || legacyPath.isEmpty())
            continue;
        QFile legacyFile(legacyPath);
        if (!legacyFile.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(legacyFile.readAll(), &err);
        legacyFile.close();
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        const auto legacyObj = doc.object();
        for (auto it = legacyObj.begin(); it != legacyObj.end(); ++it) {
            if (!m_data.contains(it.key())) {
                m_data[it.key()] = it.value();
                merged = true;
            }
        }
    }
    return merged;
#endif
}

bool Config::save(const QString &path) const
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (target.isEmpty())
        return false;

    QDir().mkpath(QFileInfo(target).absolutePath());

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QJsonDocument doc(m_data);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// --- getters / setters ---

QString Config::getString(const QString &key, const QString &defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isString()) ? it->toString() : defaultValue;
}

void Config::setString(const QString &key, const QString &value)
{
    m_data[key] = value;
}

int Config::getInt(const QString &key, int defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isDouble()) ? it->toInt() : defaultValue;
}

void Config::setInt(const QString &key, int value)
{
    m_data[key] = value;
}

bool Config::getBool(const QString &key, bool defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isBool()) ? it->toBool() : defaultValue;
}

void Config::setBool(const QString &key, bool value)
{
    m_data[key] = value;
}

double Config::getDouble(const QString &key, double defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isDouble()) ? it->toDouble() : defaultValue;
}

void Config::setDouble(const QString &key, double value)
{
    m_data[key] = value;
}

QStringList Config::getStringList(const QString &key, const QStringList &defaultValue) const
{
    auto it = m_data.constFind(key);
    if (it == m_data.constEnd() || !it->isArray())
        return defaultValue;

    QStringList result;
    const auto arr = it->toArray();
    result.reserve(arr.size());
    for (const auto &v : arr)
        result.append(v.toString());
    return result;
}

void Config::setStringList(const QString &key, const QStringList &value)
{
    QJsonArray arr;
    for (const auto &s : value)
        arr.append(s);
    m_data[key] = arr;
}

} // namespace flatlas::core
