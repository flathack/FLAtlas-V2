# C. Vollständiger Migrationsplan – FLAtlas Python → C++/Qt 6

## Phasenübersicht

| Phase | Name | Priorität | Geschätzte Komplexität | Abhängigkeiten |
|-------|------|-----------|----------------------|----------------|
| **0** | Toolchain & Grundgerüst | 🔴 Kritisch | Niedrig | Keine |
| **1** | Core-Infrastruktur | 🔴 Kritisch | Mittel | Phase 0 |
| **2** | Parser & Datenmodelle | 🔴 Kritisch | Hoch | Phase 1 |
| **3** | Hauptfenster & Navigation | 🔴 Kritisch | Mittel | Phase 1 |
| **4** | 2D-Systemkarte | 🟡 Hoch | Hoch | Phase 2, 3 |
| **5** | System-Editor (Basis) | 🟡 Hoch | Hoch | Phase 2, 3, 4 |
| **6** | INI-Editor | 🟡 Hoch | Mittel | Phase 2, 3 |
| **7** | 3D-View (Basis) | 🟡 Hoch | Sehr Hoch | Phase 2, 3 |
| **8** | CMP/3DB-Loader & Texturen | 🟡 Hoch | Sehr Hoch | Phase 7 |
| **9** | Universe-Editor | 🟡 Mittel | Mittel | Phase 4, 5 |
| **10** | Base-Editor | 🟡 Mittel | Mittel | Phase 5 |
| **11** | Trade-Route-Analyse | 🟡 Mittel | Mittel | Phase 2, 3 |
| **12** | IDS/String-Resourcen | 🟡 Mittel | Niedrig | Phase 2 |
| **13** | Mod-Manager | 🟡 Mittel | Hoch | Phase 2, 3 |
| **14** | NPC-Editor | 🟢 Normal | Mittel | Phase 2, 5 |
| **15** | Flight-Mode | 🟢 Normal | Sehr Hoch | Phase 7, 8 |
| **16** | 3D-Modell-Vorschau & Charakter | 🟢 Normal | Hoch | Phase 7, 8 |
| **17** | Infocard-Handling | 🟢 Normal | Niedrig | Phase 2, 3 |
| **18** | News/Rumor-Editor | 🟢 Niedrig | Niedrig | Phase 2, 3 |
| **19** | Jump-Connection-Dialog | 🟢 Niedrig | Niedrig | Phase 4, 5 |
| **20** | DLL/Executable-Handling | 🟢 Normal | Mittel | Phase 1 |
| **21** | Scripting/Patching | 🟢 Niedrig | Niedrig | Phase 1 |
| **22** | Auto-Updater | 🟢 Niedrig | Mittel | Phase 1 |
| **23** | Help-System | 🟢 Niedrig | Niedrig | Phase 3 |
| **24** | Shortest-Path-Generator | 🟢 Niedrig | Niedrig | Phase 2 |

---

## Phase 0: Toolchain & Grundgerüst

**Ziel**: Lauffähiges CMake-Projekt mit Qt6, leeres Fenster, CI-ready.

### Aufgaben
1. Qt6 installieren (Online-Installer, MinGW oder MSVC Kit)
2. CMakeLists.txt (Root + Subdirectories)
3. `main.cpp` → QApplication + leeres QMainWindow
4. `.qrc`-Ressourcen-Datei mit App-Icon
5. `.gitignore` für Build-Artefakte
6. Erfolgreich bauen und starten

### Deliverables
- [ ] `CMakeLists.txt` (Root + src/ + tests/)
- [ ] `src/main.cpp` zeigt leeres Fenster
- [ ] Build erfolgreich mit `cmake --build`
- [ ] App startet und zeigt Fenster

### Definition of Done
✅ `cmake -B build && cmake --build build` kompiliert fehlerfrei
✅ Ausführbare Datei startet und zeigt leeres Qt-Fenster
✅ Git-Repository initialisiert, `.gitignore` vorhanden

### Risiken
- Qt6 Installation kann auf Windows komplex sein (PATH, Kit-Auswahl)
- MinGW vs. MSVC: Empfehlung MinGW (bereits installiert: g++ 15.2)

---

## Phase 1: Core-Infrastruktur

**Ziel**: Grundlegende Dienste, die alle anderen Module benötigen.

### Module
| Modul | Quelle (Python) | Ziel (C++) |
|-------|-----------------|------------|
| Config | `config.json`, AppData | `core/Config.h/cpp` |
| I18n | `translations.json` | `core/I18n.h/cpp` + `.ts`-Dateien |
| Theme | QSS-Paletten | `core/Theme.h/cpp` + `.qss`-Dateien |
| PathUtils | case-insensitive Pfade | `core/PathUtils.h/cpp` |
| Logger | diverse print/log | `core/Logger.h/cpp` |
| UndoManager | Snapshot-Listen | `core/UndoManager.h/cpp` (QUndoStack) |

### Aufgaben
1. **Config**: JSON lesen/schreiben mit QJsonDocument. Legacy-Migration aus AppData.
2. **I18n**: `translations.json` → `.ts` konvertieren. QTranslator-Setup. `tr()`-Makros.
3. **Theme**: 4 QSS-Dateien (dark, light, founder, xp). QPalette-Setup. Dynamischer Wechsel.
4. **PathUtils**: Case-insensitive Pfadauflösung (Windows: native, Linux: Scan). CIFind, CIResolve.
5. **Logger**: qDebug/qWarning/qCritical mit kategorisiertem Output. Optional: Datei-Logging.
6. **UndoManager**: QUndoStack Wrapper. Basis-QUndoCommand-Klasse für spätere Editor-Aktionen.

### Deliverables
- [ ] Config laden/speichern + Unit-Tests
- [ ] Theme-Wechsel funktioniert visuell
- [ ] `tr()`-Strings in Deutsch/Englisch
- [ ] PathUtils mit Tests für case-insensitive Lookup
- [ ] UndoManager mit Basis-Command

### Definition of Done
✅ Alle Unit-Tests grün (≥10 Tests für Config, PathUtils, I18n)
✅ Theme-Wechsel zur Laufzeit funktioniert
✅ Sprachwechsel zur Laufzeit funktioniert
✅ Config wird korrekt in `~/.config/flatlas/config.json` gespeichert

### Risiken
- `translations.json` → `.ts` Konvertierung: Einmaliger Aufwand, Skript schreiben
- Legacy AppData-Migration: Nur Windows, bekannter Pfad

---

## Phase 2: Parser & Datenmodelle

**Ziel**: Alle Freelancer-Datenformate lesen und in Domain-Objekte wandeln.

### Module
| Modul | Quelle (Python) | Ziel (C++) |
|-------|-----------------|------------|
| INI-Parser | `parser.py` | `infrastructure/parser/IniParser.h/cpp` |
| BINI-Decoder | `bini.py` | `infrastructure/parser/BiniDecoder.h/cpp` |
| BINI-Konverter | `bini_conversion.py` | `infrastructure/parser/BiniConverter.h/cpp` |
| Freelancer-Pfade | diverse | `infrastructure/freelancer/FreelancerPaths.h/cpp` |
| Universe-Scanner | `parser.py` (Teil) | `infrastructure/freelancer/UniverseScanner.h/cpp` |
| SystemDocument | `main_window.py` (Teil) | `domain/SystemDocument.h/cpp` |
| SolarObject | `main_window.py` (Teil) | `domain/SolarObject.h/cpp` |
| ZoneItem | `main_window.py` (Teil) | `domain/ZoneItem.h/cpp` |
| UniverseData | diverse | `domain/UniverseData.h/cpp` |

### Aufgaben
1. **IniParser**: Freelancer-INI lesen (Duplikat-Keys pro Section). `QMap<QString, QVector<QPair<QString, QString>>>`. BINI-Auto-Detect (Magic-Bytes).
2. **BiniDecoder**: Header parsen (4 Bytes Magic, 4 Bytes Version, 4 Bytes StringTable-Offset). Section/Entry/Value dekodieren. CP1252→UTF-8-Tabelle.
3. **BiniConverter**: Bulk-Konvertierung ganzer Ordner. BINI→INI Text.
4. **FreelancerPaths**: Registry-basierte Suche (`HKLM\SOFTWARE\Microsoft\Microsoft Games\Freelancer`). Fallback auf bekannte Pfade.
5. **UniverseScanner**: `universe.ini` → alle Systeme finden. `multiuniverse.ini` Support.
6. **Domain-Modelle**: SystemDocument, SolarObject, ZoneItem als reine Datenklassen mit Q_PROPERTY.

### Deliverables
- [ ] INI-Parser liest echte Freelancer-INIs korrekt + Tests
- [ ] BINI-Decoder dekodiert echte `.ini`-BINI-Dateien + Tests
- [ ] Freelancer-Installation wird gefunden
- [ ] Universe-Scanner findet alle Systeme
- [ ] Domain-Modelle mit Serialisierung

### Definition of Done
✅ INI-Parser: 100% Kompatibilität mit Freelancer-Format (inkl. Duplikat-Keys, Kommentare)
✅ BINI-Decoder: Korrekte Dekodierung aller BINI-Dateien in einer Freelancer-Installation
✅ ≥20 Unit-Tests für Parser/Decoder
✅ Domain-Modelle mit vollständigen Properties

### Risiken
- 🔴 **INI-Format-Quirks**: Freelancer-INI hat undokumentierte Sonderfälle. Ausgiebig mit echten Dateien testen.
- 🟡 **BINI CP1252**: Encoding-Tabelle muss exakt stimmen (128 Einträge, 0x80-0xFF).

---

## Phase 3: Hauptfenster & Navigation

**Ziel**: Funktionales Hauptfenster mit Menüs, Panels, Tabs.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| MainWindow | `ui/MainWindow.h/cpp` |
| WelcomePage | `ui/WelcomePage.h/cpp` |
| BrowserPanel | `ui/BrowserPanel.h/cpp` |
| PropertiesPanel | `ui/PropertiesPanel.h/cpp` |
| CenterTabWidget | `ui/CenterTabWidget.h/cpp` |
| SettingsDialog | `ui/SettingsDialog.h/cpp` |
| SplashScreen | `app/SplashScreen.h/cpp` |

### Aufgaben
1. **MainWindow**: 3-Panel-Splitter (Browser | Center Tabs | Properties). Menübar aufbauen. Statusbar.
2. **WelcomePage**: Startseite mit Recent-Files, Quick-Actions. Wird als erster Tab gezeigt.
3. **BrowserPanel**: QTreeView mit System-Liste. Filter-Textfeld. Lazy-Loading.
4. **PropertiesPanel**: QTreeView mit Custom Model. Zeigt Properties des ausgewählten Objekts.
5. **CenterTabWidget**: QTabWidget mit Custom TabBar (Drag-Reorder). Tab-Schließen. Tab-Kontextmenü.
6. **SettingsDialog**: Pfade, Theme, Sprache, Updates. QDialogButtonBox.
7. **SplashScreen**: QSplashScreen-Subklasse mit QProgressBar.

### Deliverables
- [ ] Hauptfenster mit 3-Panel-Layout
- [ ] Menübar mit allen Hauptmenüs (Platzhalter-Actions)
- [ ] Welcome-Page als Standard-Tab
- [ ] Browser-Panel mit Dummy-Daten
- [ ] Settings-Dialog öffnet sich
- [ ] Splash-Screen beim Start
- [ ] Fenstergröße/Position wird gespeichert (via Config)

### Definition of Done
✅ App startet mit Splash → Hauptfenster mit korrektem Layout
✅ Menüs reagieren (mindestens Exit, Settings, Theme-Wechsel, Sprachwechsel)
✅ Fensterlayout wird beim Schließen gespeichert und beim Start wiederhergestellt
✅ Theme-Wechsel wirkt sofort auf alle Widgets

### Risiken
- 🟢 Niedrig. Standard-Qt-Patterns. Kein unbekanntes Terrain.

---

## Phase 4: 2D-Systemkarte

**Ziel**: Interaktive 2D-Ansicht eines Freelancer-Systems.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| SystemMapView | `rendering/view2d/SystemMapView.h/cpp` |
| MapScene | `rendering/view2d/MapScene.h/cpp` |
| SolarObjectItem | `rendering/view2d/items/SolarObjectItem.h/cpp` |
| ZoneItem2D | `rendering/view2d/items/ZoneItem2D.h/cpp` |
| TradelaneItem | `rendering/view2d/items/TradelaneItem.h/cpp` |

### Aufgaben
1. **SystemMapView**: QGraphicsView mit Zoom (Mausrad), Pan (Mittel-Klick/Rechts-Klick), Rubber-Band-Selektion.
2. **MapScene**: QGraphicsScene. Objekte aus SystemDocument laden. Grid zeichnen.
3. **Items**: Custom QGraphicsItem-Subklassen für SolarObjects, Zonen (Ellipsen/Rechtecke/Kugeln), Tradelanes (Linien).
4. **Interaktion**: Klick-Selektion, Multi-Select, Drag zum Verschieben, Kontextmenü.
5. **Anbindung**: SystemDocument → MapScene → Selektion → PropertiesPanel.

### Deliverables
- [ ] 2D-Karte zeigt geladenes System
- [ ] Zoom/Pan funktioniert
- [ ] Objekte/Zonen selektierbar
- [ ] Drag-Verschieben von Objekten
- [ ] Grid-Overlay optional
- [ ] Kontextmenü auf Objekten

### Definition of Done
✅ Echtes Freelancer-System (z.B. Li01) korrekt dargestellt
✅ Selektion → Properties-Panel zeigt Objektdaten
✅ Verschieben ändert Position im SystemDocument
✅ Undo/Redo für Verschieben funktioniert

### Risiken
- 🟡 **Performance**: Große Systeme mit 500+ Objekten. QGraphicsView skaliert gut, aber Items sollten keine komplexen paintEvents haben.
- 🟡 **Koordinatensystem**: Freelancer-Koordinaten (float, Y-Up) → Qt-Graphics (Y-Down). Transformation korrekt implementieren.

---

## Phase 5: System-Editor (Basis)

**Ziel**: Systeme laden, bearbeiten, speichern.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| SystemEditorPage | `editors/system/SystemEditorPage.h/cpp` |
| SystemCreationWizard | `editors/system/SystemCreationWizard.h/cpp` |
| SystemPersistence | `editors/system/SystemPersistence.h/cpp` |

### Aufgaben
1. **SystemEditorPage**: Haupt-Widget für System-Bearbeitung. Enthält 2D-View + Property-Panel-Anbindung.
2. **SystemCreationWizard**: QWizard für neue Systeme. Name, Archetype, Basisparameter.
3. **SystemPersistence**: INI-Serialisierung. System laden → SystemDocument → INI schreiben.
4. **Object-CRUD**: Objekte erstellen, duplizieren, löschen. Undo-Commands für jede Aktion.
5. **Infocard-Draft**: Temporäre IDS-Zuweisungen für neue Objekte.

### Deliverables
- [ ] System aus INI laden und in Editor anzeigen
- [ ] Objekte erstellen, bearbeiten, löschen
- [ ] System als INI speichern
- [ ] Undo/Redo für alle Aktionen
- [ ] Neues System erstellen (Wizard)

### Definition of Done
✅ Laden → Bearbeiten → Speichern → Erneut Laden ergibt identisches Ergebnis
✅ Undo/Redo-Stack funktioniert für alle Bearbeitungen
✅ Neues System kann erstellt und gespeichert werden

### Risiken
- 🔴 **Serialisierung**: INI-Roundtrip muss verlustfrei sein. Kommentare, Leerzeilen, Reihenfolge beibehalten.
- 🟡 **Undo-Granularität**: Jede Aktion braucht ein QUndoCommand. Bei vielen Aktionstypen aufwändig.

---

## Phase 6: INI-Editor

**Ziel**: Syntax-Highlighting-Editor für Freelancer-INI-Dateien.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| IniEditorPage | `editors/ini/IniEditorPage.h/cpp` |
| IniCodeEditor | `editors/ini/IniCodeEditor.h/cpp` |
| IniSyntaxHighlighter | `editors/ini/IniSyntaxHighlighter.h/cpp` |

### Aufgaben
1. **IniCodeEditor**: QPlainTextEdit-Subklasse mit Zeilennummern-Bereich, Minimap, Suchen/Ersetzen.
2. **IniSyntaxHighlighter**: QSyntaxHighlighter für INI-Format (Sections grün, Keys blau, Kommentare grau, Werte normal).
3. **IniEditorPage**: Tab-Page. Datei öffnen/speichern. Dirty-Tracking. BINI-Auto-Decode.

### Deliverables
- [ ] INI-Datei mit Syntax-Highlighting anzeigen
- [ ] Zeilennummern und Minimap
- [ ] Suchen/Ersetzen
- [ ] Speichern mit Dirty-Tracking
- [ ] BINI-Dateien automatisch als Text anzeigen

### Definition of Done
✅ Alle Freelancer-INI-Dateien korrekt dargestellt
✅ BINI-Dateien transparent als INI-Text geöffnet
✅ Änderungen speicherbar (als INI-Text, nicht zurück in BINI)

### Risiken
- 🟢 Niedrig. QPlainTextEdit + QSyntaxHighlighter ist Standard-Qt.

---

## Phase 7: 3D-View (Basis)

**Ziel**: Grundlegende 3D-Szene mit Kamera und Selektion.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| SceneView3D | `rendering/view3d/SceneView3D.h/cpp` |
| OrbitCamera | `rendering/view3d/OrbitCamera.h/cpp` |
| SelectionManager | `rendering/view3d/SelectionManager.h/cpp` |
| SkyRenderer | `rendering/view3d/SkyRenderer.h/cpp` |

### Aufgaben
1. **SceneView3D**: Qt3DWindow (oder QWidget mit Qt3D Surface). Scene Root, FrameGraph, RenderSettings.
2. **OrbitCamera**: Orbit um Ziel-Punkt. Zoom (Mausrad), Rotate (Rechts-Klick), Pan (Mittel-Klick).
3. **SelectionManager**: Object-Picking via Qt3D RayCasting. Highlight-Material für selektierte Objekte.
4. **SkyRenderer**: Hintergrund-Skybox oder Nebula-Rendering (Cube-Map oder prozedural).
5. **Placeholder-Geometrie**: Einfache Primitives (Spheres, Cubes) als Platzhalter für CMP-Modelle.

### Deliverables
- [ ] 3D-Szene mit Orbit-Kamera
- [ ] Placeholder-Objekte aus SystemDocument positioniert
- [ ] Objekt-Selektion via Klick
- [ ] Sky-Hintergrund
- [ ] Synchronisation mit 2D-Selektion

### Definition of Done
✅ 3D-Ansicht zeigt Systemobjekte an korrekten Positionen
✅ Kamera rotiert, zoomt, pant flüssig
✅ Selektion in 3D ↔ 2D synchron
✅ Mindestens 30 FPS bei typischer Systemgröße

### Risiken
- 🟡 **Qt3D API**: C++-API unterscheidet sich leicht von Python-Bindings. Entity-Component-System-Muster.
- 🟡 **Performance**: Qt3D Overhead bei vielen Entities. Scene-Graph-Optimierung nötig.
- 🔴 **Object-Picking**: Qt3D Picking kann bei vielen Objekten langsam sein. Ggf. Custom-Lösung.

---

## Phase 8: CMP/3DB-Loader & Texturen

**Ziel**: Freelancer-3D-Modelle laden und in Qt3D rendern.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| CmpLoader | `infrastructure/io/CmpLoader.h/cpp` |
| VmeshDecoder | `infrastructure/io/VmeshDecoder.h/cpp` |
| TextureLoader | `infrastructure/io/TextureLoader.h/cpp` |
| MaterialFactory | `rendering/view3d/MaterialFactory.h/cpp` |
| ModelCache | `rendering/preview/ModelCache.h/cpp` |

### Aufgaben
1. **CmpLoader**: CMP-Container parsen. Node-Hierarchie (UTF-Tree). Part-Meshes extrahieren.
2. **VmeshDecoder**: Vertex-Buffer (Position, Normal, UV, Tangent) + Index-Buffer dekodieren. FVF-Flags.
3. **TextureLoader**: DDS (DXT1/DXT5), TGA, TXM laden. → QImage/Qt3DRender::QTexture.
4. **MaterialFactory**: Qt3DExtras-Material erstellen aus Freelancer-Material-Blöcken (Dc, Dt, Et, Bt Texturen).
5. **ModelCache**: LRU-Cache (Key: Dateipfad). Thread-safe. Async-Loading via QThread.

### Deliverables
- [ ] CMP-Dateien laden und Mesh-Geometrie extrahieren
- [ ] 3DB-Dateien (Einzel-Meshes) laden
- [ ] DDS-Texturen auf Modelle anwenden
- [ ] Modelle im 3D-View rendern
- [ ] LRU-Cache funktioniert

### Definition of Done
✅ Mindestens 90% der Freelancer-CMP-Modelle laden korrekt
✅ Texturen korrekt gemappt (UV-Koordinaten stimmen)
✅ Modelle in 3D-Szene platziert und korrekt orientiert
✅ Performance: ≤500ms für typisches Modell, LRU-Cache Hit ≤1ms

### Risiken
- 🔴 **CMP-Format-Komplexität**: Mehrere LOD-Levels, Hardpoints, Fix/Rev/Pris Joints. Inkrementell implementieren.
- 🔴 **VMESH FVF-Flags**: Verschiedene Vertex-Formate je nach Flag-Kombination. Alle Varianten abdecken.
- 🟡 **DDS**: DirectDraw Surface hat viele Sub-Formate. Mindestens DXT1, DXT5, RGBA unterstützen.

---

## Phase 9: Universe-Editor

**Ziel**: Universe-Ebene (alle Systeme, Verbindungen) bearbeiten.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| UniverseEditorPage | `editors/universe/UniverseEditorPage.h/cpp` |
| UniverseSerializer | `editors/universe/UniverseSerializer.h/cpp` |

### Aufgaben
1. **UniverseEditorPage**: Übersichts-Karte aller Systeme. Verbindungen als Linien. System-Selektion → System-Editor öffnen.
2. **UniverseSerializer**: Universe.ini lesen/schreiben. System-Positionen, Verbindungen, Fraktionen.
3. **Infocard-Assignment**: IDS-Nummern für Systemnamen/Beschreibungen zuweisen.
4. **Snapshot-Sektionen**: Universe-Zustand speichern/laden für Undo.

### Deliverables
- [ ] Universe-Übersichtskarte
- [ ] System-Verbindungen sichtbar
- [ ] System erstellen/löschen auf Universe-Ebene
- [ ] Universe.ini Roundtrip

### Definition of Done
✅ Universe wird korrekt aus echten Dateien geladen
✅ Bearbeitungen werden korrekt gespeichert
✅ Navigation: Doppelklick auf System → System-Editor-Tab öffnet sich

### Risiken
- 🟢 Niedrig. Aufbauend auf bereits implementierter Infrastruktur.

---

## Phase 10: Base-Editor

**Ziel**: Freelancer-Basen (Stationen, Planeten) bearbeiten.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| BaseEditorPage | `editors/base/BaseEditorPage.h/cpp` |
| BaseBuilder | `editors/base/BaseBuilder.h/cpp` |
| RoomEditor | `editors/base/RoomEditor.h/cpp` |

### Aufgaben
1. **BaseEditorPage**: Basis-Eigenschaften bearbeiten (Name, Typ, Faction, Archetype).
2. **BaseBuilder**: Neue Basis erstellen mit Templates.
3. **RoomEditor**: Räume einer Basis bearbeiten (Bar, Equip, Commodity, Ship Dealer).

### Deliverables
- [ ] Basis-Properties bearbeiten
- [ ] Räume hinzufügen/entfernen
- [ ] Room-Templates für Standard-Basen

### Definition of Done
✅ Basis aus INI laden und bearbeiten
✅ Neue Basis mit Template erstellen
✅ Räume korrekt serialisiert

### Risiken
- 🟡 **Room-Templates**: Müssen für verschiedene Basis-Typen gepflegt werden.

---

## Phase 11: Trade-Route-Analyse

**Ziel**: Handelsrouten berechnen und visualisieren.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| TradeRoutePage | `editors/trade/TradeRoutePage.h/cpp` |
| MarketScanner | `editors/trade/MarketScanner.h/cpp` |
| TradeScoring | `editors/trade/TradeScoring.h/cpp` |

### Aufgaben
1. **MarketScanner**: Alle `market_*.ini` scannen. Basis↔Commodity↔Preis extrahieren.
2. **TradeScoring**: Profitable Routen berechnen (Profit/Zeiteinheit, Distanz via Jump-Connections).
3. **TradeRoutePage**: Tabellarische Anzeige + Karten-Overlay.

### Deliverables
- [ ] Market-Daten aus INIs extrahiert
- [ ] Top-N profitable Routen berechnet
- [ ] Tabellarische Darstellung
- [ ] Routen auf 2D-Karte hervorgehoben

### Definition of Done
✅ Korrekte Profit-Berechnung für bekannte Routen
✅ ≤5s für vollständigen Scan einer Freelancer-Installation
✅ Ergebnisse stimmen mit Python-Version überein

### Risiken
- 🟢 Niedrig. Algorithmus ist einfach. Nur Daten sammeln und berechnen.

---

## Phase 12: IDS/String-Resourcen

**Ziel**: Freelancer-String-Tabellen (DLL-Resourcen) verwalten.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| IdsEditorPage | `editors/ids/IdsEditorPage.h/cpp` |
| IdsStringTable | `infrastructure/freelancer/IdsStringTable.h/cpp` |
| DllResources | `infrastructure/io/DllResources.h/cpp` |

### Aufgaben
1. **DllResources**: Windows API `LoadLibraryEx(LOAD_LIBRARY_AS_DATAFILE)` + `LoadStringW`. Strings aus `resources.dll`, `infocards.dll` etc.
2. **IdsStringTable**: In-Memory-Tabelle aller IDS-Nummern → Strings. Lazy-Loading.
3. **IdsEditorPage**: Tabelle aller Strings. Suche, Filterung, Bearbeitung.
4. **CSV-Import/Export**: Für Batch-Bearbeitung.

### Deliverables
- [ ] DLL-Strings laden (Windows API)
- [ ] String-Tabelle anzeigen und durchsuchen
- [ ] CSV-Import/Export

### Definition of Done
✅ Alle Strings aus Standard-Freelancer-DLLs korrekt geladen
✅ Suche findet Strings in ≤100ms
✅ Bearbeitete Strings können exportiert werden

### Risiken
- 🟡 **Windows-Only**: `LoadLibraryEx` nur auf Windows. Für Cross-Platform müsste PE-Parser implementiert werden.
- 🟢 Für Windows-only-Ziel: Kein Problem.

---

## Phase 13: Mod-Manager

**Ziel**: Mods verwalten, Konflikte erkennen, aktivieren/deaktivieren.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| ModManagerPage | `editors/modmanager/ModManagerPage.h/cpp` |
| ConflictDetector | `editors/modmanager/ConflictDetector.h/cpp` |
| ModWorkflow | `editors/modmanager/ModWorkflow.h/cpp` |
| ModProfile | `domain/ModProfile.h/cpp` |

### Aufgaben
1. **ModProfile**: Profil mit aktivierten Mods, Reihenfolge, SHA1-IDs.
2. **ConflictDetector**: Dateikonflikte zwischen Mods erkennen. Dateibasierter Vergleich.
3. **ModWorkflow**: Mod aktivieren/deaktivieren. Dateien kopieren/wiederherstellen. Savegame-Policy.
4. **ModManagerPage**: UI mit Profil-Liste, Mod-Liste, Konflikt-Anzeige, Aktivieren/Deaktivieren-Buttons.

### Deliverables
- [ ] Mods scannen und auflisten
- [ ] Profile erstellen und verwalten
- [ ] Konflikte erkennen und anzeigen
- [ ] Mod aktivieren/deaktivieren mit Dateikopie

### Definition of Done
✅ Mods korrekt erkannt und aufgelistet
✅ Konflikte visuell hervorgehoben
✅ Aktivieren → Dateien kopiert → Deaktivieren → Originale wiederhergestellt

### Risiken
- 🟡 **Dateisystem-Operationen**: Fehlerbehandlung bei gesperrten Dateien, fehlenden Rechten.
- 🟡 **Savegame-Kompatibilität**: Mod-Wechsel kann Savegames brechen. Warnung implementieren.

---

## Phase 14: NPC-Editor

**Ziel**: NPCs in Bases bearbeiten (MBases.ini).

### Module
| Modul | Ziel (C++) |
|-------|------------|
| NpcEditorPage | `editors/npc/NpcEditorPage.h/cpp` |
| MbaseOperations | `editors/npc/MbaseOperations.h/cpp` |

### Aufgaben
1. **MbaseOperations**: MBases.ini parsen. NPC-Blöcke extrahieren. Multiline-Werte.
2. **NpcEditorPage**: NPCs pro Raum anzeigen. Dichte, Typ, Dialoge bearbeiten.

### Deliverables
- [ ] NPCs aus MBases.ini laden und anzeigen
- [ ] NPC-Eigenschaften bearbeiten
- [ ] Speichern in MBases.ini

### Definition of Done
✅ MBases.ini korrekt geladen und gespeichert (Roundtrip)
✅ NPC-Bearbeitung funktioniert

### Risiken
- 🟡 **MBases.ini Format**: Komplex mit verschachtelten Blöcken und Multiline-Werten.

---

## Phase 15: Flight-Mode

**Ziel**: Freelancer-artiger Flugmodus im 3D-View.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| FlightController | `rendering/flight/FlightController.h/cpp` |
| FlightHud | `rendering/flight/FlightHud.h/cpp` |
| Autopilot | `rendering/flight/Autopilot.h/cpp` |
| FlightCamera | `rendering/flight/FlightCamera.h/cpp` |
| DustParticles | `rendering/flight/DustParticles.h/cpp` |
| FlightNavigation | `rendering/flight/FlightNavigation.h/cpp` |

### Aufgaben
1. **FlightController**: State-Machine (Docked, Cruise, Normal, Formation). Tastatur/Maus-Input → Geschwindigkeit/Richtung.
2. **FlightHud**: 2D-Overlay auf 3D-View. Geschwindigkeit, Ziel, Kompass, Nachrichten.
3. **Autopilot**: Automatische Navigation zu Ziel-Punkt. Ausweichen. Cruise-Aktivierung.
4. **FlightCamera**: Chase-Kamera hinter Spieler. Smooth-Follow. Freelancer-typisch.
5. **DustParticles**: Partikel-Effekte die Geschwindigkeit visualisieren.
6. **FlightNavigation**: Wegpunkte, Jump-Gates, Docking-Approach.

### Deliverables
- [ ] Fliegen durch System möglich
- [ ] HUD zeigt Geschwindigkeit und Ziel
- [ ] Autopilot fliegt zu selektiertem Objekt
- [ ] Dust-Partikel bei Bewegung
- [ ] Docking an Basen/Gates

### Definition of Done
✅ Frei fliegen mit Maus/Tastatur
✅ Autopilot funktioniert zuverlässig
✅ HUD lesbar und korrekt
✅ Flüssige 30+ FPS während Flug

### Risiken
- 🔴 **Komplexität**: 28 Python-Dateien. Größtes einzelnes Subsystem. Aufteilen in mehrere Sprints.
- 🟡 **Physik**: Beschleunigung, Trägheit, Drehung müssen sich „Freelancer-artig" anfühlen.

---

## Phase 16: 3D-Modell-Vorschau & Charakter

**Ziel**: Standalone-Dialoge für 3D-Modell- und Charakter-Preview.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| ModelPreview | `rendering/preview/ModelPreview.h/cpp` |
| CharacterPreview | `rendering/preview/CharacterPreview.h/cpp` |

### Aufgaben
1. **ModelPreview**: QDialog mit Qt3D-View. CMP laden und rotierbar anzeigen.
2. **CharacterPreview**: QDialog mit Charakter-Modell. Kostüm/Kopf/Körper selektierbar.

### Deliverables
- [ ] Modell-Vorschau als eigenständiger Dialog
- [ ] Charakter-Vorschau mit Varianten

### Definition of Done
✅ Beliebiges CMP-Modell kann angezeigt werden
✅ Orbit-Kamera im Dialog funktioniert

### Risiken
- 🟢 Niedrig (baut auf Phase 7+8 auf).

---

## Phase 17: Infocard-Handling

**Ziel**: XML-Infocards lesen, validieren, bearbeiten.

### Module
| Modul | Ziel (C++) |
|-------|------------|
| InfocardEditor | `editors/infocard/InfocardEditor.h/cpp` |
| XmlInfocard | `infrastructure/parser/XmlInfocard.h/cpp` |

### Aufgaben
1. **XmlInfocard**: Freelancer-XML-Infocards parsen/validieren. Nicht-Standard-XML-Fragmente handhaben.
2. **InfocardEditor**: Rich-Text-Preview + XML-Source-Editor.

### Deliverables
- [ ] Infocards laden und anzeigen (formatiert)
- [ ] XML-Quelle bearbeiten
- [ ] Validierung bei Speichern

### Definition of Done
✅ Standard-Freelancer-Infocards korrekt angezeigt
✅ Bearbeitungen valide gespeichert

### Risiken
- 🟡 **Nicht-Standard-XML**: Freelancer-Infocards sind nicht immer wohlgeformt. Robuster Parser nötig.

---

## Phase 18: News/Rumor-Editor

**Ziel**: News- und Rumor-Texte bearbeiten.

### Aufgaben
1. News-Texte aus INI/CSV laden
2. Tabellarischer Editor mit Suche
3. Speichern

### Deliverables
- [ ] News/Rumors laden und bearbeiten

### Definition of Done
✅ Roundtrip funktioniert

### Risiken
- 🟢 Niedrig. Einfacher Text-Editor.

---

## Phase 19: Jump-Connection-Dialog

**Ziel**: Jump-Verbindungen zwischen Systemen erstellen/bearbeiten.

### Aufgaben
1. Dialog für Jump-Gate/Jump-Hole-Paare
2. Automatische Gegenseite erstellen
3. Validierung (keine Duplikate, korrekte Objekt-Referenzen)

### Deliverables
- [ ] Jump-Connection erstellen/bearbeiten/löschen

### Definition of Done
✅ Neue Jump-Connection erstellt korrekte INI-Einträge in beiden Systemen

### Risiken
- 🟢 Niedrig.

---

## Phase 20: DLL/Executable-Handling

**Ziel**: PE-Executable-Informationen auslesen und bearbeiten.

### Aufgaben
1. **DllResources**: Windows API für String-Extraktion (siehe Phase 12)
2. **EXE-Version**: Versionsinformationen aus PE-Header lesen
3. **Debug-Output**: Freelancer.exe Debug-Ausgaben erfassen

### Deliverables
- [ ] EXE-Version auslesen
- [ ] Debug-Output-Capture

### Definition of Done
✅ Version korrekt für alle bekannten Freelancer-EXEs

### Risiken
- 🟡 Windows-only APIs.

---

## Phase 21: Scripting/Patching

**Ziel**: Freelancer-EXE patchen (OpenSP, Resolution).

### Aufgaben
1. **OpenSP-Patch**: Single-Player → Open-SP-Modus
2. **Resolution-Patch**: Benutzerdefinierte Auflösung in EXE schreiben
3. **SP-Starter**: Freelancer im Singleplayer starten

### Deliverables
- [ ] OpenSP-Patch anwenden/entfernen
- [ ] Resolution-Patch anwenden

### Definition of Done
✅ Patches funktionieren mit Standard-Freelancer-EXE
✅ Backup vor Patch erstellt

### Risiken
- 🔴 **Binär-Patching**: Offset-abhängig. Muss für jede EXE-Version validiert werden.

---

## Phase 22: Auto-Updater

**Ziel**: Automatische Updates über GitHub Releases.

### Aufgaben
1. GitHub-API: Neueste Version prüfen
2. ZIP-Download via QNetworkAccessManager
3. Entpacken und Dateien ersetzen
4. Neustart der Anwendung

### Deliverables
- [ ] Update-Check bei Start
- [ ] Download und Installation
- [ ] Fortschrittsanzeige

### Definition of Done
✅ Update von Version N auf N+1 funktioniert
✅ Rollback bei Fehler

### Risiken
- 🟡 **Selbst-Ersetzen**: EXE kann sich nicht selbst überschreiben. Helper-Prozess oder Batch-Skript nötig.

---

## Phase 23: Help-System

**Ziel**: In-App-Hilfe mit kontextsensitiver Navigation.

### Aufgaben
1. Help-XML-Dateien laden und als HTML rendern
2. QTextBrowser oder QWebEngineView für Anzeige
3. Kontextsensitive Hilfe: F1 → passende Hilfe-Seite

### Deliverables
- [ ] Hilfe-Fenster mit Navigation
- [ ] F1-Kontext-Hilfe

### Definition of Done
✅ Alle Hilfe-Seiten lesbar
✅ F1 öffnet korrekte Seite

### Risiken
- 🟢 Niedrig. QTextBrowser reicht für einfaches HTML.

---

## Phase 24: Shortest-Path-Generator

**Ziel**: Kürzesten Pfad zwischen zwei Systemen berechnen.

### Aufgaben
1. Dijkstra über Jump-Connection-Graph
2. Ergebnis visualisieren (auf Universe-Karte hervorheben)

### Deliverables
- [ ] Pathfinding-Algorithmus
- [ ] Pfad-Visualisierung

### Definition of Done
✅ Korrekte kürzeste Pfade für bekannte Routen
✅ ≤100ms für Berechnung

### Risiken
- 🟢 Niedrig. Standard-Algorithmus.

---

## Zusammenfassung: Kritischer Pfad

```
Phase 0 (Toolchain) ─→ Phase 1 (Core) ─→ Phase 2 (Parser) ─→ Phase 3 (Hauptfenster)
                                                │                        │
                                                ▼                        ▼
                                          Phase 4 (2D-Karte) ──→ Phase 5 (System-Editor)
                                                │                        │
                                                ▼                        ▼
                                          Phase 7 (3D-View) ──→ Phase 8 (CMP-Loader)
                                                                         │
                                          Phase 6 (INI-Editor)           ▼
                                          Phase 9 (Universe)       Phase 15 (Flight)
                                          Phase 10 (Base)          Phase 16 (Preview)
                                          Phase 11 (Trade)
                                          Phase 12 (IDS)
                                          Phase 13 (Mod-Manager)
                                          Phase 14 (NPC)
                                          ...
```

**Minimaler lauffähiger Editor**: Phase 0 → 1 → 2 → 3 → 4 → 5 → 6
**Mit 3D**: + Phase 7 → 8
**Vollständig**: Alle 24 Phasen

## Offene Fragen & Technische Schulden

1. **Qt6-Installation**: Welches Kit? MinGW (bereits vorhanden) oder MSVC? → Empfehlung: Qt6 für MinGW
2. **Qt3D Zukunft**: Qt3D wird in Qt6 weniger aktiv weiterentwickelt. Alternative: Qt Quick 3D? → Entscheidung in Phase 7
3. **Cross-Platform**: Aktuell Windows-only. Linux-Support später möglich, aber DLL-Handling und Patching sind Windows-spezifisch.
4. **Testing-Strategie**: Unit-Tests für Domain/Infrastructure ab Phase 1. Integration-Tests ab Phase 5.
5. **CI/CD**: GitHub Actions mit Qt6 + MinGW. → Nach Phase 0 einrichten.
