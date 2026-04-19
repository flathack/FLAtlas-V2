#include "IdsStringTable.h"
namespace flatlas::infrastructure {
void IdsStringTable::loadFromDll(const QString &) {} // TODO Phase 12
QString IdsStringTable::getString(int) const { return {}; }
bool IdsStringTable::hasString(int) const { return false; }
int IdsStringTable::count() const { return m_strings.size(); }
} // namespace flatlas::infrastructure
