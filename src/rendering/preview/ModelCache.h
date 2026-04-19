#pragma once
// rendering/preview/ModelCache.h – LRU-Cache für 3D-Modelle
// TODO Phase 8
#include <QMutex>
#include <QString>
#include <QHash>
namespace flatlas::infrastructure { struct ModelNode; }
namespace flatlas::rendering {
class ModelCache {
public:
    static ModelCache &instance();
    bool contains(const QString &key) const;
    void insert(const QString &key, const flatlas::infrastructure::ModelNode &model);
    void setMaxSize(int maxEntries);
    void clear();
private:
    ModelCache() = default;
    mutable QMutex m_mutex;
    int m_maxSize = 128;
};
} // namespace flatlas::rendering
