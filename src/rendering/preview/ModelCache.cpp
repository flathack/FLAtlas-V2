#include "ModelCache.h"
namespace flatlas::rendering {
ModelCache &ModelCache::instance() { static ModelCache c; return c; }
bool ModelCache::contains(const QString &) const { return false; } // TODO Phase 8
void ModelCache::insert(const QString &, const flatlas::infrastructure::ModelNode &) {} // TODO
void ModelCache::setMaxSize(int s) { m_maxSize = s; }
void ModelCache::clear() {}
} // namespace flatlas::rendering
