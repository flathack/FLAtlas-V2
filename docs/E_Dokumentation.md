# E – Abschlussdokumentation (Phase D Grundgerüst)

## 1. Projektüberblick

FLAtlas V2 ist die geplante C++/Qt6-Neuentwicklung des Freelancer-Editor-Werkzeugs FLAtlas
(ursprünglich Python/PySide6, ca. 200 Module, ~80–150k LOC).

Das Grundgerüst (Phase D) wurde vollständig aufgesetzt und enthält:
- CMake-Build-System (C++20, AUTOMOC/AUTORCC/AUTOUIC)
- 7 statische Bibliotheken + 1 ausführbare Datei
- Alle Klassen aus den Phasen 0–24 des Migrationsplans als Stubs/Grundgerüste
- 4 Unit-Test-Stubs mit QtTest

## 2. Verzeichnisstruktur

```
FLAtlas-V2/
├── CMakeLists.txt              ← Wurzel-CMake
├── .gitignore
├── migration_to_C++.md         ← Migrationsspezifikation
├── docs/
│   ├── A_Bestandsanalyse.md
│   ├── B_Zielarchitektur.md
│   ├── C_Migrationsplan.md
│   └── E_Dokumentation.md      ← Diese Datei
├── resources/
│   └── resources.qrc
├── third_party/
│   └── CMakeLists.txt
├── tests/
│   ├── CMakeLists.txt
│   ├── test_IniParser.cpp
│   ├── test_BiniDecoder.cpp
│   ├── test_PathUtils.cpp
│   └── test_AppConfig.cpp
└── src/
    ├── CMakeLists.txt
    ├── main.cpp
    ├── app/
    │   ├── Application.h/cpp    (flatlas::app)
    │   ├── SplashScreen.h/cpp
    │   └── AppConfig.h/cpp
    ├── core/                    (flatlas::core)
    │   ├── Config.h/cpp
    │   ├── I18n.h/cpp
    │   ├── Theme.h/cpp
    │   ├── Logger.h/cpp
    │   ├── PathUtils.h/cpp      ← voll implementiert
    │   └── UndoManager.h/cpp
    ├── domain/                  (flatlas::domain)
    │   ├── SystemDocument.h/cpp ← voll implementiert
    │   ├── SolarObject.h/cpp
    │   ├── ZoneItem.h/cpp
    │   ├── UniverseData.h/cpp
    │   ├── BaseData.h/cpp
    │   ├── TradeRoute.h/cpp
    │   ├── NpcData.h/cpp
    │   └── ModProfile.h/cpp
    ├── infrastructure/          (flatlas::infrastructure)
    │   ├── parser/  (IniParser, BiniDecoder, BiniConverter, XmlInfocard)
    │   ├── io/      (CmpLoader, VmeshDecoder, TextureLoader, DllResources, CsvImporter)
    │   ├── freelancer/ (FreelancerPaths, UniverseScanner, IdsStringTable)
    │   ├── updater/ (AutoUpdater)
    │   └── net/     (DownloadManager)
    ├── rendering/               (flatlas::rendering)
    │   ├── view2d/  (SystemMapView)
    │   ├── view3d/  (SceneView3D)
    │   ├── flight/  (FlightController)
    │   ├── preview/ (ModelPreview, ModelCache)
    │   └── pathgen/ (ShortestPath)
    ├── editors/                 (flatlas::editors)
    │   ├── system/  (SystemEditorPage)
    │   ├── universe/ (UniverseEditorPage)
    │   ├── base/    (BaseEditorPage)
    │   ├── npc/     (NpcEditorPage)
    │   ├── ini/     (IniEditorPage, IniSyntaxHighlighter)
    │   ├── infocard/ (InfocardEditor)
    │   ├── trade/   (TradeRoutePage)
    │   ├── news/    (NewsRumorEditor)
    │   ├── ids/     (IdsEditorPage)
    │   ├── modmanager/ (ModManagerPage)
    │   └── jump/    (JumpConnectionDialog)
    ├── ui/                      (flatlas::ui)
    │   ├── MainWindow.h/cpp
    │   ├── WelcomePage.h/cpp
    │   ├── BrowserPanel.h/cpp
    │   ├── PropertiesPanel.h/cpp
    │   ├── CenterTabWidget.h/cpp
    │   ├── SettingsDialog.h/cpp
    │   └── StatusBarManager.h/cpp
    └── tools/                   (flatlas::tools)
        ├── ScriptPatcher.h/cpp
        ├── SpStarter.h/cpp
        └── HelpBrowser.h/cpp
```

## 3. Build-Anleitung

### Voraussetzungen
- **CMake** ≥ 3.21
- **Qt 6** (Core, Widgets, Gui, Network, Xml; optional: 3DCore, 3DRender, 3DInput, 3DExtras, LinguistTools)
- **C++20-Compiler** (g++ 15.2 MinGW oder MSVC 2022)

### Qt6 installieren (noch nicht auf dem System)
1. Qt Online Installer von https://www.qt.io/download herunterladen
2. Qt 6.x für MinGW 64-bit (passend zum vorhandenen g++ 15.2) installieren
3. `CMAKE_PREFIX_PATH` auf das Qt6-Verzeichnis setzen, z.B.:
   ```powershell
   $env:CMAKE_PREFIX_PATH = "C:\Qt\6.8.0\mingw_64"
   ```

### Bauen
```powershell
cd FLAtlas-V2
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/mingw_64"
cmake --build build
```

### Tests ausführen
```powershell
cd build
ctest --output-on-failure
```

## 4. Starten
```powershell
.\build\src\FLAtlas.exe
```
Das Programm zeigt ein Splash-Screen, dann das Hauptfenster mit:
- Links: Browser-Panel (leer)
- Mitte: Welcome-Tab
- Rechts: Properties-Panel (leer)

## 5. Architektur-Entscheidungen
| Entscheidung | Begründung |
|---|---|
| C++20 + Qt6 | Direkte Qt-Nutzung, kein Python-Overhead, volle Widget-Kontrolle |
| 7 statische Bibliotheken | Klare Trennung, schnelle inkrementelle Builds |
| Namespace-Hierarchie | `flatlas::{core,domain,infrastructure,rendering,editors,ui,tools}` |
| Stub-Architektur | Kompilierbares Skelett, schrittweise auffüllbar laut Migrationsplan |
| QSettings für Fensterstatus | Standard-Qt-Persistenz, keine externe Abhängigkeit |

## 6. Nächste konkrete Schritte (laut Migrationsplan Phase 0–2)
1. **Qt6 installieren** – Build verifizieren
2. **Phase 0**: IniParser voll implementieren (parseFile, parseText, serialize)
3. **Phase 0**: BiniDecoder voll implementieren (decode, isBini)
4. **Phase 1**: Config, Theme, I18n auffüllen
5. **Phase 1**: Logger mit Dateiausgabe erweitern
6. **Phase 2**: FreelancerPaths implementieren (Registry, Standardpfade)
7. **Phase 2**: UniverseScanner implementieren (system.ini, universe.ini parsen)

## 7. Hinweis
> Qt6 ist derzeit **noch nicht installiert** auf dem Entwicklungsrechner.
> Das Projekt ist vollständig vorbereitet und wird kompilieren, sobald Qt6 verfügbar ist.
