#include "UniverseData.h"

namespace flatlas::domain {

void UniverseData::addSystem(const SystemInfo &info)
{
    systems.append(info);
}

const SystemInfo *UniverseData::findSystem(const QString &nickname) const
{
    for (const auto &s : systems) {
        if (s.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            return &s;
    }
    return nullptr;
}

SystemInfo *UniverseData::findSystem(const QString &nickname)
{
    for (auto &s : systems) {
        if (s.nickname.compare(nickname, Qt::CaseInsensitive) == 0)
            return &s;
    }
    return nullptr;
}

int UniverseData::systemCount() const
{
    return systems.size();
}

} // namespace flatlas::domain
