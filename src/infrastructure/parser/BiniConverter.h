#pragma once
// infrastructure/parser/BiniConverter.h – Bulk BINI→INI Konvertierung
// TODO Phase 2
#include <QString>
namespace flatlas::infrastructure {
class BiniConverter {
public:
    /// Alle BINI-Dateien in einem Ordner nach INI konvertieren.
    static int convertDirectory(const QString &inputDir, const QString &outputDir);
};
} // namespace flatlas::infrastructure
