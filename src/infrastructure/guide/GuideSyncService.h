#pragma once

#include "infrastructure/guide/GuideCache.h"
#include "infrastructure/guide/GuideJsonReader.h"

#include <QByteArray>
#include <QHash>
#include <QString>

namespace flatlas::infrastructure::guide {

class GuideSyncService
{
public:
    struct SyncPackage {
        QByteArray manifestJson;
        QHash<QString, QByteArray> articlesByPath;
    };

    explicit GuideSyncService(GuideCache cache);

    bool applyPackage(const SyncPackage &packageData, QString *errorMessage = nullptr) const;

private:
    GuideCache m_cache;
    GuideJsonReader m_reader;
};

} // namespace flatlas::infrastructure::guide
