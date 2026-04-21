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
- Preview-Material-Bindings werden jetzt auch dann pro Ref aufgebaut, wenn ein Modell gar keine expliziten Material-/Texture-Referenzen enthält
- `station_large_a_lod.cmp` hält damit jetzt nicht nur seine `material_*`-Submeshes stabil, sondern auch den erwarteten Binding-Status `no-texture-reference` für diese Meshes
- der Stationsfall zeigt zugleich: für diesen CMP liegt die nächste echte Paritätslücke nicht in der Ref-Auflösung, sondern darin, ein Modell mit tatsächlich vorhandenen Texture-/Material-Referenzen als nächsten Bindungs-Referenzfall auszuwählen

Zugehörige Dateien:
- `tests/test_StationSolarModel.cpp`

### Iteration 5
Referenz:
- `DATA/BASES/GENERIC/ocean_blue.cmp`

Status:
- umgesetzt / erster expliziter Preview-Texture-Binding-Guard ergänzt

Ergebnis:
- erster kleiner Referenzfall mit tatsächlich vorhandenen Texture-Referenzen statt nur `material_*`-IDs aufgenommen
- `ocean_blue.cmp` belegt, dass der Loader Texture-Einträge aus Material-/Texture-Library bis in `PreviewMaterialBinding` durchreicht
- Texture-Candidate-Listen werden dabei jetzt dedupliziert statt denselben Dateinamen mehrfach aus Material- und Texture-Library zu wiederholen
- neuer Regressionstest sichert für `ocean_blue.cmp` zwei explizite Preview-Bindings mit stabiler Candidate-Liste und `first-texture-fallback` ab
- damit ist der nächste Bindungs-Schritt präziser eingegrenzt: nicht mehr "gibt es überhaupt Texture-Referenzen?", sondern "wann reicht V1-nahe Token-/Part-Zuordnung statt Fallback aus?"

Zugehörige Dateien:
- `tests/test_OceanBluePreviewBinding.cpp`

### Iteration 6
Referenz:
- `DATA/BASES/BRETONIA/br_barbican_station_deck.cmp`

Status:
- umgesetzt / Preview-Bindings jetzt part-aware bis in einen zweiten expliziten Texture-Fall abgesichert

Ergebnis:
- `matchedPartNameForRef(...)` löst Preview-Binding-Partnamen jetzt nicht mehr nur über den Ref-Pfad auf, sondern fällt zusätzlich auf Part-`fileName`/`sourceName`/`objectName` gegen `modelName` zurück
- dadurch bleiben Preview-Bindings auch in Base-/Interior-CMPs part-aware, bei denen der Ref-Pfad den Partnamen nicht sauber trägt, der `modelName` aber bereits den korrekten `.3db`-Bezug enthält
- der Binding-Lookup beim Mesh-Aufbau behandelt den Partnamen jetzt als Priorisierung statt als harten Ausschluss; Modell, LOD und Gruppenbereich bleiben die eigentlichen Identitätsanker
- neuer Regressionstest sichert für `br_barbican_station_deck.cmp` fünf explizite Preview-Bindings mit nichtleeren Partnamen und erweitertem `sourceNames`-Kontext ab
- damit ist der nächste Texture-Schritt weiter eingegrenzt: Der Loader verliert den Partkontext auf solchen Base-CMPs nicht mehr, auch wenn die eigentliche Texture-Auswahl dort weiterhin noch auf `first-texture-fallback` landet

Zugehörige Dateien:
- `tests/test_BarbicanStationDeckPreviewBinding.cpp`

### Iteration 7
Referenz:
- `DATA/SHIPS/ORDER/OR_ELITE/or_elite.cmp`

Status:
- umgesetzt / zusätzlicher Schiffs-Guard für direkte Refs und part-aware `no-texture-reference`-Bindings ergänzt

Ergebnis:
- `or_elite.cmp` ergänzt den bisherigen Satz um einen zweiten Ship-Fall, der strukturell deutlich dichter ist als `cv_starflier.cmp`, aber weiterhin ohne explizite Material-/Texture-Referenzen dekodiert
- der neue Regressionstest sichert jetzt 9 Parts, 25 Refs, 3 `VMeshData`-Blöcke, 25 direkte Decode-Pfade und 25 Preview-Bindings ohne Warnings ab
- alle Preview-Bindings bleiben dabei part-aware und tragen stabil `no-texture-reference`, obwohl `materialReferences` leer bleibt
- zusätzlich ist die aktuelle Top-Level-Struktur samt materialtragender Glas-Submeshes fest als Snapshot abgesichert; damit werden ungewollte Struktur- oder Binding-Regressionsfehler im Ship-Pfad früher sichtbar
- `OR_ELITE` ist damit kein nächster Texture-Paritätsfall, aber ein belastbarer Guard für den bereits erreichten direkten Decode- und Partkontext-Stand auf komplexeren Schiffen

Zugehörige Dateien:
- `tests/test_OrderEliteModel.cpp`

### Iteration 8
Referenz:
- `DATA/BASES/GENERIC/ocean_blue.cmp`

Status:
- umgesetzt / erster expliziter `token-match`-Guard im Preview-Binding-Pfad ergänzt

Ergebnis:
- der Preview-Binding-Matcher bewertet jetzt nicht mehr nur vollständige normalisierte Modellnamen, sondern zusätzlich stabile Namensfragmente aus Modell-, Part-, Source- und Texture-Bezeichnern
- bei gleichem Match-Score bleibt die ursprüngliche Texture-Referenzreihenfolge erhalten; dadurch verbessert der neue Matcher die Auswahlqualität, ohne bestehende Prioritäten unnötig umzudrehen
- `ocean_blue.cmp` kippt damit erstmals von `first-texture-fallback` auf `token-match`, während Candidate-Liste und bevorzugte Texture stabil bleiben
- `br_barbican_station_deck.cmp` und `or_elite.cmp` bleiben dabei unverändert grün; der Schritt verbreitert also den Matchraum, ohne den part-aware Fallback- und no-texture-Stand zu destabilisieren
- der verbleibende Binding-Gap ist damit enger: nicht mehr "gibt es überhaupt einen belastbaren `token-match`?", sondern "welcher nächste Base-/Interior-Fall soll nachziehen, obwohl seine Texture-Namen schwächer an den Modell-/Partkontext gekoppelt sind?"

Zugehörige Dateien:
- `src/infrastructure/io/CmpLoader.cpp`
- `tests/test_OceanBluePreviewBinding.cpp`

### Iteration 9
Referenz:
- `DATA/BASES/BRETONIA/br_barbican_station_deck.cmp`

Status:
- umgesetzt / globalen Texture-Library-Fallback als eigenen Preview-Binding-Zustand abgesichert

Ergebnis:
- die Barbican-Diagnose zeigt jetzt explizit, dass dieser Base-/Interior-Fall zwar part-aware Bindings hat, seine Texture-Referenzen aber nur als flache globale `Material library` ohne part- oder node-nahe Zuordnung vorliegen
- der Loader klassifiziert diesen Restfall deshalb nicht mehr unscharf als generischen `first-texture-fallback`, sondern präzise als `global-texture-library-fallback`
- dadurch ist der verbleibende Gap technisch sauber getrennt: `ocean_blue.cmp` belegt jetzt echten `token-match`, während `br_barbican_station_deck.cmp` dokumentiert, dass für manche Base-CMPs zunächst reichhaltigerer Material-/Node-Kontext nötig ist, bevor ein Match überhaupt möglich wird
- der Barbican-Guard hält damit nicht nur den Partkontext stabil, sondern auch die konkrete Ursache fest, warum der aktuelle Matcher dort noch nicht über die globale Texture-Library hinauskommt

Zugehörige Dateien:
- `src/infrastructure/io/CmpLoader.cpp`
- `tests/test_BarbicanStationDeckPreviewBinding.cpp`

## Aktive Referenzmodelle
Siehe auch:
- `docs/3d-viewer-reference-models.md`

Aktuell aktiv:
- `DATA/BASES/BRETONIA/br_barbican_station_deck.cmp`
- `DATA/BASES/GENERIC/ocean_blue.cmp`
- `DATA/SOLAR/DOCKABLE/TLR_lod.3db`
- `DATA/SHIPS/ORDER/OR_ELITE/or_elite.cmp`
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
- auch Modelle ohne explizite Texture-Referenzen behalten jetzt `PreviewMaterialBinding`-Metadaten pro Ref; dadurch bleibt der Binding-Pfad im Debug-/Testzustand sichtbar statt vollständig leer wegzufallen
- `ocean_blue.cmp` sichert jetzt den ersten Fall mit expliziten Texture-Referenzen; doppelte Candidate-Einträge aus Material-/Texture-Library werden dabei vor der Weitergabe an Resolver und Tests dedupliziert
- `ocean_blue.cmp` sichert jetzt zusätzlich den ersten echten `token-match` im Preview-Binding-Pfad; Namensfragmente aus Modell-, Part- und Texture-Kontext reichen hier jetzt für eine belastbare Auswahl
- `br_barbican_station_deck.cmp` sichert jetzt zusätzlich explizit den Zustand `global-texture-library-fallback`; der Fall behält also Partkontext, zeigt aber zugleich, dass die Texture-Referenzen dort nur als globale Library ohne lokale Match-Hinweise vorliegen
- `or_elite.cmp` sichert jetzt zusätzlich einen komplexeren Schiffsfall mit 25 direkten Refs, 25 part-aware Preview-Bindings und stabilen `no-texture-reference`-Snapshots ab

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
- nach dem neuen `no-texture-reference`-Guard gezielt ein Modell mit realen Texture-/Material-Referenzen auswählen; der nächste sinnvolle Restfall ist jetzt nicht mehr ein weiterer ref-seitig sauberer Stationsfall, sondern ein CMP mit tatsächlich belegtem Preview-Binding
- nach dem jetzt abgesicherten `token-match` in `ocean_blue.cmp` den nächsten expliziten Texture-Fall auswählen, in dem der part-aware Kontext ebenfalls von Fallback auf Match kippen kann
- nach der Barbican-Klassifikation als `global-texture-library-fallback` gezielt den ersten Fall angehen, in dem Material-/Texture-Kontext nicht nur global gelistet, sondern wieder node- oder materialnah genug für einen echten Matchaufstieg ist

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
2. `ocean_blue.cmp` als ersten echten `token-match`-Guard behalten und `br_barbican_station_deck.cmp` als expliziten `global-texture-library-fallback`-Gegenfall danebenstehen lassen
3. als Nächstes genau einen schwierigeren Base-/Interior-Fall wählen, dessen Material-/Texture-Kontext mehr als nur eine globale Library bietet, und diesen von Fallback auf Match ziehen

Praktisch:
- vorhandene Snapshot-Tests für `cv_starflier.cmp`, `docking_ringx2_lod.cmp`, `station_large_a_lod.cmp` und `ocean_blue.cmp` als Guard behalten
- `ocean_blue.cmp` als Beleg behalten, dass der aktuelle Matcher nicht nur Candidate-Listen stabilisiert, sondern auch echte `token-match`-Entscheidungen liefern kann
- den Barbican-Guard als Beleg behalten, dass Preview-Bindings ihren Partkontext auch bei Base-/Interior-CMPs nicht verlieren und dass rein globale Texture-Libraries derzeit bewusst als eigener Restfall markiert werden
- die neue Top-Level-CMP-Extraktion als etablierten Pfad betrachten, nicht als Sonderfall
- die Hierarchiebereinigung für leere Top-Level-`Root`-Knoten als etablierten Normalfall betrachten
- die jetzt immer vorhandenen `PreviewMaterialBinding`-Einträge auch bei `materialReferences == 0` als etablierten Debug-/Regression-Pfad betrachten
- deduplizierte Texture-Candidate-Listen aus Material-/Texture-Library als neuen Normalfall betrachten; Mehrfachnennungen derselben Datei sind kein sinnvoller Debugwert mehr
- part-aware Preview-Bindings über Part-`fileName`-/`sourceName`-/`objectName`-Fallback als neuen Normalfall betrachten, nicht nur über Ref-Pfadpräfixe
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
