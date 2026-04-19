#pragma once
// core/Config.h – JSON-basierte Konfiguration mit Legacy-Migration
// TODO Phase 1: Vollständige Implementierung

#include <QJsonObject>
#include <QString>

namespace flatlas::core {

class Config
{
public:
    static Config &instance();

    bool load(const QString &path = {});
    bool save(const QString &path = {}) const;

    QString getString(const QString &key, const QString &defaultValue = {}) const;
    void setString(const QString &key, const QString &value);

    int getInt(const QString &key, int defaultValue = 0) const;
    void setInt(const QString &key, int value);

    bool getBool(const QString &key, bool defaultValue = false) const;
    void setBool(const QString &key, bool value);

private:
    Config() = default;
    QJsonObject m_data;
    QString m_filePath;
};

} // namespace flatlas::core
