#pragma once
// infrastructure/parser/IniParser.h – Freelancer-INI-Parser (Duplikat-Keys)
// TODO Phase 2

#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

namespace flatlas::infrastructure {

/// Ein Key-Value-Paar in einer INI-Section.
using IniEntry = QPair<QString, QString>;

/// Eine INI-Section mit Name und Einträgen (Duplikat-Keys erlaubt).
struct IniSection {
    QString name;
    QVector<IniEntry> entries;

    /// Alle Werte für einen Key (case-insensitive).
    QStringList values(const QString &key) const;

    /// Ersten Wert für einen Key.
    QString value(const QString &key, const QString &defaultValue = {}) const;
};

/// Ergebnis des INI-Parsens: Liste von Sections.
using IniDocument = QVector<IniSection>;

class IniParser
{
public:
    /// INI-Datei parsen. Erkennt automatisch BINI-Format.
    static IniDocument parseFile(const QString &filePath);

    /// INI-Text parsen.
    static IniDocument parseText(const QString &text);

    /// IniDocument zurück in INI-Text serialisieren.
    static QString serialize(const IniDocument &doc);
};

} // namespace flatlas::infrastructure
