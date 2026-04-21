// rendering/preview/ModelCache.cpp – LRU-Cache für 3D-Modelle (Phase 8)

#include "ModelCache.h"
#include <QFileInfo>

namespace flatlas::rendering {

ModelCache &ModelCache::instance()
{
    static ModelCache c;
    return c;
}

bool ModelCache::contains(const QString &key) const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.contains(key);
}

flatlas::infrastructure::DecodedModel ModelCache::get(const QString &key)
{
    QMutexLocker lock(&m_mutex);
    if (!m_cache.contains(key))
        return {};

    // Move to back of access order (most recently used)
    m_accessOrder.removeOne(key);
    m_accessOrder.append(key);

    return m_cache.value(key);
}

void ModelCache::insert(const QString &key, const flatlas::infrastructure::DecodedModel &model)
{
    QMutexLocker lock(&m_mutex);

    if (m_cache.contains(key)) {
        m_cache[key] = model;
        m_accessOrder.removeOne(key);
        m_accessOrder.append(key);
        return;
    }

    evictIfNeeded();
    m_cache.insert(key, model);
    m_accessOrder.append(key);
}

flatlas::infrastructure::DecodedModel ModelCache::load(const QString &filePath)
{
    // Check cache first (without holding lock for file I/O)
    {
        QMutexLocker lock(&m_mutex);
        if (m_cache.contains(filePath)) {
            m_accessOrder.removeOne(filePath);
            m_accessOrder.append(filePath);
            return m_cache.value(filePath);
        }
    }

    // Load from disk
    const auto decoded = flatlas::infrastructure::CmpLoader::loadModel(filePath);

    // Cache the result
    if (decoded.isValid())
        insert(filePath, decoded);

    return decoded;
}

void ModelCache::setMaxSize(int maxEntries)
{
    QMutexLocker lock(&m_mutex);
    m_maxSize = maxEntries;
    evictIfNeeded();
}

int ModelCache::size() const
{
    QMutexLocker lock(&m_mutex);
    return m_cache.size();
}

void ModelCache::clear()
{
    QMutexLocker lock(&m_mutex);
    m_cache.clear();
    m_accessOrder.clear();
}

void ModelCache::evictIfNeeded()
{
    // Must be called with m_mutex held
    while (m_cache.size() >= m_maxSize && !m_accessOrder.isEmpty()) {
        QString oldest = m_accessOrder.takeFirst();
        m_cache.remove(oldest);
    }
}

} // namespace flatlas::rendering
