#pragma once
// tools/SpStarter.h – Singleplayer-Starter
// TODO Phase 21
#include <QString>
namespace flatlas::tools {
class SpStarter {
public:
    static bool launch(const QString &exePath, const QString &args = {});
};
} // namespace flatlas::tools
