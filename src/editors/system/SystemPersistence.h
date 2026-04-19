#pragma once
// editors/system/SystemPersistence.h – Laden/Speichern von System-INI-Dateien

#include <memory>
#include <QHash>
#include <QString>
#include "../../infrastructure/parser/IniParser.h"

namespace flatlas::domain { class SystemDocument; }

namespace flatlas::editors {

/// Lädt und speichert Freelancer-System-INI-Dateien in/aus SystemDocument.
class SystemPersistence
{
public:
    /// Lädt eine System-INI-Datei in ein SystemDocument. Gibt nullptr bei Fehler zurück.
    static std::unique_ptr<flatlas::domain::SystemDocument> load(const QString &filePath);

    /// Speichert ein SystemDocument als INI-Datei. Gibt true bei Erfolg zurück.
    static bool save(const flatlas::domain::SystemDocument &doc, const QString &filePath);

    /// Speichert mit dem im Dokument hinterlegten Dateipfad.
    static bool save(const flatlas::domain::SystemDocument &doc);

    /// Gibt die gespeicherten Extra-Sections für ein Dokument zurück.
    static flatlas::infrastructure::IniDocument extraSections(const flatlas::domain::SystemDocument *doc);

    /// Entfernt gespeicherte Extra-Sections (Aufräumen bei Dokumentschließung).
    static void clearExtras(const flatlas::domain::SystemDocument *doc);

private:
    static QHash<const flatlas::domain::SystemDocument *,
                 flatlas::infrastructure::IniDocument> s_extras;
};

} // namespace flatlas::editors
