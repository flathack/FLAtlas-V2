Analysiere mein bestehendes Projekt und plane eine vollständige Migration von Python/PySide6 bzw. PyQt-ähnlicher Architektur auf C++ mit Qt 6.

Projektquelle:
C:\Users\steve\Github\FL-Atlas-Launcher

Ziel:
Erstelle daneben ein neues Projekt in diesem Ordner:

C:\Users\steve\Github\FLAtlas-V2

und richte dort eine neue, saubere C++/Qt-6-Codebasis ein.

Wichtige Grundregeln:
- Das neue Projekt soll KEINE 1:1-Übersetzung des Python-Codes sein.
- Es soll eine vollständige architektonische Neufassung in modernem C++ und Qt 6 werden.
- Behalte die fachliche Funktionalität der bestehenden App bei, aber übernimm keine Python-spezifischen oder architektonisch schlechten Patterns blind.
- Arbeite so, dass am Ende ein gut wartbares, modular aufgebautes Desktop-Projekt entsteht.
- Primäre Zielplattform ist Windows.
- Nutze Qt 6 mit CMake als Buildsystem.
- Verwende moderne C++-Best-Practices, klare Zuständigkeiten, saubere Trennung von UI, Application-Layer, Domain-Logik, Datenmodellen, Rendering und Infrastruktur.
- Nutze Qt-konforme Patterns für Signale/Slots, Models/Views, Ressourcen, Übersetzungen, Threading und UI-Komposition.
- Falls Widgets, Docking, Graphics View, Qt3D oder Qt Quick sinnvoll sind, entscheide bewusst und begründe die Wahl.
- Bevorzuge robuste und pragmatische Architektur statt Overengineering.

Deine Aufgaben:
1. Analysiere zuerst die vorhandene Python-Anwendung im Ordner FL-Atlas-Launcher.
2. Erfasse die bestehende Modulstruktur, Startpunkte, Abhängigkeiten, Kern-Workflows, Datei- und Ressourcenstrukturen.
3. Leite daraus eine sinnvolle Zielarchitektur für C++ + Qt 6 ab.
4. Erstelle danach im neuen Ordner FLAtlas-V2 eine neue Projektstruktur.
5. Erstelle einen vollständigen, priorisierten Migrationsplan für ALLE Module und Teilsysteme.
6. Der Migrationsplan soll so strukturiert sein, dass man ihn Schritt für Schritt abarbeiten kann.
7. Danach beginne mit der Initialisierung der neuen C++/Qt-6-Codebasis im neuen Ordner.
8. Erzeuge das Grundgerüst so, dass spätere Module geordnet eingefügt werden können.

Vorhandene Modullandschaft, die in den Plan aufgenommen werden muss:
1. UI-Framework & Hauptfenster
- MainWindow
- Welcome-Page
- Browser
- Center-Tabs
- Settings-Navigation
- Workspace-Presets
- Sidebar-Layout
- Splash-Screen

2. 2D-Systemkarte
- view_2d
- models
- top_view_icons

3. 3D-View
- view_3d
- Kamera
- Gizmo
- Objekte
- Selektion
- Materialien
- Sky-Rendering

4. Flight-Mode
- Flugsteuerung
- HUD
- Autopilot
- Navigation
- Kamera
- Dust-Particles

5. Native 3D Preview
- CMP/3DB-Loader
- Geometrie-Dekodierung
- Material/Textur
- Qt3D-Szene
- LRU-Cache
- Async-Loading

6. System-Editor
- System-Erstellung
- System-Dialog
- Document-Runtime
- Editor-Persistence
- Infocard-Draft
- Name-Runtime
- Tab-Management

7. Universe-Editor
- Edit-State
- Infocard-Assignment
- Serialisierung
- Snapshot-Sektionen

8. Base-Editor
- Base-Erstellung
- Base-Builder
- Room-Templates
- Room-Editor

9. Trade-Route-Analyse
- Market-Scanning
- Analyse/Scoring
- Custom-Storage
- Market-Editor
- UI-Seite

10. IDS/String-Resourcen
- CSV-Import
- Resource-Runtime
- Toolchain-Erkennung

11. INI-Editor
- File-Context
- Editor-Page
- Section-Writes

12. Mod-Manager
- Conflict-Detection
- Resolution/Aspect-Ratio
- Savegame-Policy
- Workflows
- UI-Page

13. NPC-Editor
- Multiline-Parsing
- MBase-Operations
- Room-Customizations
- Density/Persistence

14. Infocard-Handling
- XML-Validierung
- XML-Helpers
- Row-Navigation

15. DLL/Executable-Handling
- DLL-Resources
- EXE-Version
- Debug-Output

16. Scripting/Patching
- OpenSP-Patch
- Resolution-Patch
- SP-Starter

17. Shortest-Path-Generator
- pathgen

18. News/Rumor-Editor
- News-Parsing
- Rumor-CSV

19. Auto-Updater
- flatlas_updater

20. Help-System
- help_content

21. Jump-Connection-Dialog
- Jump-Object-Logic
- Jump-Dialog

22. Charakter-Vorschau
- character_3d_preview

23. Ressource-Dateien
- Bild-Dateien
- Help-XML
- translations.json
- flvanilla/freelancer.ini

Wichtige Architekturvorgaben:
- Lege eine sinnvolle C++/Qt-6-Projektstruktur an, z. B. mit Bereichen wie app, ui, core, domain, infrastructure, editors, rendering, resources, third_party, tests.
- Nutze CMake sauber und modern mit klaren Targets.
- Trenne fachliche Logik von Qt-UI-Code.
- Nutze Qt-Model/View dort, wo es sinnvoll ist.
- Plane Konfigurations-, Datei-, Parser- und Serialisierungslogik als testbare Nicht-UI-Komponenten.
- Plane 2D- und 3D-Komponenten getrennt, aber mit klaren Schnittstellen.
- Plane Caching, Hintergrundladen, Fehlerbehandlung und Logging strukturiert.
- Plane Übersetzungen, Themes und Ressourcenverwaltung sauber ein.
- Plane den Updater, Mod-Manager und Patcher mit Windows-Fokus.
- Überlege pro Modul, ob QWidget, QGraphicsView, Qt3D, Qt Quick oder eine Mischform sinnvoll ist, und begründe das kurz.
- Wenn ein externer Baustein wie ein Docking-System oder ein spezialisiertes Rendering-Hilfspaket sinnvoll ist, nenne das als Vorschlag mit Begründung.

Erwartete Ausgabe:
A. Bestandsanalyse
- Welche Architektur hat das Python-Projekt aktuell?
- Welche fachlichen Subsysteme gibt es?
- Welche Teile sind UI, Domain, Infrastruktur, Rendering, Parsing, Tools?
- Welche Teile sind kritisch oder risikoreich?

B. Zielarchitektur für C++ + Qt 6
- Empfohlene Projektstruktur
- Empfohlene zentrale Klassen/Subsysteme
- Empfohlene Qt-Technologien je Bereich
- Build-/Ordner-/Target-Struktur
- Ressourcen- und Übersetzungsstrategie

C. Vollständiger Migrationsplan
- Für ALLE oben genannten Module
- In Phasen gegliedert
- Mit Priorität
- Mit Abhängigkeiten
- Mit klaren Deliverables je Phase
- Mit „Definition of Done“ pro Phase
- Mit Risiken, offenen Fragen und technischen Schulden
- Mit Kennzeichnung, was zuerst als Fundament gebaut werden muss

D. Umsetzung im neuen Ordner
- Lege den Ordner FLAtlas-V2 an
- Erzeuge dort das neue C++/Qt-6-Grundgerüst
- Richte CMake, Hauptanwendung, Basisordner, Ressourcenstruktur und ggf. erste Targets ein
- Lege Platzhalter/Scaffolding für die wichtigsten Subsysteme an

E. Abschlussdokumentation
- Zusammenfassung der Zielarchitektur
- Projektbaum
- Build-Anleitung
- Start-Anleitung
- Nächste konkrete Migrationsschritte

Wichtige Arbeitsweise:
- Nichts im alten Projekt löschen oder überschreiben.
- Nur im neuen Ordner FLAtlas-V2 arbeiten.
- Vor größeren Architekturentscheidungen kurz begründen, warum sie in Qt 6/C++ sinnvoll sind.
- Falls Informationen im Python-Projekt fehlen oder unklar sind, zuerst analysieren und dann eine begründete Annahme treffen.
- Lieber eine starke, realistisch wartbare Architektur als ein vorschnelles Komplettgerüst ohne Richtung.
- Erstelle den Migrationsplan so, dass man ihn direkt als Abarbeitungsliste für die nächsten Wochen/Monate verwenden kann.

Zusätzliche Qualitätsprüfung vor Abschluss:
- Prüfe, ob die Zielarchitektur wirklich zu einer großen Windows-Desktop-App mit vielen Editoren, 2D/3D-Ansichten und Ressourcentools passt.
- Prüfe, ob UI und Logik sauber getrennt sind.
- Prüfe, ob die Projektstruktur nicht unnötig kompliziert ist.
- Prüfe, ob die wichtigsten Risiken der Migration sichtbar gemacht wurden.
- Prüfe, ob der Migrationsplan vollständig, priorisiert und realistisch umsetzbar ist.
- Prüfe, ob die neue C++/Qt-6-Struktur langfristig wartbar ist.

Wichtig:
Beginne NICHT sofort mit einer vollständigen Implementierung aller Module.
Erstelle zuerst die Analyse, dann die Zielarchitektur und dann den detaillierten Migrationsplan.
Initialisiere anschließend nur das tragfähige Grundgerüst für FLAtlas-V2.
Mit der eigentlichen Modulmigration soll erst begonnen werden, wenn die Architektur und Reihenfolge nachvollziehbar festgelegt sind.