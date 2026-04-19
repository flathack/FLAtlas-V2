#pragma once
// infrastructure/parser/BiniConverter.h – Bulk BINI→INI Konvertierung
#include <QString>
namespace flatlas::infrastructure {
class BiniConverter {
public:
    /// Alle BINI-Dateien in einem Ordner nach INI konvertieren.
    static int convertDirectory(const QString &inputDir, const QString &outputDir);
    /// Einzelne BINI-Datei konvertieren.
    static bool convertFile(const QString &inputPath, const QString &outputPath);
    /// BINI-Dateien in einem Ordner zählen (ohne Konvertierung).
    static int countBiniFiles(const QString &directory);
};
} // namespace flatlas::infrastructure
