# B. Zielarchitektur – C++/Qt 6

## 1. Projektstruktur

```
FLAtlas-V2/
├── CMakeLists.txt                   # Root-CMake: Projekt-Definition, Subdirectories
├── cmake/
│   └── Qt6Helpers.cmake             # Qt-spezifische CMake-Hilfsmakros
├── docs/
│   ├── A_Bestandsanalyse.md
│   ├── B_Zielarchitektur.md
│   ├── C_Migrationsplan.md
│   └── E_Dokumentation.md
├── resources/
│   ├── resources.qrc                # Qt-Ressourcen-Datei
│   ├── icons/                       # App-Icons (16–256px, .ico, .png)
│   ├── images/                      # Splash, Toolbar-Icons, Hintergründe
│   ├── help/                        # Help-XML-Dateien
│   ├── translations/
│   │   ├── flatlas_de.ts            # Deutsche Übersetzung
│   │   ├── flatlas_en.ts            # Englische Übersetzung (Fallback)
│   │   └── CMakeLists.txt           # lupdate/lrelease Targets
│   └── data/
│       └── freelancer.ini           # Referenz-Vanilla-Config
├── src/
│   ├── CMakeLists.txt               # Alle Source-Targets
│   ├── main.cpp                     # Entry-Point
│   ├── app/
│   │   ├── CMakeLists.txt
│   │   ├── Application.h/cpp        # QApplication-Subklasse, Startup-Logik
│   │   ├── SplashScreen.h/cpp       # Splash mit Fortschrittsbalken
│   │   └── AppConfig.h/cpp          # Konfigurationsmanagement
│   ├── ui/
│   │   ├── CMakeLists.txt
│   │   ├── MainWindow.h/cpp         # Hauptfenster (schlank, delegiert an Module)
│   │   ├── WelcomePage.h/cpp        # Startseite
│   │   ├── BrowserPanel.h/cpp       # Linke Sidebar: System-Browser
│   │   ├── PropertiesPanel.h/cpp    # Rechte Sidebar: Eigenschaften-Inspector
│   │   ├── CenterTabWidget.h/cpp    # Tab-Container mit Reorder
│   │   ├── SettingsDialog.h/cpp     # Einstellungsdialog
│   │   ├── StatusBarManager.h/cpp   # Statusbar-Verwaltung
│   │   └── dialogs/
│   │       ├── CMakeLists.txt
│   │       └── ...                  # Einzelne Dialog-Klassen
│   ├── core/
│   │   ├── CMakeLists.txt
│   │   ├── Config.h/cpp             # JSON-Config laden/speichern
│   │   ├── I18n.h/cpp               # Übersetzungsmanagement
│   │   ├── Theme.h/cpp              # Theme/QSS-Paletten
│   │   ├── Logger.h/cpp             # Strukturiertes Logging
│   │   ├── PathUtils.h/cpp          # Pfad-Hilfsfunktionen, case-insensitive
│   │   └── UndoManager.h/cpp        # QUndoStack-basierter Undo/Redo
│   ├── domain/
│   │   ├── CMakeLists.txt
│   │   ├── SystemDocument.h/cpp     # Systemdokument-Datenmodell
│   │   ├── SolarObject.h/cpp        # Freelancer-Objekt
│   │   ├── ZoneItem.h/cpp           # Zone im System
│   │   ├── UniverseData.h/cpp       # Universe-Datenmodell
│   │   ├── BaseData.h/cpp           # Basis-Datenmodell
│   │   ├── TradeRoute.h/cpp         # Trade-Route-Modell
│   │   ├── NpcData.h/cpp            # NPC-Datenmodell
│   │   └── ModProfile.h/cpp         # Mod-Manager-Profil
│   ├── infrastructure/
│   │   ├── CMakeLists.txt
│   │   ├── parser/
│   │   │   ├── IniParser.h/cpp      # Freelancer-INI-Parser (Duplikat-Keys)
│   │   │   ├── BiniDecoder.h/cpp    # BINI-Binärformat-Decoder
│   │   │   ├── BiniConverter.h/cpp  # Bulk-BINI→INI-Konvertierung
│   │   │   └── XmlInfocard.h/cpp    # XML-Infocard-Parser
│   │   ├── io/
│   │   │   ├── CmpLoader.h/cpp      # CMP/3DB-Modell-Loader
│   │   │   ├── VmeshDecoder.h/cpp   # VMESH-Geometrie-Dekodierung
│   │   │   ├── TextureLoader.h/cpp  # DDS/TGA/TXM-Texturen
│   │   │   ├── DllResources.h/cpp   # PE-DLL-String-Extraktion
│   │   │   └── CsvImporter.h/cpp    # CSV-Import (IDS, News, Rumors)
│   │   ├── freelancer/
│   │   │   ├── FreelancerPaths.h/cpp # Freelancer-Installation finden
│   │   │   ├── UniverseScanner.h/cpp # Universe-INI scannen, Systeme laden
│   │   │   └── IdsStringTable.h/cpp  # IDS-String-Tabelle verwalten
│   │   ├── updater/
│   │   │   └── AutoUpdater.h/cpp    # ZIP-basiertes Update-System
│   │   └── net/
│   │       └── DownloadManager.h/cpp # HTTP-Downloads (Qt Network)
│   ├── editors/
│   │   ├── CMakeLists.txt
│   │   ├── system/
│   │   │   ├── SystemEditorPage.h/cpp
│   │   │   ├── SystemCreationWizard.h/cpp
│   │   │   └── SystemPersistence.h/cpp
│   │   ├── universe/
│   │   │   ├── UniverseEditorPage.h/cpp
│   │   │   └── UniverseSerializer.h/cpp
│   │   ├── base/
│   │   │   ├── BaseEditorPage.h/cpp
│   │   │   ├── BaseBuilder.h/cpp
│   │   │   └── RoomEditor.h/cpp
│   │   ├── npc/
│   │   │   ├── NpcEditorPage.h/cpp
│   │   │   └── MbaseOperations.h/cpp
│   │   ├── ini/
│   │   │   ├── IniEditorPage.h/cpp
│   │   │   ├── IniCodeEditor.h/cpp
│   │   │   └── IniSyntaxHighlighter.h/cpp
│   │   ├── infocard/
│   │   │   └── InfocardEditor.h/cpp
│   │   ├── trade/
│   │   │   ├── TradeRoutePage.h/cpp
│   │   │   ├── MarketScanner.h/cpp
│   │   │   └── TradeScoring.h/cpp
│   │   ├── news/
│   │   │   └── NewsRumorEditor.h/cpp
│   │   ├── ids/
│   │   │   └── IdsEditorPage.h/cpp
│   │   ├── modmanager/
│   │   │   ├── ModManagerPage.h/cpp
│   │   │   ├── ConflictDetector.h/cpp
│   │   │   └── ModWorkflow.h/cpp
│   │   └── jump/
│   │       └── JumpConnectionDialog.h/cpp
│   ├── rendering/
│   │   ├── CMakeLists.txt
│   │   ├── view2d/
│   │   │   ├── SystemMapView.h/cpp   # QGraphicsView für 2D-Karte
│   │   │   ├── MapScene.h/cpp        # QGraphicsScene
│   │   │   └── items/
│   │   │       ├── SolarObjectItem.h/cpp
│   │   │       ├── ZoneItem2D.h/cpp
│   │   │       └── TrdelaneItem.h/cpp
│   │   ├── view3d/
│   │   │   ├── SceneView3D.h/cpp     # Qt3D-Hauptview
│   │   │   ├── OrbitCamera.h/cpp     # Orbit-Kamera-Controller
│   │   │   ├── TransformGizmo.h/cpp  # 3D-Transformations-Gizmo
│   │   │   ├── SkyRenderer.h/cpp     # Sky/Nebula-Rendering
│   │   │   ├── SelectionManager.h/cpp
│   │   │   └── MaterialFactory.h/cpp # Material-Erstellung
│   │   ├── flight/
│   │   │   ├── FlightController.h/cpp # Flugsteuerung (State-Machine)
│   │   │   ├── FlightHud.h/cpp        # HUD-Overlay
│   │   │   ├── Autopilot.h/cpp
│   │   │   ├── FlightCamera.h/cpp
│   │   │   ├── DustParticles.h/cpp
│   │   │   └── FlightNavigation.h/cpp
│   │   ├── preview/
│   │   │   ├── ModelPreview.h/cpp     # 3D-Modell-Vorschau
│   │   │   ├── CharacterPreview.h/cpp # Charakter-3D-Vorschau
│   │   │   └── ModelCache.h/cpp       # LRU-Cache
│   │   └── pathgen/
│   │       └── ShortestPath.h/cpp     # Dijkstra über Jump-Connections
│   └── tools/
│       ├── CMakeLists.txt
│       ├── ScriptPatcher.h/cpp        # OpenSP-Patch, Resolution-Patch
│       ├── SpStarter.h/cpp            # Singleplayer-Starter
│       └── HelpBrowser.h/cpp          # In-App-Hilfe-System
├── tests/
│   ├── CMakeLists.txt
│   ├── test_IniParser.cpp
│   ├── test_BiniDecoder.cpp
│   ├── test_CmpLoader.cpp
│   ├── test_Config.cpp
│   ├── test_I18n.cpp
│   ├── test_PathUtils.cpp
│   ├── test_TradeScoring.cpp
│   └── test_ShortestPath.cpp
├── third_party/
│   └── CMakeLists.txt                 # Externe Abhängigkeiten
└── .gitignore
```

## 2. Zentrale Klassen und Subsysteme

### 2.1 Application Layer

| Klasse | Verantwortung |
|--------|---------------|
| `Application` | QApplication-Subklasse. Startup-Sequenz, globale Initialisierung, Shutdown. |
| `SplashScreen` | QSplashScreen mit Progress-Bar, phasenbasiertem Fortschritt. |
| `AppConfig` | Singleton. Liest/schreibt `config.json`. Bietet typsichere Getter/Setter. Thread-safe. |

### 2.2 UI Layer

| Klasse | Verantwortung |
|--------|---------------|
| `MainWindow` | **Schlank** (~500-800 Zeilen). Baut Menüs, Splitter, Docks. Delegiert an Panels und Editor-Pages. |
| `BrowserPanel` | Links: Systemliste, Tree-Widget, Filter. |
| `PropertiesPanel` | Rechts: Kontext-abhängige Eigenschaften des ausgewählten Objekts. |
| `CenterTabWidget` | Mitte: Tabbed-Interface für offene Editoren. Custom Tab-Bar mit Drag-Reorder. |
| `WelcomePage` | Startseite mit Recent-Files, Quick-Actions. |
| `SettingsDialog` | Modale Einstellungen (Pfade, Theme, Sprache, Auto-Update). |

### 2.3 Core Layer

| Klasse | Verantwortung |
|--------|---------------|
| `Config` | JSON-basierte Konfiguration. Legacy-Migration aus AppData. |
| `I18n` | Qt-Translator-Integration. Dynamischer Sprachwechsel. |
| `Theme` | QSS-Paletten (dark, light, founder, xp). Dynamischer Theme-Wechsel. |
| `UndoManager` | `QUndoStack` + Custom `QUndoCommand`-Subklassen für jede Aktion. |
| `PathUtils` | Case-insensitive Pfadauflösung (Freelancer-spezifisch). |

### 2.4 Domain Layer (UI-frei, testbar)

| Klasse | Verantwortung |
|--------|---------------|
| `SystemDocument` | Geladenes System mit Objekten, Zonen, Verbindungen. Zentrales Datenmodell. |
| `SolarObject` | Freelancer-Objekt (Position, Rotation, Typ, Archetype, IDS-Referenzen). |
| `ZoneItem` | Zone mit Shape, Größe, Properties. |
| `UniverseData` | Gesamte Universe-Struktur (Systeme, Connections, Fraktionen). |
| `TradeRoute` | Commodity, Basis-Paare, Profit-Berechnung. |
| `ModProfile` | Mod-Profil mit aktiven Mods, Konflikt-Status, Reihenfolge. |

### 2.5 Infrastructure Layer (UI-frei, testbar)

| Klasse | Verantwortung |
|--------|---------------|
| `IniParser` | Freelancer-INI lesen/schreiben. Duplikat-Keys, Section-Zuordnung. |
| `BiniDecoder` | BINI-Binärformat dekodieren (Magic, Header, CP1252). |
| `CmpLoader` | CMP/3DB-Binärformate laden. Struct-Parsing, CRC-Tabelle. |
| `VmeshDecoder` | Vertex-/Index-Buffer aus VMESH-Blöcken. |
| `DllResources` | PE-DLL-String-Extraktion. Windows-API (`LoadLibraryEx`, `LoadString`). |
| `FreelancerPaths` | Freelancer-Installation finden (Registry, bekannte Pfade). |
| `AutoUpdater` | GitHub-Release prüfen, ZIP downloaden, extrahieren, neustarten. |

### 2.6 Rendering Layer

| Klasse | Verantwortung |
|--------|---------------|
| `SystemMapView` | QGraphicsView: 2D-Karte mit Zoom, Pan, Selektion. |
| `SceneView3D` | Qt3D-basierte 3D-Ansicht mit Orbit-Kamera. |
| `FlightController` | State-Machine für Freelancer-artigen Flugmodus. |
| `ModelPreview` | Standalone 3D-Vorschau für CMP-Modelle. |
| `ModelCache` | Thread-safe LRU-Cache für geladene 3D-Modelle. |

## 3. Qt-Technologie-Entscheidungen pro Bereich

| Bereich | Technologie | Begründung |
|---------|-------------|------------|
| **Hauptfenster** | QWidgets + QMainWindow | Standard für Desktop-Apps. Docking, Menüs, Toolbars, Statusbar nativ. |
| **Docking-System** | QDockWidget (Qt native) oder **KDDockWidgets** | Qt-Docking genügt für Start. KDDockWidgets als Upgrade-Option bei Bedarf (Advanced Docking, Tabs in Docks). |
| **2D-Karte** | QGraphicsView / QGraphicsScene | Perfekt für 2D-Karten mit vielen Objekten, Zoom, Pan, Selektion. Bewährt und performant. |
| **3D-Ansicht** | Qt3D (Qt6) | Bereits im Python-Projekt verwendet. Nativer Qt-Szenegraph. Für Editor-Preview ausreichend. |
| **3D-Modell-Loader** | Eigener C++-Loader | Freelancer-CMP/3DB ist proprietär. Kein externes Framework möglich. |
| **Flight-Mode** | Qt3D + Custom Logic | Flugsteuerung als State-Machine über Qt3D-Szene. |
| **INI-Editor** | QPlainTextEdit + QSyntaxHighlighter | Standard für Code-Editoren in Qt. Minimap via QScrollBar-Customization. |
| **Dialoge** | QDialog / QWizard | Standard-Qt-Pattern. |
| **Properties-Panel** | QTreeView + Custom Model | Qt Model/View für strukturierte Eigenschaften. Flexibler als QFormLayout. |
| **Tabellen** | QTableView + QAbstractTableModel | Qt Model/View. Sortierung, Filterung, Custom-Delegates. |
| **Bäume (Browser)** | QTreeView + QAbstractItemModel | Qt Model/View für hierarchische Daten. |
| **Config** | QJsonDocument | Qt-natives JSON-Handling. Kein externes Framework nötig. |
| **Übersetzungen** | Qt Linguist (`.ts`/`.qm`) | Qt-Standard. `lupdate`/`lrelease` in CMake integriert. `tr()` im Code. |
| **Themes** | QSS-Dateien + QPalette | Qt-Standard. Identisch zur Python-Lösung. |
| **HTTP** | QNetworkAccessManager | Qt-natives Networking für Updater/Downloads. |
| **Threading** | QThread + Signals/Slots | Für Async-Loading, Background-Parsing. Kein manuelles Thread-Management. |
| **Testing** | Qt Test + Catch2 | Qt Test für UI-nahes Testing. Catch2 für reine Logik. |
| **DLL-Strings** | Windows API (`LoadLibraryEx`) | Nativer Zugriff statt pefile. Einfacher und schneller in C++. |
| **Texturen** | Qt Image + Custom DDS | QImage für Standard-Formate. Custom DDS-Loader für DirectDraw. |

## 4. Build- und Target-Struktur (CMake)

```cmake
# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.21)
project(FLAtlas VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Gui
    3DCore 3DRender 3DInput 3DExtras
    Network Xml
    LinguistTools
)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(third_party)
```

### CMake-Targets

| Target | Typ | Abhängigkeiten |
|--------|-----|----------------|
| `flatlas_core` | STATIC LIB | Qt6::Core |
| `flatlas_domain` | STATIC LIB | flatlas_core |
| `flatlas_infrastructure` | STATIC LIB | flatlas_core, flatlas_domain, Qt6::Xml, Qt6::Network |
| `flatlas_rendering` | STATIC LIB | flatlas_domain, Qt6::Widgets, Qt6::3DCore, Qt6::3DRender, Qt6::3DInput, Qt6::3DExtras |
| `flatlas_editors` | STATIC LIB | flatlas_domain, flatlas_infrastructure, flatlas_rendering, Qt6::Widgets |
| `flatlas_ui` | STATIC LIB | flatlas_editors, flatlas_rendering, Qt6::Widgets |
| `flatlas_tools` | STATIC LIB | flatlas_core, flatlas_infrastructure |
| **`FLAtlas`** | EXECUTABLE | flatlas_ui, flatlas_tools |
| `flatlas_tests` | TEST EXE | flatlas_infrastructure, flatlas_domain, Qt6::Test |

### Abhängigkeitsgraph

```
FLAtlas (exe)
 ├── flatlas_ui
 │    ├── flatlas_editors
 │    │    ├── flatlas_domain
 │    │    │    └── flatlas_core
 │    │    ├── flatlas_infrastructure
 │    │    │    ├── flatlas_core
 │    │    │    └── flatlas_domain
 │    │    └── flatlas_rendering
 │    │         └── flatlas_domain
 │    └── flatlas_rendering
 └── flatlas_tools
      ├── flatlas_core
      └── flatlas_infrastructure
```

## 5. Ressourcen- und Übersetzungsstrategie

### Ressourcen
- **Qt Resource System (`.qrc`)**: Icons, Images, Help-XML, Vanilla-Referenzdaten werden in die Binary eingebettet.
- **Zugriff**: `QFile(":/icons/app_icon.png")`, `QPixmap(":/images/splash.png")`
- **Externe Daten**: User-Config, Mod-Daten, Freelancer-Installationen bleiben auf Dateisystem.

### Übersetzungen
- **Qt Linguist Workflow**: `tr("text")` → `lupdate` → `.ts`-Dateien → `lrelease` → `.qm`-Binärdateien
- **Sprachen**: Deutsch (Standard), Englisch
- **Dynamischer Wechsel**: `QTranslator` austauschen → `retranslateUi()` aufrufen
- **Migration**: `translations.json` aus Python wird in `.ts`-Format konvertiert

### Themes
- **QSS-Dateien** pro Theme: `dark.qss`, `light.qss`, `founder.qss`, `xp.qss`
- **QPalette** für systemnahe Farben
- **Dynamischer Wechsel**: `qApp->setStyleSheet(loadTheme(name))`

## 6. Externe Bibliotheken (Vorschläge)

| Bibliothek | Zweck | Begründung |
|------------|-------|------------|
| **Catch2** (Header-Only) | Unit-Testing | Leichtgewichtig, C++-modern, neben Qt Test für Nicht-UI-Logik. |
| **spdlog** (Optional) | Logging | Schnell, formatierbar. Alternative: `qDebug()`/`qWarning()` reicht ggf. aus. |
| **KDDockWidgets** (Optional) | Advanced Docking | Nur falls Qt-Docking nicht ausreicht. LGPL-lizenziert. |

**Bewusst NICHT empfohlen:**
- Boost (zu schwer, Qt bietet alles Nötige)
- Assimp (CMP/3DB sind proprietär, kein Standard-Format)
- Dear ImGui (nicht Qt-konform, anderes Paradigma)
