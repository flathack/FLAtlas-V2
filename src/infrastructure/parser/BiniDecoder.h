#pragma once
// infrastructure/parser/BiniDecoder.h – BINI-Binärformat-Decoder
#include <QByteArray>
#include <QString>

namespace flatlas::infrastructure {

class BiniDecoder
{
public:
    /// Prüft ob Daten BINI-Format sind (Magic: "BINI").
    static bool isBini(const QByteArray &data);

    /// Prüft ob eine Datei BINI-Format hat (liest erste 4 Bytes).
    static bool isBiniFile(const QString &filePath);

    /// BINI-Daten dekodieren → INI-Text.
    static QString decode(const QByteArray &data);

private:
    /// Liest null-terminierten CP1252-String aus der String-Tabelle.
    static QString getCStr(const QByteArray &strTable, int offset);

    /// Formatiert float wie Python "%.7g", hängt ".0" an wenn nötig.
    static QString fmtFloat(float v);
};

} // namespace flatlas::infrastructure
