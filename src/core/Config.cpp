#include "Config.h"
// TODO Phase 1: Vollständige Implementierung

namespace flatlas::core {

Config &Config::instance()
{
    static Config config;
    return config;
}

bool Config::load(const QString & /*path*/) { return false; }
bool Config::save(const QString & /*path*/) const { return false; }
QString Config::getString(const QString & /*key*/, const QString &defaultValue) const { return defaultValue; }
void Config::setString(const QString & /*key*/, const QString & /*value*/) {}
int Config::getInt(const QString & /*key*/, int defaultValue) const { return defaultValue; }
void Config::setInt(const QString & /*key*/, int /*value*/) {}
bool Config::getBool(const QString & /*key*/, bool defaultValue) const { return defaultValue; }
void Config::setBool(const QString & /*key*/, bool /*value*/) {}

} // namespace flatlas::core
