#pragma once
// infrastructure/parser/BiniDecoder.h – BINI-Binärformat-Decoder
// TODO Phase 2
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QPair>

namespace flatlas::infrastructure {

struct IniSection; // forward

class BiniDecoder
{
public:
    /// Prüft ob Daten BINI-Format sind (Magic: "BINI").
    static bool isBini(const QByteArray &data);

    /// BINI-Daten dekodieren → INI-Text.
    static QString decode(const QByteArray &data);
};

} // namespace flatlas::infrastructure
