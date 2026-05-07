#include "Config.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace flatlas::core {
namespace {

constexpr int maxBackupCount = 10;

QString configPathSettingsKey()
{
    return QStringLiteral("config/path");
}

QString backupsDirectoryFor(const QString &path)
{
    return QDir(QFileInfo(path).absolutePath()).absoluteFilePath(QStringLiteral("backups"));
}

void pruneBackups(const QString &path)
{
    QDir dir(backupsDirectoryFor(path));
    const QString baseName = QFileInfo(path).completeBaseName();
    const QFileInfoList backups = dir.entryInfoList({baseName + QStringLiteral("-*.json")},
                                                    QDir::Files,
                                                    QDir::Time);
    for (int i = maxBackupCount; i < backups.size(); ++i)
        QFile::remove(backups.at(i).absoluteFilePath());
}

} // namespace

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

QString Config::configuredConfigPath()
{
    return QSettings().value(configPathSettingsKey(), defaultConfigPath()).toString();
}

bool Config::load(const QString &path)
{
    m_filePath = path.isEmpty() ? configuredConfigPath() : path;
    m_data = {};

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

    if (path.isEmpty() && QFileInfo::exists(target))
        createBackup();

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    QJsonDocument doc(m_data);
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0)
        return false;
    return file.commit();
}

bool Config::setConfigPath(const QString &path)
{
    if (path.trimmed().isEmpty())
        return false;

    const QString target = QFileInfo(path).absoluteFilePath();
    QDir().mkpath(QFileInfo(target).absolutePath());
    QSettings().setValue(configPathSettingsKey(), target);
    m_filePath = target;
    return save();
}

bool Config::importFrom(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    m_data = doc.object();
    return save();
}

bool Config::exportTo(const QString &path) const
{
    return save(path);
}

bool Config::createBackup(QString *backupPath) const
{
    if (m_filePath.isEmpty() || !QFileInfo::exists(m_filePath))
        return false;

    const QString backupDir = backupsDirectoryFor(m_filePath);
    QDir().mkpath(backupDir);

    const QFileInfo sourceInfo(m_filePath);
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString target = QDir(backupDir).absoluteFilePath(
        QStringLiteral("%1-%2.json").arg(sourceInfo.completeBaseName(), timestamp));
    const bool copied = QFile::copy(m_filePath, target);
    if (copied) {
        if (backupPath)
            *backupPath = target;
        pruneBackups(m_filePath);
    }
    return copied;
}

QString Config::filePath() const
{
    return m_filePath;
}

QJsonObject Config::data() const
{
    return m_data;
}

void Config::setData(const QJsonObject &data)
{
    m_data = data;
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

QJsonArray Config::getJsonArray(const QString &key, const QJsonArray &defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isArray()) ? it->toArray() : defaultValue;
}

void Config::setJsonArray(const QString &key, const QJsonArray &value)
{
    m_data[key] = value;
}

QJsonObject Config::getJsonObject(const QString &key, const QJsonObject &defaultValue) const
{
    auto it = m_data.constFind(key);
    return (it != m_data.constEnd() && it->isObject()) ? it->toObject() : defaultValue;
}

void Config::setJsonObject(const QString &key, const QJsonObject &value)
{
    m_data[key] = value;
}

void Config::clear()
{
    m_data = {};
}

} // namespace flatlas::core
