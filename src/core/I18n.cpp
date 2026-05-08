#include "I18n.h"

#include <QHash>
#include <QLocale>
#include <utility>

namespace flatlas::core {
namespace {

class RuntimeTranslator final : public QTranslator
{
public:
    explicit RuntimeTranslator(QHash<QString, QString> translations, QObject *parent = nullptr)
        : QTranslator(parent)
        , m_translations(std::move(translations))
    {
    }

    QString translate(const char *, const char *sourceText, const char *, int) const override
    {
        if (!sourceText)
            return {};
        return m_translations.value(QString::fromUtf8(sourceText));
    }

private:
    QHash<QString, QString> m_translations;
};

void addPair(QHash<QString, QString> &translations,
             const QString &english,
             const QString &german,
             const QString &language)
{
    if (language == QStringLiteral("de"))
        translations.insert(english, german);
    else
        translations.insert(german, english);
}

QHash<QString, QString> builtInTranslations(const QString &language)
{
    QHash<QString, QString> t;

    addPair(t, QStringLiteral("Starting..."), QStringLiteral("Starte..."), language);
    addPair(t, QStringLiteral("Ready"), QStringLiteral("Bereit"), language);
    addPair(t, QStringLiteral("&File"), QStringLiteral("&Datei"), language);
    addPair(t, QStringLiteral("&Save"), QStringLiteral("&Speichern"), language);
    addPair(t, QStringLiteral("&Settings..."), QStringLiteral("&Einstellungen..."), language);
    addPair(t, QStringLiteral("E&xit"), QStringLiteral("B&eenden"), language);
    addPair(t, QStringLiteral("&Edit"), QStringLiteral("&Bearbeiten"), language);
    addPair(t, QStringLiteral("&Undo"), QStringLiteral("&Rückgängig"), language);
    addPair(t, QStringLiteral("&Redo"), QStringLiteral("&Wiederholen"), language);
    addPair(t, QStringLiteral("&View"), QStringLiteral("&Ansicht"), language);
    addPair(t, QStringLiteral("System &Names"), QStringLiteral("System&namen"), language);
    addPair(t, QStringLiteral("&Grid"), QStringLiteral("&Raster"), language);
    addPair(t, QStringLiteral("&Tools"), QStringLiteral("&Werkzeuge"), language);
    addPair(t, QStringLiteral("Open &File Editor..."), QStringLiteral("&Datei-Editor öffnen..."), language);
    addPair(t, QStringLiteral("&Trade Routes"), QStringLiteral("&Handelsrouten"), language);
    addPair(t, QStringLiteral("&IDS Editor"), QStringLiteral("&IDS-Editor"), language);
    addPair(t, QStringLiteral("&Mod Manager"), QStringLiteral("&Mod-Manager"), language);
    addPair(t, QStringLiteral("&NPC Editor"), QStringLiteral("&NPC-Editor"), language);
    addPair(t, QStringLiteral("&News Editor"), QStringLiteral("&News Editor"), language);
    addPair(t, QStringLiteral("&3D Model Viewer"), QStringLiteral("&3D-Modellviewer"), language);
    addPair(t, QStringLiteral("&Shortest Path..."), QStringLiteral("&Kürzester Pfad..."), language);
    addPair(t, QStringLiteral("&Launch Freelancer..."), QStringLiteral("Freelancer &starten..."), language);
    addPair(t, QStringLiteral("&Settings"), QStringLiteral("&Einstellungen"), language);
    addPair(t, QStringLiteral("&Theme"), QStringLiteral("&Design"), language);
    addPair(t, QStringLiteral("&Language"), QStringLiteral("&Sprache"), language);
    addPair(t, QStringLiteral("&Help"), QStringLiteral("&Hilfe"), language);
    addPair(t, QStringLiteral("&Help Contents"), QStringLiteral("&Hilfeinhalt"), language);
    addPair(t, QStringLiteral("Keyboard &Shortcuts"), QStringLiteral("Tastatur&kürzel"), language);
    addPair(t, QStringLiteral("Check for &Updates..."), QStringLiteral("Nach &Updates suchen..."), language);
    addPair(t, QStringLiteral("&About FL Atlas..."), QStringLiteral("&Über FL Atlas..."), language);

    addPair(t, QStringLiteral("Settings"), QStringLiteral("Einstellungen"), language);
    addPair(t, QStringLiteral("Appearance"), QStringLiteral("Darstellung"), language);
    addPair(t, QStringLiteral("Theme:"), QStringLiteral("Design:"), language);
    addPair(t, QStringLiteral("Language:"), QStringLiteral("Sprache:"), language);
    addPair(t, QStringLiteral("Freelancer"), QStringLiteral("Freelancer"), language);
    addPair(t, QStringLiteral("Path to Freelancer installation..."), QStringLiteral("Pfad zur Freelancer-Installation..."), language);
    addPair(t, QStringLiteral("Browse..."), QStringLiteral("Durchsuchen..."), language);
    addPair(t, QStringLiteral("Game Path:"), QStringLiteral("Spielpfad:"), language);
    addPair(t, QStringLiteral("Select Freelancer Installation"), QStringLiteral("Freelancer-Installation auswählen"), language);

    addPair(t, QStringLiteral("Welcome"), QStringLiteral("Willkommen"), language);
    addPair(t, QStringLiteral("Welcome to FL Atlas"), QStringLiteral("Willkommen bei FL Atlas"), language);
    addPair(t, QStringLiteral("Quick Introduction"), QStringLiteral("Kurze Einführung"), language);
    addPair(t, QStringLiteral("How to start"), QStringLiteral("So startest du"), language);
    addPair(t, QStringLiteral("Quick Start"), QStringLiteral("Schnellstart"), language);
    addPair(t, QStringLiteral("Welcome back. Use this page as a quick start and entry point to Mod Manager."),
            QStringLiteral("Willkommen zurück. Diese Seite dient als Schnellstart und Einstieg in den Mod-Manager."), language);
    addPair(t, QStringLiteral("FL Atlas is a visual editor for Freelancer INI files. You can edit systems, objects, zones, trade routes and IDS names in one place."),
            QStringLiteral("FL Atlas ist ein visueller Editor für Freelancer-INI-Dateien. Systeme, Objekte, Zonen, Handelsrouten und IDS-Namen können an einem Ort bearbeitet werden."), language);
    addPair(t, QStringLiteral("<ol style='margin:0; padding-left:18px;'><li>Open the <b>Mod Manager</b>.</li><li>Create a mod or select an existing mod folder.</li><li>Set that mod as the <b>editing context</b>.</li><li>Then switch to Universe, System, Trade Routes, or Name Editor.</li></ol>"),
            QStringLiteral("<ol style='margin:0; padding-left:18px;'><li>Öffne den <b>Mod-Manager</b>.</li><li>Erstelle einen Mod oder wähle einen bestehenden Mod-Ordner.</li><li>Setze diesen Mod als <b>Bearbeitungskontext</b>.</li><li>Wechsle danach zu Universe, System, Handelsrouten oder Namens-Editor.</li></ol>"), language);
    addPair(t, QStringLiteral("Automatically check for updates at startup"),
            QStringLiteral("Beim Start automatisch nach Updates suchen"), language);
    addPair(t, QStringLiteral("Don't show this page on next start"),
            QStringLiteral("Diese Seite beim nächsten Start nicht anzeigen"), language);
    addPair(t, QStringLiteral("Update Check"), QStringLiteral("Updateprüfung"), language);
    addPair(t, QStringLiteral("Welcome Screen"), QStringLiteral("Willkommensseite"), language);
    addPair(t, QStringLiteral("IDS toolchain detected. IDS names and IDS info can be created and edited."),
            QStringLiteral("IDS-Werkzeuge erkannt. IDS-Namen und IDS-Infos können erstellt und bearbeitet werden."), language);
    addPair(t, QStringLiteral("Open Wiki"), QStringLiteral("Wiki öffnen"), language);
    addPair(t, QStringLiteral("Install IDS Tools"), QStringLiteral("IDS-Werkzeuge installieren"), language);
    addPair(t, QStringLiteral("Open Mod Manager"), QStringLiteral("Mod-Manager öffnen"), language);

    addPair(t, QStringLiteral("Search objects / zones"), QStringLiteral("Objekte / Zonen suchen"), language);
    addPair(t, QStringLiteral("Display Filters"), QStringLiteral("Anzeigefilter"), language);
    addPair(t, QStringLiteral("Nickname"), QStringLiteral("Nickname"), language);
    addPair(t, QStringLiteral("Type"), QStringLiteral("Typ"), language);
    addPair(t, QStringLiteral("Objects"), QStringLiteral("Objekte"), language);
    addPair(t, QStringLiteral("Zones"), QStringLiteral("Zonen"), language);
    addPair(t, QStringLiteral("Properties"), QStringLiteral("Eigenschaften"), language);
    addPair(t, QStringLiteral("Quick Access"), QStringLiteral("Schnellzugriff"), language);
    addPair(t, QStringLiteral("System Browser"), QStringLiteral("Systembrowser"), language);
    addPair(t, QStringLiteral("Refresh Systems"), QStringLiteral("Systeme aktualisieren"), language);
    addPair(t, QStringLiteral("Systems:"), QStringLiteral("Systeme:"), language);
    addPair(t, QStringLiteral("%1 systems"), QStringLiteral("%1 Systeme"), language);
    addPair(t, QStringLiteral("Filter systems..."), QStringLiteral("Systeme filtern..."), language);
    addPair(t, QStringLiteral("Set editing context to load systems"), QStringLiteral("Bearbeitungskontext setzen, um Systeme zu laden"), language);

    addPair(t, QStringLiteral("Update Available"), QStringLiteral("Update verfügbar"), language);
    addPair(t, QStringLiteral("Download Failed"), QStringLiteral("Download fehlgeschlagen"), language);
    addPair(t, QStringLiteral("Update Failed"), QStringLiteral("Update fehlgeschlagen"), language);
    addPair(t, QStringLiteral("Update Ready"), QStringLiteral("Update bereit"), language);
    addPair(t, QStringLiteral("Update Check"), QStringLiteral("Updateprüfung"), language);
    addPair(t, QStringLiteral("No Update"), QStringLiteral("Kein Update"), language);
    addPair(t, QStringLiteral("Checking for updates..."), QStringLiteral("Suche nach Updates..."), language);
    addPair(t, QStringLiteral("Downloading update..."), QStringLiteral("Update wird heruntergeladen..."), language);
    addPair(t, QStringLiteral("Cancel"), QStringLiteral("Abbrechen"), language);
    addPair(t, QStringLiteral("Version %1 is available (current: %2).\n\n%3\n\nDownload and install?"),
            QStringLiteral("Version %1 ist verfügbar (aktuell: %2).\n\n%3\n\nHerunterladen und installieren?"), language);
    addPair(t, QStringLiteral("Version %1 is available (current: %2).\n\n%3"),
            QStringLiteral("Version %1 ist verfügbar (aktuell: %2).\n\n%3"), language);
    addPair(t, QStringLiteral("Update prepared. Restart now to apply?"),
            QStringLiteral("Update vorbereitet. Jetzt neu starten, um es anzuwenden?"), language);
    addPair(t, QStringLiteral("You are running the latest version (%1)."),
            QStringLiteral("Du verwendest die neueste Version (%1)."), language);
    addPair(t, QStringLiteral("Update check failed: %1"), QStringLiteral("Updateprüfung fehlgeschlagen: %1"), language);

    addPair(t, QStringLiteral("Currently Editing: -"), QStringLiteral("Aktuell bearbeitet: -"), language);
    addPair(t, QStringLiteral("Currently Editing: %1"), QStringLiteral("Aktuell bearbeitet: %1"), language);
    addPair(t, QStringLiteral("Launch FL"), QStringLiteral("FL starten"), language);
    addPair(t, QStringLiteral("FLAtlas Settings"), QStringLiteral("FLAtlas-Einstellungen"), language);
    addPair(t, QStringLiteral("Activity"), QStringLiteral("Aktivität"), language);
    addPair(t, QStringLiteral("Language set to '%1'. Restart FLAtlas to fully apply."),
            QStringLiteral("Sprache auf '%1' gesetzt. Starte FLAtlas neu, um alles vollständig anzuwenden."), language);
    addPair(t, QStringLiteral("Language set to '%1'. Open tabs may need to be reopened."),
            QStringLiteral("Sprache auf '%1' gesetzt. Geöffnete Tabs müssen ggf. neu geöffnet werden."), language);
    addPair(t, QStringLiteral("Launch Freelancer"), QStringLiteral("Freelancer starten"), language);
    addPair(t, QStringLiteral("No editing context set. Select an installation first."),
            QStringLiteral("Kein Bearbeitungskontext gesetzt. Wähle zuerst eine Installation."), language);
    addPair(t, QStringLiteral("Select Freelancer.exe"), QStringLiteral("Freelancer.exe auswählen"), language);
    addPair(t, QStringLiteral("Executable (*.exe)"), QStringLiteral("Ausführbare Datei (*.exe)"), language);
    addPair(t, QStringLiteral("Freelancer launched."), QStringLiteral("Freelancer gestartet."), language);
    addPair(t, QStringLiteral("Failed to launch Freelancer."), QStringLiteral("Freelancer konnte nicht gestartet werden."), language);

    addPair(t, QStringLiteral("Unsaved Changes"), QStringLiteral("Ungespeicherte Änderungen"), language);
    addPair(t, QStringLiteral("Tab \"%1\" has unsaved changes."),
            QStringLiteral("Im Tab \"%1\" gibt es ungespeicherte Änderungen."), language);
    addPair(t, QStringLiteral("Do you want to save the changes before closing?"),
            QStringLiteral("Möchtest du die Änderungen speichern, bevor geschlossen wird?"), language);
    addPair(t, QStringLiteral("Save"), QStringLiteral("Speichern"), language);
    addPair(t, QStringLiteral("Discard"), QStringLiteral("Verwerfen"), language);
    addPair(t, QStringLiteral("Cancel"), QStringLiteral("Abbrechen"), language);
    addPair(t, QStringLiteral("Untitled"), QStringLiteral("Unbenannt"), language);
    addPair(t, QStringLiteral("New Faction"), QStringLiteral("Neue Fraktion"), language);
    addPair(t, QStringLiteral("Ingame name"), QStringLiteral("Ingame-Name"), language);
    addPair(t, QStringLiteral("Short name"), QStringLiteral("Kurzname"), language);
    addPair(t, QStringLiteral("Infocard text"), QStringLiteral("Infocard-Text"), language);
    addPair(t, QStringLiteral("Faction template"), QStringLiteral("Fraktionsvorlage"), language);
    addPair(t, QStringLiteral("Legality"), QStringLiteral("Legalität"), language);
    addPair(t, QStringLiteral("Uses ingame name if empty"), QStringLiteral("Nutzt den Ingame-Namen, wenn leer"), language);
    addPair(t, QStringLiteral("Minimal faction defaults"), QStringLiteral("Minimale Fraktions-Defaults"), language);
    addPair(t, QStringLiteral("Could not create faction:\n%1"), QStringLiteral("Fraktion konnte nicht erstellt werden:\n%1"), language);

    addPair(t, QStringLiteral("Error"), QStringLiteral("Fehler"), language);
    addPair(t, QStringLiteral("Could not save file."), QStringLiteral("Datei konnte nicht gespeichert werden."), language);
    addPair(t, QStringLiteral("Could not open file:\n%1"), QStringLiteral("Datei konnte nicht geöffnet werden:\n%1"), language);
    addPair(t, QStringLiteral("Saved"), QStringLiteral("Gespeichert"), language);
    addPair(t, QStringLiteral("Saved: %1"), QStringLiteral("Gespeichert: %1"), language);
    addPair(t, QStringLiteral("Opened: %1"), QStringLiteral("Geöffnet: %1"), language);
    addPair(t, QStringLiteral("Save System INI"), QStringLiteral("System-INI speichern"), language);
    addPair(t, QStringLiteral("Save INI File"), QStringLiteral("INI-Datei speichern"), language);
    addPair(t, QStringLiteral("INI Files (*.ini);;All Files (*)"), QStringLiteral("INI-Dateien (*.ini);;Alle Dateien (*)"), language);
    addPair(t, QStringLiteral("File Editor"), QStringLiteral("Datei-Editor"), language);
    addPair(t, QStringLiteral("File Editor workspace opened"), QStringLiteral("Datei-Editor-Arbeitsbereich geöffnet"), language);
    addPair(t, QStringLiteral("Universe.ini not found in editing context"), QStringLiteral("Universe.ini im Bearbeitungskontext nicht gefunden"), language);
    addPair(t, QStringLiteral("Could not load Universe from editing context"), QStringLiteral("Universe konnte nicht aus dem Bearbeitungskontext geladen werden"), language);
    addPair(t, QStringLiteral("Universe loaded from editing context"), QStringLiteral("Universe aus Bearbeitungskontext geladen"), language);
    addPair(t, QStringLiteral("Could not reload Universe from editing context"), QStringLiteral("Universe konnte nicht aus dem Bearbeitungskontext neu geladen werden"), language);
    addPair(t, QStringLiteral("Universe reloaded from editing context"), QStringLiteral("Universe aus Bearbeitungskontext neu geladen"), language);
    addPair(t, QStringLiteral("Editing context switched to %1"), QStringLiteral("Bearbeitungskontext gewechselt zu %1"), language);
    addPair(t, QStringLiteral("Editing context cleared"), QStringLiteral("Bearbeitungskontext gelöscht"), language);
    addPair(t, QStringLiteral("Opening system: %1"), QStringLiteral("Öffne System: %1"), language);
    addPair(t, QStringLiteral("Opened system: %1"), QStringLiteral("System geöffnet: %1"), language);
    addPair(t, QStringLiteral("Could not load system file"), QStringLiteral("Systemdatei konnte nicht geladen werden"), language);
    addPair(t, QStringLiteral("Could not load system file:\n%1"), QStringLiteral("Systemdatei konnte nicht geladen werden:\n%1"), language);
    addPair(t, QStringLiteral("Could not resolve system file for '%1':\n%2"),
            QStringLiteral("Systemdatei für '%1' konnte nicht aufgelöst werden:\n%2"), language);
    addPair(t, QStringLiteral("3D: %1"), QStringLiteral("3D: %1"), language);
    addPair(t, QStringLiteral("3D system view opened: %1"), QStringLiteral("3D-Systemansicht geöffnet: %1"), language);
    addPair(t, QStringLiteral("3D Model Viewer opened"), QStringLiteral("3D-Modellviewer geöffnet"), language);
    addPair(t, QStringLiteral("3D model loaded"), QStringLiteral("3D-Modell geladen"), language);

    addPair(t, QStringLiteral("Trade Routes"), QStringLiteral("Handelsrouten"), language);
    addPair(t, QStringLiteral("Trade Routes opened"), QStringLiteral("Handelsrouten geöffnet"), language);
    addPair(t, QStringLiteral("IDS Editor"), QStringLiteral("IDS-Editor"), language);
    addPair(t, QStringLiteral("IDS Editor opened"), QStringLiteral("IDS-Editor geöffnet"), language);
    addPair(t, QStringLiteral("all"), QStringLiteral("alle"), language);
    addPair(t, QStringLiteral("NPC Editor"), QStringLiteral("NPC-Editor"), language);
    addPair(t, QStringLiteral("NPC Editor opened"), QStringLiteral("NPC-Editor geöffnet"), language);
    addPair(t, QStringLiteral("%1 (%2 NPCs)"), QStringLiteral("%1 (%2 NPCs)"), language);
    addPair(t, QStringLiteral("News Editor"), QStringLiteral("News Editor"), language);
    addPair(t, QStringLiteral("News Editor opened"), QStringLiteral("News Editor geöffnet"), language);
    addPair(t, QStringLiteral("Shortest Path"), QStringLiteral("Kürzester Pfad"), language);
    addPair(t, QStringLiteral("Please open a Universe file first."), QStringLiteral("Bitte zuerst eine Universe-Datei öffnen."), language);

    addPair(t, QStringLiteral("Solar"), QStringLiteral("Solar"), language);
    addPair(t, QStringLiteral("Ships"), QStringLiteral("Schiffe"), language);
    addPair(t, QStringLiteral("Equipment"), QStringLiteral("Ausrüstung"), language);
    addPair(t, QStringLiteral("Models"), QStringLiteral("Modelle"), language);
    addPair(t, QStringLiteral("Rooms"), QStringLiteral("Räume"), language);
    addPair(t, QStringLiteral("Docking Ring"), QStringLiteral("Docking Ring"), language);
    addPair(t, QStringLiteral("Create Docking Ring"), QStringLiteral("Docking Ring erstellen"), language);
    addPair(t, QStringLiteral("Archetype:"), QStringLiteral("Archetype:"), language);
    addPair(t, QStringLiteral("Loadout:"), QStringLiteral("Loadout:"), language);
    addPair(t, QStringLiteral("Reputation:"), QStringLiteral("Reputation:"), language);
    addPair(t, QStringLiteral("Voice:"), QStringLiteral("Voice:"), language);
    addPair(t, QStringLiteral("Space Costume:"), QStringLiteral("Space Costume:"), language);
    addPair(t, QStringLiteral("Pilot:"), QStringLiteral("Pilot:"), language);
    addPair(t, QStringLiteral("Difficulty Level:"), QStringLiteral("Schwierigkeitsgrad:"), language);
    addPair(t, QStringLiteral("ids_info:"), QStringLiteral("ids_info:"), language);
    addPair(t, QStringLiteral("Distance to Planet Core:"), QStringLiteral("Abstand zum Planetenkern:"), language);
    addPair(t, QStringLiteral("Create docking_fixture"), QStringLiteral("docking_fixture erstellen"), language);
    addPair(t, QStringLiteral("Creates or keeps a docking_fixture above the docking ring with ids_name=261166 and ids_info=66489."),
            QStringLiteral("Erstellt oder behält ein docking_fixture über dem Docking Ring mit ids_name=261166 und ids_info=66489."), language);
    addPair(t, QStringLiteral("Base Nickname:"), QStringLiteral("Base-Nickname:"), language);
    addPair(t, QStringLiteral("strid_name:"), QStringLiteral("strid_name:"), language);
    addPair(t, QStringLiteral("General"), QStringLiteral("Allgemein"), language);
    addPair(t, QStringLiteral("Room Editor"), QStringLiteral("Room-Editor"), language);
    addPair(t, QStringLiteral("Equipment & Ships"), QStringLiteral("Ausrüstung & Schiffe"), language);
    addPair(t, QStringLiteral("Commodities"), QStringLiteral("Commodities"), language);
    addPair(t, QStringLiteral("Base Equipment"), QStringLiteral("Base-Ausrüstung"), language);
    addPair(t, QStringLiteral("Available"), QStringLiteral("Verfügbar"), language);
    addPair(t, QStringLiteral("Search equipment..."), QStringLiteral("Equipment suchen..."), language);
    addPair(t, QStringLiteral("Search commodities..."), QStringLiteral("Commodities suchen..."), language);
    addPair(t, QStringLiteral("On This Base"), QStringLiteral("Auf dieser Base"), language);
    addPair(t, QStringLiteral("Market Settings"), QStringLiteral("Market-Einstellungen"), language);
    addPair(t, QStringLiteral("Reputation:"), QStringLiteral("Reputation:"), language);
    addPair(t, QStringLiteral("Min Stock:"), QStringLiteral("Min-Bestand:"), language);
    addPair(t, QStringLiteral("Max Stock:"), QStringLiteral("Max-Bestand:"), language);
    addPair(t, QStringLiteral("Trade Mode:"), QStringLiteral("Handelsmodus:"), language);
    addPair(t, QStringLiteral("Base sells to player"), QStringLiteral("Base verkauft an Spieler"), language);
    addPair(t, QStringLiteral("Base buys from player"), QStringLiteral("Base kauft vom Spieler"), language);
    addPair(t, QStringLiteral("MarketGood mode: sell means the player can buy this commodity here; buy means the player can sell it here."),
            QStringLiteral("MarketGood-Modus: Verkaufen bedeutet, dass der Spieler die Ware hier kaufen kann; Kaufen bedeutet, dass der Spieler sie hier verkaufen kann."), language);
    addPair(t, QStringLiteral("Price Factor:"), QStringLiteral("Preisfaktor:"), language);
    addPair(t, QStringLiteral("Base Price:"), QStringLiteral("Basispreis:"), language);
    addPair(t, QStringLiteral("Calculated Price:"), QStringLiteral("Berechneter Preis:"), language);
    addPair(t, QStringLiteral("Weapons"), QStringLiteral("Waffen"), language);
    addPair(t, QStringLiteral("Shields / Thrusters"), QStringLiteral("Schilde / Thruster"), language);
    addPair(t, QStringLiteral("Misc"), QStringLiteral("Sonstiges"), language);
    addPair(t, QStringLiteral("Engines"), QStringLiteral("Engines"), language);
    addPair(t, QStringLiteral("Level:"), QStringLiteral("Level:"), language);
    addPair(t, QStringLiteral("Remove Selected"), QStringLiteral("Auswahl entfernen"), language);
    addPair(t, QStringLiteral("Ships (maximum 3)"), QStringLiteral("Schiffe (maximal 3)"), language);
    addPair(t, QStringLiteral("Ship Slot %1:"), QStringLiteral("Schiff-Slot %1:"), language);
    addPair(t, QStringLiteral("Freelancer supports a maximum of 3 ships per base."),
            QStringLiteral("Freelancer unterstützt maximal 3 Schiffe pro Base."), language);
    addPair(t, QStringLiteral("Freelancer supports at most 3 ships per base."),
            QStringLiteral("Freelancer unterstützt maximal 3 Schiffe pro Base."), language);
    addPair(t, QStringLiteral("Empty"), QStringLiteral("Leer"), language);
    addPair(t, QStringLiteral("Ship packages are shown as nickname - ingamename. Empty ship slots are allowed."),
            QStringLiteral("Schiffspakete werden als nickname - ingamename angezeigt. Leere Schiff-Slots sind erlaubt."), language);
    addPair(t, QStringLiteral("Duplicate ship assignments were removed. Freelancer supports each ship package only once per base."),
            QStringLiteral("Doppelte Schiff-Zuweisungen wurden entfernt. Freelancer unterstützt jedes Schiffspaket nur einmal pro Base."), language);
    addPair(t, QStringLiteral("Ship packages are displayed as nickname - ingamename. Freelancer supports at most 3 ships per base."),
            QStringLiteral("Schiffspakete werden als nickname - ingamename angezeigt. Freelancer unterstützt maximal 3 Schiffe pro Base."), language);
    addPair(t, QStringLiteral("Duplicate ship assignments were removed. Freelancer supports at most 3 ships per base."),
            QStringLiteral("Doppelte Schiff-Zuweisungen wurden entfernt. Freelancer unterstützt maximal 3 Schiffe pro Base."), language);
    addPair(t, QStringLiteral("Could not resolve Freelancer DATA path for equipment and ship markets."),
            QStringLiteral("Der Freelancer-DATA-Pfad für Ausrüstungs- und Schiffsmärkte konnte nicht ermittelt werden."), language);
    addPair(t, QStringLiteral("Start Room"), QStringLiteral("Start-Room"), language);
    addPair(t, QStringLiteral("Price Variance"), QStringLiteral("Preisabweichung"), language);
    addPair(t, QStringLiteral("Room Template"), QStringLiteral("Room-Vorlage"), language);
    addPair(t, QStringLiteral("Copy Rooms From:"), QStringLiteral("Rooms kopieren von:"), language);
    addPair(t, QStringLiteral("Type:"), QStringLiteral("Typ:"), language);
    addPair(t, QStringLiteral("Room nickname (auto if empty)"), QStringLiteral("Room-Nickname (automatisch, falls leer)"), language);
    addPair(t, QStringLiteral("Add"), QStringLiteral("Hinzufügen"), language);
    addPair(t, QStringLiteral("Remove"), QStringLiteral("Entfernen"), language);

    addPair(t, QStringLiteral("Delete System"), QStringLiteral("System löschen"), language);
    addPair(t, QStringLiteral("Delete System: %1"), QStringLiteral("System löschen: %1"), language);
    addPair(t, QStringLiteral("Precheck"), QStringLiteral("Vorprüfung"), language);
    addPair(t, QStringLiteral("Base '%1' is still used by object '%2' and therefore cannot be deleted automatically."),
            QStringLiteral("Base '%1' wird noch von Objekt '%2' verwendet und kann deshalb nicht automatisch gelöscht werden."), language);
    addPair(t, QStringLiteral("No host object to delete was found for base '%1'."),
            QStringLiteral("Für Base '%1' wurde kein zu löschendes Host-Objekt gefunden."), language);
    addPair(t, QStringLiteral("Base '%1' could not be loaded."),
            QStringLiteral("Base '%1' konnte nicht geladen werden."), language);
    addPair(t, QStringLiteral("Remove [Base] '%1'"), QStringLiteral("[Base] '%1' entfernen"), language);
    addPair(t, QStringLiteral("Remove [MBase] '%1'"), QStringLiteral("[MBase] '%1' entfernen"), language);
    addPair(t, QStringLiteral("Remove [BaseGood] '%1'"), QStringLiteral("[BaseGood] '%1' entfernen"), language);
    addPair(t, QStringLiteral("Room file for base '%1'"), QStringLiteral("Room-Datei von Base '%1'"), language);
    addPair(t, QStringLiteral("Base INI '%1'"), QStringLiteral("Base-INI '%1'"), language);
    addPair(t, QStringLiteral("Multiple docking_fixture objects found for docking ring '%1'; fixtures were not added automatically."),
            QStringLiteral("Mehrere docking_fixture-Objekte für Docking Ring '%1' gefunden; Fixtures wurden nicht automatisch hinzugefügt."), language);
    addPair(t, QStringLiteral("Objects:\n%1\n\n"), QStringLiteral("Objekte:\n%1\n\n"), language);
    addPair(t, QStringLiteral("Zones:\n%1\n\n"), QStringLiteral("Zonen:\n%1\n\n"), language);
    addPair(t, QStringLiteral("Files will be updated:\n%1\n\n"), QStringLiteral("Dateien werden angepasst:\n%1\n\n"), language);
    addPair(t, QStringLiteral("Files will be deleted:\n%1"), QStringLiteral("Dateien werden gelöscht:\n%1"), language);
    addPair(t, QStringLiteral("  - none"), QStringLiteral("  - keine"), language);
    addPair(t, QStringLiteral("\n\nNotes:\n  - %1"), QStringLiteral("\n\nHinweise:\n  - %1"), language);
    addPair(t, QStringLiteral("Delete planet with linked data"), QStringLiteral("Planet mit verknüpften Daten löschen"), language);
    addPair(t, QStringLiteral("Linked objects, zones, base entries, and files belonging to the planet will be deleted. Review the details and confirm the operation."),
            QStringLiteral("Zum Planet gehörende Objekte, Zonen, Base-Einträge und Dateien werden gelöscht. Bitte prüfe die Details und bestätige den Vorgang."), language);
    addPair(t, QStringLiteral("Delete linked data"), QStringLiteral("Verknüpfte Daten löschen"), language);
    addPair(t, QStringLiteral("Linked objects, zones, base entries, and files belonging to the selection will be deleted. Review the details and confirm the operation."),
            QStringLiteral("Zur Auswahl gehörende Objekte, Zonen, Base-Einträge und Dateien werden gelöscht. Bitte prüfe die Details und bestätige den Vorgang."), language);
    addPair(t, QStringLiteral("Delete Planet"), QStringLiteral("Planet löschen"), language);
    addPair(t, QStringLiteral("The linked planet data could not be determined completely."),
            QStringLiteral("Die verknüpften Planet-Daten konnten nicht vollständig ermittelt werden."), language);
    addPair(t, QStringLiteral("The linked data could not be determined completely."),
            QStringLiteral("Die verknüpften Daten konnten nicht vollständig ermittelt werden."), language);
    addPair(t, QStringLiteral("A linked file has no valid path and therefore could not be deleted."),
            QStringLiteral("Eine verknüpfte Datei hat keinen gültigen Pfad und konnte deshalb nicht gelöscht werden."), language);
    addPair(t, QStringLiteral("The linked file could not be deleted:\n%1"),
            QStringLiteral("Die verknüpfte Datei konnte nicht gelöscht werden:\n%1"), language);
    addPair(t, QStringLiteral("Target"), QStringLiteral("Ziel"), language);
    addPair(t, QStringLiteral("Warnings"), QStringLiteral("Warnungen"), language);
    addPair(t, QStringLiteral("Blockers"), QStringLiteral("Blocker"), language);
    addPair(t, QStringLiteral("Result"), QStringLiteral("Ergebnis"), language);
    addPair(t, QStringLiteral("0 - normal"), QStringLiteral("0 - normal"), language);
    addPair(t, QStringLiteral("1 - always visible"), QStringLiteral("1 - immer sichtbar"), language);
    addPair(t, QStringLiteral("128 - hidden system"), QStringLiteral("128 - verstecktes System"), language);

    addPair(t, QStringLiteral("About FL Atlas"), QStringLiteral("Über FL Atlas"), language);
    addPair(t, QStringLiteral("<h2>FL Atlas V2</h2><p><b>Version:</b> v%1</p><p><b>Author:</b> Steven</p><p><b>License:</b> MIT License</p><hr><p>A visual editor for Freelancer system files (INI). FL Atlas is also compatible with FLMM mods so they can be used here.</p><p>Systems are shown as interactive 2-D/3-D maps. Objects, zones, bases, docking rings, tradelanes, and connections can be created, edited, and moved.</p><p>Thanks to IGx89 for Freelancer Mod Manager (FLMM) and his work for the modding community.</p><hr><p><b>Technology:</b> C++ · Qt 6 · Qt3D</p><p><b>Game:</b> Freelancer (2003, Digital Anvil / Microsoft)</p><p>&copy; 2024–2025 flathack</p>"),
            QStringLiteral("<h2>FL Atlas V2</h2><p><b>Version:</b> v%1</p><p><b>Autor:</b> Steven</p><p><b>Lizenz:</b> MIT License</p><hr><p>Ein visueller Editor für Freelancer-Systemdateien (INI). FL Atlas ist zusätzlich kompatibel mit FLMM-Mods, damit diese auch hier genutzt werden können.</p><p>Zeigt Systeme als interaktive 2-D/3-D-Karte an. Objekte, Zonen, Bases, Docking Rings, Tradelanes und Verbindungen können erstellt, bearbeitet und verschoben werden.</p><p>Vielen Dank an IGx89 für Freelancer Mod Manager (FLMM) und seine Arbeit für die Modding-Community.</p><hr><p><b>Technologie:</b> C++ · Qt 6 · Qt3D</p><p><b>Spiel:</b> Freelancer (2003, Digital Anvil / Microsoft)</p><p>&copy; 2024–2025 flathack</p>"), language);

    return t;
}

QString normalizeLanguage(QString langCode)
{
    langCode = langCode.trimmed().toLower();
    if (langCode.startsWith(QStringLiteral("de")))
        return QStringLiteral("de");
    return QStringLiteral("en");
}

} // namespace

I18n &I18n::instance()
{
    static I18n i18n;
    return i18n;
}

QStringList I18n::availableLanguages()
{
    return {QStringLiteral("de"), QStringLiteral("en")};
}

void I18n::removeTranslators()
{
    if (m_appTranslator) {
        QCoreApplication::removeTranslator(m_appTranslator);
        delete m_appTranslator;
        m_appTranslator = nullptr;
    }
    if (m_qtTranslator) {
        QCoreApplication::removeTranslator(m_qtTranslator);
        delete m_qtTranslator;
        m_qtTranslator = nullptr;
    }
}

void I18n::setLanguage(const QString &langCode)
{
    const QString normalized = normalizeLanguage(langCode);
    if (m_language == normalized && m_appTranslator)
        return;

    removeTranslators();

    const auto translations = builtInTranslations(normalized);
    if (!translations.isEmpty()) {
        m_appTranslator = new RuntimeTranslator(translations, this);
        QCoreApplication::installTranslator(m_appTranslator);
    }

    if (normalized != QStringLiteral("en")) {
        m_qtTranslator = new QTranslator(this);
        const QString qtResource = QStringLiteral(":/translations/qt_%1.qm").arg(normalized);
        const QString qtSystem = QStringLiteral("qt_%1").arg(normalized);
        if (m_qtTranslator->load(qtResource)
            || m_qtTranslator->load(QLocale(normalized), qtSystem, QStringLiteral("_"))) {
            QCoreApplication::installTranslator(m_qtTranslator);
        } else {
            delete m_qtTranslator;
            m_qtTranslator = nullptr;
        }
    }

    m_language = normalized;
    emit languageChanged();
}

QString I18n::currentLanguage() const
{
    return m_language.isEmpty() ? QStringLiteral("en") : m_language;
}

} // namespace flatlas::core
