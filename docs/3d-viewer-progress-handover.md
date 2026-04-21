# 3D Viewer Progress Handover

## Ziel dieses Dokuments
Diese Datei fasst den aktuellen Umsetzungsstand der V1-3D-Portierung in `FLAtlas-V2` so zusammen, dass ein anderes KI-Modell oder ein anderer Entwickler direkt weiterarbeiten kann.

Fokus bleibt:
- decoder first
- ein Referenzmodell pro Iteration
- erst Geometrie / CMP / 3DB korrekt
- MAT / Texture weiterhin nachrangig

## Gesamtstatus
- Gesamtfortschritt am iterativen Decoder-Plan: ca. `50-60 %`
- Fortschritt an der reinen Decoder-Infrastruktur: ca. `65-70 %`
- Fortschritt bis zu sichtbar V1-naher Darstellung mehrerer Modelle: ca. `30-40 %`

Wichtig:
- Viewer-Stabilität ist deutlich besser als am Anfang
- Decoder-Struktur ist inzwischen tragfähig
- visuelle V1-Parität bei komplexen `.cmp`-Fällen ist noch nicht erreicht

## Bereits umgesetzte Iterationen

### Iteration 1
Referenz:
- `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`

Status:
- umgesetzt / Regression jetzt auf Part- und Materialsignaturen abgesichert

Ergebnis:
- erstes kleines `.cmp` als Leitmodell etabliert
- echte Part-Geometrie hängt an mehreren Schiffsteilen
- Test vorhanden
- Binding-/Material-Matching bekommt jetzt zusätzlich Part-`file_name`-/`source_name`-Tokens, näher an V1
- `cv_starflier.cmp` hat jetzt einen festen Snapshot für Part-Hierarchie und materialisierte Mesh-Signaturen
- ungebundene Top-Level-Refs werden jetzt zusätzlich als synthetische CMP-Knoten über denselben Decoderpfad extrahiert; dadurch hängt auch der `cv_starflier_lod...`-Hull nicht mehr ungenutzt am Root
- der Ship-Signaturtest vergleicht diese Part-/Materialsignaturen jetzt bewusst reihenfolgeunabhängig, damit nur echte Decoder-Änderungen triggern

Zugehörige Dateien:
- `docs/3d-viewer-iteration-01.md`
- `tests/test_SimpleShipModel.cpp`

### Iteration 2
Referenz:
- `DATA/SOLAR/MISC/space_dome.cmp`

Status:
- umgesetzt / Regression jetzt deutlich schärfer abgesichert

Ergebnis:
- zweiter Family-/Solar-Referenzfall etabliert
- `source_name`-aware Blockauflösung näher an V1
- zusätzliche Family-/Header-/Stream-Arbeit im Decoder
- `space_dome.cmp` hat jetzt einen festen Ref-/Mesh-Snapshot für den aktuellen Decoderpfad
- fehlerhafte source-name-Priorisierung für Multi-LOD-Refs beseitigt: `space_dome.cmp` Level1 läuft jetzt direkt auf `lod1`
- der frühere `mesh-variant-fallback` in `space_dome.cmp` ist damit weggefallen

Zugehörige Dateien:
- `docs/3d-viewer-iteration-02.md`
- `tests/test_FamilySolarModel.cpp`

### Iteration 3
Referenz:
- `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`

Status:
- in Arbeit, aber erneut deutlich weiter stabilisiert

Ergebnis:
- härterer Dockable-/Solar-Fall etabliert
- Family-Planwahl deutlich näher an V1
- `decode_ready`-Logik eingebaut
- Planwahl jetzt an Mesh und Ref sichtbar
- echter V1-Freelancer-CRC für `VMeshRef -> VMeshData`-Auflösung in V2 nachgezogen
- V1-näherer Part-Metadaten-Scan (`Root` + Follower-Suche) im CMP-Loader ergänzt
- `docking_ringx2_lod.cmp` dekodiert wieder mit Mesh-Ausgabe statt unresolved refs
- Repro-/Snapshot-Test für die konkrete Ref-Auflösung ergänzt
- direkte Decode-Pfade (`structured-header` / `structured-single-block`) werden jetzt ebenfalls als `debugHint` sichtbar
- ungebundene Top-Level-Refs werden jetzt als synthetische CMP-Knoten extrahiert statt ungenutzt liegenzubleiben
- aktueller Dockable-Snapshot zeigt damit: alle 14 Refs von `docking_ringx2_lod.cmp` dekodieren jetzt direkt über `structured-single-block@0`

Zugehörige Dateien:
- `docs/3d-viewer-iteration-03.md`
- `tests/test_DockableSolarModel.cpp`

### Iteration 4
Referenz:
- `DATA/SOLAR/DOCKABLE/station_large_a_lod.cmp`

Status:
- umgesetzt / Regression für komplexeren Stations- und Materialpfad ergänzt

Ergebnis:
- zusätzlicher größerer Stations-/Dockable-Fall als Guard aufgenommen
- `station_large_a_lod.cmp` bestätigt aktuell 20/20 direkte Refs ohne `structured-family`-Fallback
- der Loader entfernt jetzt leere Top-Level-`Root`-Knoten, wenn sie weder Meshes noch Kinder tragen
- dadurch ist die CMP-Hierarchie näher an der tatsächlich relevanten Part-Struktur und weniger mit technischen Leer-Knoten verrauscht
- neuer Regressionstest sichert für `station_large_a_lod.cmp` direkten Decode-Pfad, Mesh-Anzahl und Top-Level-Kindsignatur ab
- derselbe Guard deckt jetzt zusätzlich die materialtragenden Submeshes des Main-/Docking-Bereichs über feste Materialsignaturen ab

Zugehörige Dateien:
- `tests/test_StationSolarModel.cpp`

## Aktive Referenzmodelle
Siehe auch:
- `docs/3d-viewer-reference-models.md`

Aktuell aktiv:
- `DATA/SOLAR/DOCKABLE/TLR_lod.3db`
- `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`
- `DATA/SOLAR/MISC/space_dome.cmp`
- `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`
- `DATA/SOLAR/DOCKABLE/station_large_a_lod.cmp`

## Wichtige Decoder-Arbeit, die bereits im Code steckt

### Allgemein
Zentrale Datei:
- `src/infrastructure/io/CmpLoader.cpp`

Wichtige Bereiche:
- UTF-/Node-/Part-Parsing
- `VMeshData`-Blockerkennung
- `VMeshRef`-Auflösung
- CMP-Hierarchie / Transform-Hints
- Family-Decode

### Bereits vorhandene Decoder-Bausteine
- `LevelN -> lodN`-Auflösung
- `objectName`- / Part-bezogenes Matching
- source-name-aware `VMeshData`-Suche
- strukturierte `VMeshData`-Header-Erkennung
- `structured-single-block`
- `structured-family`
- Header-/Stream-Split-Fallback
- V1-nahe Triangle-Soup-Normalisierung
- CMP-Transform-Pfad mit:
  - `Fix`
  - `Rev`
  - `Pris`
- Ref-/Mesh-Debugpfade für Family-Fallback

### Aktueller Family-Decode-Stand
Der aktuelle Family-Pfad macht jetzt:
- Family-Vorfilterung über `source_name` / `familyKey`
- Header-/Stream-Bewertung über:
  - `groupStart/groupCount`
  - `source_vertex_end`
  - `source_index_end`
- Semantik-Klassifikation:
  - `mesh-header-end-ranges-and-group-match-source`
  - `mesh-header-end-ranges-match-source`
  - schwächere Matches
- explizite Planwahl:
  - bevorzugter Split-Plan
  - bevorzugter Single-Block-Plan
- `decode_ready`-Verhalten:
  - gute Pläne zuerst exklusiv
  - erst danach schwächere Fallbacks

### Debug-/Trace-Stand
Aktuell sichtbar:
- am Mesh:
  - `MeshData.debugHint`
- am Ref:
  - `VMeshRefRecord.debugHint`
  - `VMeshRefRecord.usedStructuredFamilyFallback`

Das ist bewusst eingebaut worden, damit der aktive Decoderpfad pro Ref gegen V1 verglichen werden kann.

Neu abgesichert:
- `space_dome.cmp` zeigt jetzt explizit, dass alle 5 Refs direkt über `structured-single-block@0` auf den passenden LOD-Blöcken dekodieren
- `station_large_a_lod.cmp` sichert jetzt zusätzlich einen größeren Stationsfall mit 20 direkten Refs und bereinigter Top-Level-Hierarchie ab
- `station_large_a_lod.cmp` sichert jetzt auch materialtragende Mesh-Gruppen (`material_*`) im Main-/Dock4-Bereich gegen unbeabsichtigte Material-/Submesh-Regressionsfehler ab

## Relevante Dateien für die Fortsetzung

### Decoder / Loader
- `src/infrastructure/io/CmpLoader.h`
- `src/infrastructure/io/CmpLoader.cpp`
- `src/infrastructure/io/VmeshDecoder.h`
- `src/infrastructure/io/VmeshDecoder.cpp`

### Viewer-Anbindung
- `src/rendering/preview/ModelCache.cpp`
- `src/rendering/view3d/ModelViewport3D.cpp`
- `src/rendering/view3d/ModelGeometryBuilder.cpp`

### Referenz / Tests / Doku
- `docs/3d-viewer-reference-models.md`
- `docs/3d-viewer-v1-parity-plan.md`
- `docs/3d-viewer-iteration-01.md`
- `docs/3d-viewer-iteration-02.md`
- `docs/3d-viewer-iteration-03.md`
- `tests/test_SimpleShipModel.cpp`
- `tests/test_FamilySolarModel.cpp`
- `tests/test_DockableSolarModel.cpp`
- `tests/test_TradeLaneModel.cpp`

### V1-Referenzen
- `../FLAtlas/fl_editor/cmp_loader.py`
- `../FLAtlas/fl_editor/cmp_orientation_debug.py`

## Was noch offen ist

### Kurzfristig
- aktiven Ref-Plan des nächsten komplexeren `.cmp`-Falls gezielter gegen V1 vergleichen
- nach `docking_ringx2_lod.cmp` jetzt den nächsten echten Family-/Station-/Dockable-Fall auswählen, der noch nicht vollständig auf direkten Decode-Pfaden läuft
- nächste echte Abweichung wieder an genau einem Ref isolieren
- auf Basis des jetzt sauberen `TLR_lod.3db`-Pfads den nächsten komplexeren `.cmp`- oder Dockable-Fall auswählen
- Material-/Submesh-Gruppierung bei einem Modell mit klaren sichtbaren Materialwechseln als nächsten echten Restfall priorisieren, da die bisher getesteten komplexeren Stationen/Dockables direkt laufen

### Mittelfristig
- Family-/Header-/Stream-Fälle weiter an V1 schließen
- komplexeren Solar-/Dockable-/Station-Fall als nächste Referenz aufnehmen
- erst danach wieder stärker auf sichtbare Viewer-Parität schauen

### Noch bewusst zurückgestellt
- MAT-Semantik
- vollständige Texture-Parität
- Viewer-Feinschliff jenseits von Debug-/Kamerathemen

## Empfohlener nächster Arbeitsschritt
Der sinnvollste nächste Schritt ist jetzt:

1. `docking_ringx2_lod.cmp` als Guard behalten, aber nicht mehr als Hauptproblemfall
2. den nächsten komplexeren Family-/Dockable-/Station-Fall auswählen, der noch nicht vollständig direkt dekodiert oder materialseitig sichtbar abweicht
3. dort wieder genau einen Ref/Block gegen V1 schließen

Praktisch:
- vorhandene Snapshot-Tests für `cv_starflier.cmp`, `docking_ringx2_lod.cmp` und `station_large_a_lod.cmp` als Guard behalten
- die neue Top-Level-CMP-Extraktion als etablierten Pfad betrachten, nicht als Sonderfall
- die Hierarchiebereinigung für leere Top-Level-`Root`-Knoten als etablierten Normalfall betrachten
- den nächsten Problemfall wieder auf genau einen Ref/Block eingrenzen
- gegen V1 `cmp_loader.py` / `preview_family_decode_hints` / `structured_decode_plans` abgleichen

## Bekannte Build-/Tooling-Fallen
Auf dieser Maschine gibt es wiederholt MinGW-/Archivierungsprobleme, die **nicht** vom Decoder-Code selbst kommen.

Typische Symptome:
- `file truncated`
- `no more archived files`
- `illegal relocation type`
- `No rule to make target ... libflatlas_rendering.a`

Das ist bisher mehrfach als Build-Artefaktproblem aufgetreten.

### Bewährter Workaround
Wenn der Build auf Archiv-/Relocation-Fehler läuft:

1. betroffene `.a` und problematische `.obj` löschen
2. getrennt bauen:
   - `build/src` für `FLAtlas`
   - `build/tests` für einzelne Tests
3. eher `-j 1` verwenden

Bewährte Kommandos:

```powershell
cmake --build C:\Users\steve\Github\FLAtlas-V2\build\src --target FLAtlas -j 1
cmake --build C:\Users\steve\Github\FLAtlas-V2\build\tests --target test_DockableSolarModel -j 1
```

Nicht ideal, aber aktuell der stabilste Weg.

## Letzter bestätigter Build-Stand
Zuletzt erfolgreich gebaut:
- `FLAtlas.exe`
- `test_DockableSolarModel`

Zuletzt zusätzlich erfolgreich verifiziert:
- `test_FamilySolarModel`
- `test_SimpleShipModel`
- `test_TradeLaneModel`

Aktueller `.3db`-Stand:
- `TLR_lod.3db` verwendet keinen rohen `direct VMeshData fallback` mehr
- der Loader extrahiert jetzt den Top-Level-UTF-Pfad aus `VMeshRef`/`VMeshData`, wenn kein CMP-Part vorhanden ist
- Regression kann dadurch feste Mesh-Signaturen auf dem normalen Decoderpfad prüfen

Aktueller Family-Solar-Stand:
- `space_dome.cmp` hat jetzt einen festen Ref-/Mesh-Snapshot statt nur eines groben Mesh-Existenz-Tests
- die frühere Fehlauflösung eines Level1-Refs auf `lod0` ist behoben; `space_dome.cmp` läuft jetzt vollständig direkt auf den passenden LOD-Blöcken

Zuletzt gemeinsam erfolgreich verifiziert:
- `test_SimpleShipModel`
- `test_FamilySolarModel`
- `test_DockableSolarModel`
- `test_TradeLaneModel`

Aktueller Ship-Stand:
- `cv_starflier.cmp` ist jetzt nicht mehr nur über Mesh-Existenz abgesichert, sondern über feste Part- und Material-Signaturen
- die Binding-Schicht nutzt dafür V1-näher zusätzliche Part-Metadaten (`file_name` / `source_name`) als Match-Tokens
- der bislang ungebundene Top-Level-Hull (`cv_starflier_lod...`) wird jetzt ebenfalls als synthetischer CMP-Knoten dekodiert und im Signaturtest mit abgesichert

Aktueller Dockable-Stand:
- `docking_ringx2_lod.cmp` hat keinen ungenutzten Root-Ref-Pfad mehr; auch die früheren `Dock_lod...`-Refs laufen jetzt direkt über den normalen Decoderpfad
- der Dockable-Snapshot erwartet damit jetzt 14 direkte Refs statt 12

Aktueller Station-Stand:
- `station_large_a_lod.cmp` läuft aktuell ohne Warnings und ohne `structured-family`-Fallback durch
- die zuvor technisch leeren Top-Level-`Root`-Knoten werden nicht mehr als leere Kinder im Modellbaum belassen
- der neue Stations-Regressionstest sichert deshalb die bereinigte Top-Level-Kindstruktur zusätzlich zur direkten Ref-Dekodierung ab
- zusätzlich ist jetzt ein fester Snapshot der materialtragenden Submeshes hinterlegt, um den nächsten Material-/Submesh-Block nicht nur visuell, sondern decoderseitig abzusichern

Zusätzlich wurden in früheren Runden erfolgreich gebaut:
- `test_FamilySolarModel`
- `test_SimpleShipModel`
- `test_TradeLaneModel`

## Wichtige inhaltliche Leitlinie für die Fortsetzung
Nicht wieder in allgemeinen Viewer-Feinschliff abgleiten.

Weiterhin gilt:
- Decoder first
- Referenzmodell pro Schritt
- V1-Pfad funktional nachziehen
- MAT / Texture weiterhin hinten anstellen
