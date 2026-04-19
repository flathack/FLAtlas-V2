#pragma once
// core/Config.h – JSON-basierte Konfiguration mit Legacy-Migration

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace flatlas::core {

class Config
{
public:
    static Config &instance();
    static QString defaultConfigPath();

    bool load(const QString &path = {});
    bool save(const QString &path = {}) const;

    QString getString(const QString &key, const QString &defaultValue = {}) const;
    void setString(const QString &key, const QString &value);

    int getInt(const QString &key, int defaultValue = 0) const;
    void setInt(const QString &key, int value);

    bool getBool(const QString &key, bool defaultValue = false) const;
    void setBool(const QString &key, bool value);

    double getDouble(const QString &key, double defaultValue = 0.0) const;
    void setDouble(const QString &key, double value);

    QStringList getStringList(const QString &key, const QStringList &defaultValue = {}) const;
    void setStringList(const QString &key, const QStringList &value);

private:
    Config() = default;
    bool mergeLegacy();
    QJsonObject m_data;
    QString m_filePath;
};

} // namespace flatlas::core
