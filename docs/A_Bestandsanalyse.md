# A. Bestandsanalyse – FLAtlas Python/PySide6

## 1. Projektübersicht

| Aspekt | Wert |
|--------|------|
| **Sprache** | Python 3.10+ |
| **GUI-Framework** | PySide6 (Qt 6 Bindings) |
| **Quellordner** | `C:\Users\steve\Github\FLAtlas\fl_editor\` |
| **Dateien** | ~200 `.py`-Dateien |
| **Geschätzte LOC** | 80.000–150.000 Zeilen Python |
| **Hauptdatei** | `main_window.py` (~40.000 Zeilen) |
| **Entry-Point** | `fl_atlas.py` |
| **Build-Dependencies** | PySide6 ≥ 6.6, PyInstaller ≥ 6.0, pefile ≥ 2023.2.7, pytest ≥ 9.0 |
| **Zielplattform** | Windows (primär) |
| **Zweck** | Freelancer-Modding-Editor-Suite (Universe, System, Base, Trade, NPC, Mod-Manager, 3D-Viewer) |

## 2. Startup-Flow

```
fl_atlas.py
  ├── Umgebung: Qt3D Renderer Fallback → OpenGL (Windows)
  ├── CLI-Args: --open-system, --test-updater-zip
  ├── QApplication(Fusion-Style, App-Icons 16–256px)
  ├── StartupSplashScreen mit Fortschrittsbalken
  │     6%  → "Initializing runtime"
  │    20%  → "Building interface"
  │    45%  → "Applying theme"
  │    97%  → "Opening system"
  │   100%  → "Ready"
  ├── MainWindow(startup_progress_callback)
  ├── Fenstergeometrie/Position
  └── Verzögerter Update-Check (1400ms Timer)
```

## 3. Architektur: Hauptfenster

### 3.1 Klassen in `main_window.py` (13 Klassen)

| Klasse | Zeile | Funktion |
|--------|-------|----------|
| `CenterTabBar` | 127 | Tab-Reorder-Tracking |
| `_NumericTableWidgetItem` | 628 | Numerische Tabellensortierung |
| `_IniLineNumberArea` | 648 | INI-Editor Zeilennummern |
| `_IniMiniMap` | 660 | INI-Datei Minimap |
| `_TextOverviewMiniMap` | 858 | Text-Übersicht |
| `_RevisionTimelineStrip` | 1088 | Versionshistorie-Timeline |
| `_IniCodeEditor` | 1191 | Syntax-Highlighting-INI-Editor |
| `_IniSyntaxHighlighter` | 1291 | Syntaxfärbung |
| `SystemDocument` | 1429 | Dokumenten-Datenmodell |
| `WorkspaceLayoutState` | 1485 | Layout-Persistenz |
| `SystemEditorHost` | 1498 | Editor-Kontext |
| `IniEditorDocument` | 1505 | INI-Dokument-Modell |
| **`MainWindow`** | 1513 | Hauptfenster (QMainWindow) |

### 3.2 Layout-Struktur

```
┌──────────────────────────────────────────────────────────┐
│  MenuBar: File | Edit | View | Tools | Suite | Settings  │
│           | Language | Help                               │
├──────────┬────────────────────────┬──────────────────────┤
│  Left    │     Center             │   Right              │
│  Panel   │     (Tabbed)           │   Panel              │
│ (Browser)│  System Editor/Views   │  (Properties/        │
│  240px   │   CenterTabBar with    │   Inspector)         │
│          │   reorder support      │   320px              │
├──────────┴────────────────────────┴──────────────────────┤
│  [QDockWidget: Flight-Info (links, nicht entfernbar)]    │
└──────────────────────────────────────────────────────────┘
```

### 3.3 Menüstruktur

- **File**: New System, Open/Recent, Save, Settings, Exit
- **Edit**: Undo/Redo, System-Editor Create/Edit Submenus, Cut/Copy/Paste
- **View**: System-Namen, Zoom, Grid-Toggles, 2D/3D-Sichtbarkeit
- **Tools**: Game-Path-Setup, Trade-Route-Analyse, Model-Viewer, Character-Preview
- **Suite Apps**: FL-Lingo, Save-Editor, Launcher (externe Tools)
- **Settings**: Theme (founder, dark, light, xp, custom), Language, Auto-Naming
- **Language**: English, German
- **Help**: About, Documentation, Forum-Links

### 3.4 Instanzvariablen (~250+)

- Selektions-State (Objekte, Zonen, Multi-Selection)
- Dirty-Flag + Undo/Redo-History (Snapshot-basiert)
- Caches: Musik, Hintergründe, Fraktionen, DLL-Strings, Modell-Maps
- Universe-View: Sektoren, Verbindungen, Overlap-Gruppen
- Mod-Manager: Profile, aktiver Kontext

## 4. Fachliche Subsysteme

### 4.1 UI-Framework & Hauptfenster
**Dateien**: `main_window.py` (Kern), `fl_atlas.py` (Entry), `browser.py`, `dialogs.py` (~3.500 Zeilen, 40+ Dialog-Klassen)
**Technologie**: QMainWindow, QSplitter (3-Panel), QTabWidget, QDockWidget, QMenu

### 4.2 2D-Systemkarte
**Dateien**: `view_2d.py` (~2.000 Zeilen), Modell-Dateien, Icons
**Technologie**: QGraphicsView / QGraphicsScene mit Custom-Items (Zonen, Objekte, Tradelanes)

### 4.3 3D-View
**Dateien**: `view_3d.py` (~2.500 Zeilen), Kamera, Gizmo, Materialien, Sky, Selektion (~25 Dateien)
**Technologie**: Qt3D (Scene, Camera, RenderSettings, Materials)

### 4.4 Flight-Mode
**Dateien**: ~28 Dateien (~20.000 Zeilen)
**Subsysteme**: Flugsteuerung, HUD, Autopilot, Navigation, Kamera, Dust-Particles
**Pattern**: Funktionale Komposition – Module sind reine Zustandstransformer

### 4.5 Native 3D Preview
**Dateien**: ~13 Dateien (~5.000–8.000 Zeilen)
**Subsysteme**: CMP/3DB-Loader (struct-basiertes Binär-Parsing), VMESH-Decoder, Material/Textur (.DDS/.TGA/.TXM), LRU-Cache, Async-Loading
**Technologie**: Qt3D-Szene

### 4.6 System-Editor
**Dateien**: ~11 Dateien
**Subsysteme**: Erstellung, Dialog, Document-Runtime, Persistence, Infocard-Draft, Name-Runtime, Tab-Management

### 4.7 Universe-Editor
**Dateien**: ~5 Dateien
**Subsysteme**: Edit-State, Infocard-Assignment, Serialisierung, Snapshot-Sektionen

### 4.8 Base-Editor
**Dateien**: ~12 Dateien
**Subsysteme**: Erstellung, Builder, Room-Templates, Room-Editor

### 4.9 Trade-Route-Analyse
**Dateien**: ~9 Dateien (~2.500 Zeilen UI + Backend)
**Subsysteme**: Market-Scanning, Scoring/Analyse, Custom-Storage, Market-Editor

### 4.10 IDS/String-Resourcen
**Dateien**: ~3 Dateien
**Subsysteme**: CSV-Import, Resource-Runtime, Toolchain-Erkennung

### 4.11 INI-Editor
**Dateien**: ~4 Dateien
**Subsysteme**: File-Context, Editor-Page, Section-Writes
**Features**: Syntax-Highlighting, Minimap, Zeilennummern, Code-Folding

### 4.12 Mod-Manager
**Dateien**: ~11 Dateien
**Subsysteme**: Conflict-Detection, Resolution/Aspect-Ratio, Savegame-Policy, Workflows, Profiles

### 4.13 NPC-Editor
**Dateien**: ~5 Dateien
**Subsysteme**: Multiline-Parsing, MBase-Operations, Room-Customizations, Density/Persistence

### 4.14 Infocard-Handling
**Dateien**: ~3 Dateien
**Subsysteme**: XML-Validierung, XML-Helpers, Row-Navigation

### 4.15 DLL/Executable-Handling
**Dateien**: ~3 Dateien
**Subsysteme**: DLL-Resources (via pefile), EXE-Version, Debug-Output

### 4.16 Scripting/Patching
**Dateien**: ~3 Dateien
**Subsysteme**: OpenSP-Patch, Resolution-Patch, SP-Starter

### 4.17 Shortest-Path-Generator (pathgen)
**Dateien**: 1 Datei
**Algorithmus**: Dijkstra/A* über Jump-Connections

### 4.18 News/Rumor-Editor
**Dateien**: ~2 Dateien
**Subsysteme**: News-Parsing, Rumor-CSV

### 4.19 Auto-Updater
**Dateien**: 1 Datei (`flatlas_updater.py`)
**Funktion**: ZIP-basiertes Update, Download, Extract, Restart

### 4.20 Help-System
**Dateien**: 1 Datei + Help-XML-Ressourcen
**Subsysteme**: Kontextsensitive Hilfe, In-App-Browser

### 4.21 Jump-Connection-Dialog
**Dateien**: ~2 Dateien
**Subsysteme**: Jump-Object-Logik, Jump-Dialog

### 4.22 Charakter-Vorschau
**Dateien**: 1 Datei
**Technologie**: Qt3D – 3D-Modell-Rendering im Dialogfenster

### 4.23 Ressource-Dateien
- Bilder (Icons 16–256px, Splash, Toolbar)
- Help-XML-Dateien
- `translations.json` (EN/DE)
- `flvanilla/freelancer.ini` (Referenz-Datei)

## 5. Architektonische Klassifikation

### UI-Schicht
- MainWindow, Browser, Dialogs, Welcome-Page, Splash-Screen
- View-2D (QGraphicsView), View-3D (Qt3D), Flight-HUD
- Alle Editor-Pages (System, Universe, Base, NPC, INI, Trade, Mod)
- Settings-UI, Theme-System

### Domain-Logik (UI-frei)
- SystemDocument, SolarObject, ZoneItem, TradeRouteCandidate
- Undo/Redo-Snapshots, Dirty-Tracking
- BINI-Decoder, INI-Parser (mit Duplikat-Key-Support)
- CMP/3DB-Modell-Parser, VMESH-Decoder, CRC-Tabelle
- Pathfinding (Jump-Connections, Dijkstra)
- Mod-Manager-Logik (Profiles, Conflicts, Resolution)

### Infrastruktur
- Config-Management (`~/.config/fl_editor/config.json`, AppData-Fallback)
- i18n (`translations.json`, EN/DE)
- Theme-System (QSS-Paletten: dark, light, founder, xp)
- Auto-Updater (ZIP-Download, Extract, Restart)
- DLL-Ressourcen-Extraktion (pefile)
- Logging, Fehlerbehandlung

### Rendering
- Qt3D: 3D-Szene, Kamera, Gizmo, Sky, Materialien
- QGraphicsView: 2D-Karte, Custom-Items
- CMP/3DB Binary-Mesh-Loading, DDS/TGA/TXM-Texturen
- LRU-Cache für 3D-Modelle
- Asynchrones Laden (Background-Threading)

### Parsing/Serialisierung
- INI-Parser (Freelancer-Format: Duplikat-Keys, Section-Zuordnung)
- BINI-Decoder (Magic `b"BINI"`, 12-Byte-Header, CP1252)
- CMP/3DB Struct-basiertes Binärformat
- UTF-Parser (Modellbaum)
- XML-Infocards
- DLL-String-Resources
- CSV-Import (IDS, News, Rumors)

### Tools/Hilfen
- Shortest-Path-Generator
- Trade-Route-Scoring
- News/Rumor-Editor
- Character-Preview
- Help-System

## 6. Kritische / Risikoreiche Bereiche

| Bereich | Risiko | Begründung |
|---------|--------|------------|
| **main_window.py** | 🔴 HOCH | ~40.000 Zeilen monolithischer Code. Muss in 15+ Klassen/Module aufgeteilt werden. |
| **CMP/3DB-Loader** | 🔴 HOCH | Komplexes Binärformat, struct-basiertes Parsing, CRC-Tabelle. Fehleranfällig bei C++-Migration. |
| **Flight-Mode** | 🟡 MITTEL | 28 Dateien mit funktionalem Pattern. Gut isoliert, aber komplex (Physik, Kamera, HUD). |
| **Qt3D-Integration** | 🟡 MITTEL | Qt3D API-Unterschiede Python↔C++. Szenegraph-Aufbau, Material-Setup, Rendering-Pipeline. |
| **DLL-Ressourcen** | 🟡 MITTEL | pefile-Ersatz nötig. Windows PE-Format-Parsing in C++ manuell oder via WinAPI. |
| **Undo/Redo** | 🟡 MITTEL | Snapshot-basiert mit tiefen Kopien. Muss in C++ mit QUndoStack/QUndoCommand gelöst werden. |
| **INI-Parser** | 🟢 NIEDRIG | Gut definiert, bereits in Go implementiert. Einfache C++-Portierung. |
| **Config/i18n** | 🟢 NIEDRIG | Qt hat natives QSettings, QTranslator. Direktes Mapping möglich. |
| **Theme-System** | 🟢 NIEDRIG | QSS wird in C++ identisch verwendet. |
