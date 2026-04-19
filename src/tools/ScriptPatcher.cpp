#include "ScriptPatcher.h"
namespace flatlas::tools {
bool ScriptPatcher::applyOpenSpPatch(const QString &) { return false; } // TODO Phase 21
bool ScriptPatcher::applyResolutionPatch(const QString &, int, int) { return false; }
bool ScriptPatcher::revertPatch(const QString &, const QString &) { return false; }
} // namespace flatlas::tools
