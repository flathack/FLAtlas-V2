#pragma once
// rendering/preview/ModelCache.h – LRU-Cache für 3D-Modelle (Phase 8)

#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QHash>
#include <QList>
#include "infrastructure/io/CmpLoader.h"

namespace flatlas::rendering {

/// Thread-safe LRU cache for loaded 3D models (keyed by file path).
class ModelCache {
public:
    static ModelCache &instance();

    /// Check if a model is cached.
    bool contains(const QString &key) const;

    /// Retrieve a cached decoded model. Returns empty data if not found.
    flatlas::infrastructure::DecodedModel get(const QString &key);

    /// Insert a model into the cache. Evicts oldest if at capacity.
    void insert(const QString &key, const flatlas::infrastructure::DecodedModel &model);

    /// Load a model (from cache or disk). Automatically caches.
    flatlas::infrastructure::DecodedModel load(const QString &filePath);

    void setMaxSize(int maxEntries);
    int maxSize() const { return m_maxSize; }
    int size() const;
    void clear();

private:
    ModelCache() = default;
    void evictIfNeeded();

    mutable QMutex m_mutex;
    int m_maxSize = 128;
    QHash<QString, flatlas::infrastructure::DecodedModel> m_cache;
    QList<QString> m_accessOrder; // Most recently used at back
};

} // namespace flatlas::rendering
