#include "IniParser.h"
// TODO Phase 2: Vollständige Implementierung

namespace flatlas::infrastructure {

QStringList IniSection::values(const QString &key) const
{
    QStringList result;
    for (const auto &e : entries)
        if (e.first.compare(key, Qt::CaseInsensitive) == 0)
            result.append(e.second);
    return result;
}

QString IniSection::value(const QString &key, const QString &defaultValue) const
{
    for (const auto &e : entries)
        if (e.first.compare(key, Qt::CaseInsensitive) == 0)
            return e.second;
    return defaultValue;
}

IniDocument IniParser::parseFile(const QString & /*filePath*/)
{
    // TODO Phase 2
    return {};
}

IniDocument IniParser::parseText(const QString & /*text*/)
{
    // TODO Phase 2
    return {};
}

QString IniParser::serialize(const IniDocument & /*doc*/)
{
    // TODO Phase 2
    return {};
}

} // namespace flatlas::infrastructure
