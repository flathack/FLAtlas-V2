#pragma once
// tools/ScriptPatcher.h – OpenSP-Patch, Resolution-Patch
// TODO Phase 21
#include <QString>
namespace flatlas::tools {
class ScriptPatcher {
public:
    static bool applyOpenSpPatch(const QString &exePath);
    static bool applyResolutionPatch(const QString &exePath, int width, int height);
    static bool revertPatch(const QString &exePath, const QString &backupPath);
};
} // namespace flatlas::tools
