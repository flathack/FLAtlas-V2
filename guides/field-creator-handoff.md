# Field Creator Handoff

## Ziel

Der Field Creator ist ein eigenes FLAtlas-Tool zum Erstellen von Freelancer-Field-INI-Dateien. Er soll Asteroid-, Mine-, Debris-, Gas- und Nebula-Felder aus strukturierten Daten erzeugen, platzierte Objekte verwalten und die erzeugten Dateien später im 2D System Editor referenzierbar machen.

## Aktueller Stand

- Tool ist als eigener Tab eingebunden:
  - Tools-Menü: `Field Creator`
  - Toolbox-Kachel: `Field Creator`
  - Tool-Key: `fieldCreator`
- Generator ist UI-frei in `src/tools/FieldTemplateGenerator.*`.
- Bestehende Field-INIs können über `Load Template` als Vorlage geladen werden; die Parser-Logik liegt in `FieldTemplateGenerator::parseFieldIni`.
- UI liegt in `src/tools/FieldCreatorPage.*`.
- Speichern schreibt in den aktiven Editing Context:
  - `DATA\SOLAR\ASTEROIDS\<file>.ini`
  - `DATA\SOLAR\NEBULA\<file>.ini`
- Generator-Test existiert:
  - `tests/test_FieldTemplateGenerator.cpp`
- HTML-Mockup liegt unter:
  - `guides/mockups/field-creator-mockup.html`

## Wichtige Dateien

- `src/tools/FieldTemplateGenerator.h`
- `src/tools/FieldTemplateGenerator.cpp`
- `src/tools/FieldCreatorPage.h`
- `src/tools/FieldCreatorPage.cpp`
- `src/tools/CMakeLists.txt`
- `src/ui/MainWindow.cpp`
- `src/ui/ToolboxPage.cpp`
- `src/ui/ToolIcons.cpp`
- `src/ui/SettingsDialog.cpp`
- `tests/test_FieldTemplateGenerator.cpp`
- `resources/languages/de.json`

## Datenmodell

`FieldTemplate` hält die Template-Daten:

- `kind`: Asteroid, Ice, Debris, Mine, Gas, Nebula
- Output-Daten: `fileName`; `zoneNickname` wird intern automatisch aus dem Field-Typ abgeleitet und ist kein sichtbares UI-Feld mehr.
- INI-Parameter: TexturePanels, Billboard Shape, Spacedust, Music, Farben, Cube/Fill/Fog-Werte
- `placedObjects`: Liste platzierter Objekte

`FieldPlacedObject` ist die zentrale Grundlage für manuelle und automatische Platzierung:

- `assetNickname`
- `x`, `y`, `z` im normalisierten Cube-/Feldraum
- `rotateX`, `rotateY`, `rotateZ`
- `mineRole`

Für Asteroid/Mine/Debris/Gas serialisiert das nach `[Cube]`:

```ini
asteroid = minedout_asteroid10, 0.60, 0.20, -0.20, 35, 10, 20
asteroid = mine_spike1, -0.90, 0.00, 0.05, 110, 0, 10, mine
```

## Preview-Stand

Die Preview in `FieldCreatorPage.cpp` ist jetzt zweistufig:

- Mit `FLATLAS_HAS_QT3D`: echte Qt3D-Szene.
- Ohne Qt3D: statische 2D-Fallback-Preview.

Die 3D-Preview liegt im Field Creator in einem eigenen Tab (`3D Preview`), nicht mehr in der engen rechten Editor-Spalte. Der Editor-Tab enthält weiterhin Parameter, Platzierungstabelle, System-Link-Vorschau und generierte Field-INI.

Wichtig: Das Feld soll still stehen. Die Kamera bewegt sich durch das Feld.

Wichtig für die Skalierung: `[Cube]`-Koordinaten wie `0.6, 0.2, -0.2` sind relative Positionen innerhalb eines einzelnen Cubes. Die 3D-Preview skaliert diese Positionen deshalb mit `cube_size`, nicht mit der gesamten Field-/Zone-Größe.

Aktuelle Qt3D-Steuerung:

- `W/A/S/D`: bewegen
- `Space`: hoch
- `Ctrl`: runter
- linke Maustaste ziehen: umsehen
- Mausrad: Geschwindigkeit ändern

Asteroid-/Mine-Assets werden aus `DATA\SOLAR\asteroidarch.ini` über `nickname` -> `DA_archetype` aufgelöst. Die Modelle werden über `ModelCache`/`CmpLoader` geladen und mit `ModelGeometryBuilder` gerendert. Falls kein Modell gefunden wird, wird ein einfacher Fallback-Körper angezeigt.

Modelltexturen werden in der Field-Preview und in der kleinen Asset-Preview unter der Asset-Palette über `FreelancerMaterialResolver::loadTextureForMesh` und `MaterialFactory::createFromImage` geladen. Beide Qt3D-Previews nutzen außerdem `SkyRenderer`, damit derselbe 360-Grad-Star-Hintergrund wie in der 3D-Systemansicht sichtbar ist. Die Asset-Preview ist ein eigener kompakter Fit-Modus mit horizontalem/vertikalem Meter-Lineal, damit die sichtbare Objektgröße besser einschätzbar ist.

Mehrfachauswahl in der Asset-Palette ist bewusst unterstützt: `Add Selected` fügt alle selektierten Assets als einzelne platzierte Objekte hinzu. `Auto Fill` nutzt bevorzugt die bereits in der Platzierungsliste enthaltenen Objekte und verteilt genau diese Anzahl neu; nur wenn die Liste leer ist, fällt es auf die aktuelle Auswahl bzw. die ganze Asset-Palette zurück.

## Bekannte Einschränkungen

- Die echte 3D-Preview nutzt Texturen, wenn `FreelancerMaterialResolver` sie für das Mesh auflösen kann; sonst fällt sie auf Standardmaterialien/Farben zurück.
- Nebula-Clouds sind meist TexturePanel-/Billboard-Shapes, keine `.3db/.cmp`-Modelle. Für Nebula gibt es deshalb noch keine echte Cloud-Billboard-Preview.
- Manual Placement ist formularbasiert. Es gibt noch kein direktes Klicken/Gizmo im 3D-Viewport.
- Platzierte Objekte können aktuell hinzugefügt und per Auto-Fill erzeugt werden, aber nicht komfortabel in der Tabelle editiert/gelöscht werden.
- Geladene bestehende Field-INIs werden auf die vom Creator unterstützten Parameter gemappt. Kommentare und unbekannte Spezialsections werden beim erneuten Speichern noch nicht erhalten.
- Auto-Fill verteilt deterministisch, aber noch ohne echte Freelancer-Cube-Regeln/Collision/Clustering.
- Der 2D System Editor nutzt den Field Creator noch nicht direkt. Er kann aber später die gespeicherten INI-Dateien referenzieren.

## Nächste sinnvolle Schritte

1. Tabelle editierbar machen:
   - Objekte löschen
   - Position/Rotation direkt ändern
   - Auswahl im Viewport mit Tabelle synchronisieren

2. 3D-Interaktion ergänzen:
   - Objekt im Viewport auswählen
   - Objekt mit Gizmo verschieben/rotieren
   - Placement-Modus: Asset auswählen und per Klick platzieren

3. Echte Materialien/Texturen:
   - vorhandene `ModelViewport3D`/`MaterialFactory`-Texturpfade wiederverwenden
   - Asset-Preview-Scale/Lineal visuell gegen echte Freelancer-Modelle prüfen und bei Bedarf feinjustieren

4. Nebula-Preview:
   - TexturePanel-Shapes als Billboard-/Cloud-Planes rendern
   - Fog/Cloud-Dichte im Viewport darstellen
   - BackgroundLightning/DynamicLightning visualisieren

5. Asset-Scan verbessern:
   - `asteroidarch.ini` vollständig klassifizieren
   - Filter für Rock/Ice/Debris/Mine/Gas
   - Suchfeld in Asset-Palette
   - Modellpfad/Quelle im UI zeigen

6. Generator erweitern:
   - `[LootableZone]`
   - `[Exclusion Zones]`
   - `[ExclusionBand]`
   - `[DynamicLightning]`
   - optionale CRLF-Ausgabe, falls bestehende Save-Policy das verlangt

7. 2D System Editor Integration:
   - aus dem Systemeditor Field Creator öffnen
   - erzeugte Template-Datei direkt als `[Asteroids]`/`[Nebula]` verlinken
   - vorhandenen Kopier-Workflow als Legacy/Template-from-existing behalten

## Build/Tests

Zuletzt erfolgreich ausgeführt:

```powershell
$env:PATH='C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;' + $env:PATH
cmake --build build --target FLAtlas test_FieldTemplateGenerator
ctest --test-dir build -R test_FieldTemplateGenerator --output-on-failure
```

## Hinweise für den nächsten Agenten

- Nicht den bestehenden `CreateFieldZoneDialog` ersetzen, solange die 2D-Systemeditor-Integration nicht bewusst geplant ist.
- Kommentare/CRLF-Regeln aus `AGENTS.md` beachten, besonders bei INI-Schreiblogik.
- Lokale fremde Änderungen nicht revertieren.
- Für echte 3D-Weiterarbeit zuerst `ModelViewport3D.cpp` und `SceneView3D.cpp` ansehen; dort gibt es bereits funktionierende Modell- und Kamera-Patterns.
- Die Preview soll explizit kein Orbit-Diorama sein: Feld bleibt statisch, Kamera fliegt durch.
