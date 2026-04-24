#pragma once
// editors/system/SystemPersistence.h - Laden/Speichern von System-INI-Dateien

#include <memory>
#include <QHash>
#include <QString>
#include "../../infrastructure/parser/IniParser.h"

namespace flatlas::domain { class SystemDocument; class SolarObject; class ZoneItem; }

namespace flatlas::editors {

/// Laedt und speichert Freelancer-System-INI-Dateien in/aus SystemDocument.
class SystemPersistence
{
public:
    /// Laedt eine System-INI-Datei in ein SystemDocument. Gibt nullptr bei Fehler zurueck.
    static std::unique_ptr<flatlas::domain::SystemDocument> load(const QString &filePath);

    /// Speichert ein SystemDocument als INI-Datei. Gibt true bei Erfolg zurueck.
    static bool save(const flatlas::domain::SystemDocument &doc, const QString &filePath);

    /// Speichert mit dem im Dokument hinterlegten Dateipfad.
    static bool save(const flatlas::domain::SystemDocument &doc);

    /// Gibt die gespeicherten Extra-Sections fuer ein Dokument zurueck.
    static flatlas::infrastructure::IniDocument extraSections(const flatlas::domain::SystemDocument *doc);

    /// Gibt die gespeicherte SystemInfo-Section fuer ein Dokument zurueck.
    static flatlas::infrastructure::IniSection systemInfoSection(const flatlas::domain::SystemDocument *doc);

    /// Ersetzt die gespeicherten Extra-Sections fuer ein Dokument.
    static void setExtraSections(const flatlas::domain::SystemDocument *doc,
                                 const flatlas::infrastructure::IniDocument &extras);

    /// Ersetzt die gespeicherte SystemInfo-Section fuer ein Dokument.
    static void setSystemInfoSection(const flatlas::domain::SystemDocument *doc,
                                     const flatlas::infrastructure::IniSection &section);

    /// Entfernt gespeicherte Extra-Sections und Layout-Metadaten.
    static void clearExtras(const flatlas::domain::SystemDocument *doc);

    /// Liefert true, wenn die geladene Section-Reihenfolge deutlich vom
    /// ueblichen Freelancer-Systemaufbau abweicht.
    static bool hasNonStandardSectionOrder(const flatlas::domain::SystemDocument *doc);

    /// Ordnet die intern gespeicherte Section-Reihenfolge in eine
    /// standardnahe Freelancer-Struktur um.
    static bool normalizeSectionOrder(const flatlas::domain::SystemDocument *doc);

    /// Ordnet die Section-Bloecke einer System-INI direkt in der Datei um,
    /// ohne vorhandene Zeilen innerhalb unveraenderter Sections neu zu serialisieren.
    static bool normalizeSectionOrderInFile(const QString &filePath,
                                            bool *changed = nullptr,
                                            QString *errorMessage = nullptr);

    static flatlas::infrastructure::IniSection serializeObjectSection(const flatlas::domain::SolarObject &obj);
    static flatlas::infrastructure::IniSection serializeZoneSection(const flatlas::domain::ZoneItem &zone);
    static void applyObjectSection(flatlas::domain::SolarObject &obj,
                                   const flatlas::infrastructure::IniSection &sec);
    static void applyZoneSection(flatlas::domain::ZoneItem &zone,
                                 const flatlas::infrastructure::IniSection &sec);

private:
    static QHash<const flatlas::domain::SystemDocument *,
                 flatlas::infrastructure::IniDocument> s_extras;
    static QHash<const flatlas::domain::SystemDocument *,
                 flatlas::infrastructure::IniSection> s_systemInfoSections;
    static QHash<const flatlas::domain::SystemDocument *,
                 flatlas::infrastructure::IniDocument> s_layoutSections;
    static QHash<const flatlas::domain::SystemDocument *, bool> s_nonStandardOrder;
};

} // namespace flatlas::editors
