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
- umgesetzt

Ergebnis:
- erstes kleines `.cmp` als Leitmodell etabliert
- echte Part-Geometrie hängt an mehreren Schiffsteilen
- Test vorhanden

Zugehörige Dateien:
- `docs/3d-viewer-iteration-01.md`
- `tests/test_SimpleShipModel.cpp`

### Iteration 2
Referenz:
- `DATA/SOLAR/MISC/space_dome.cmp`

Status:
- umgesetzt / Basis steht

Ergebnis:
- zweiter Family-/Solar-Referenzfall etabliert
- `source_name`-aware Blockauflösung näher an V1
- zusätzliche Family-/Header-/Stream-Arbeit im Decoder

Zugehörige Dateien:
- `docs/3d-viewer-iteration-02.md`
- `tests/test_FamilySolarModel.cpp`

### Iteration 3
Referenz:
- `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`

Status:
- in Arbeit, aber deutlich weiter stabilisiert

Ergebnis:
- härterer Dockable-/Solar-Fall etabliert
- Family-Planwahl deutlich näher an V1
- `decode_ready`-Logik eingebaut
- Planwahl jetzt an Mesh und Ref sichtbar
- echter V1-Freelancer-CRC für `VMeshRef -> VMeshData`-Auflösung in V2 nachgezogen
- V1-näherer Part-Metadaten-Scan (`Root` + Follower-Suche) im CMP-Loader ergänzt
- `docking_ringx2_lod.cmp` dekodiert wieder mit Mesh-Ausgabe statt unresolved refs
- Repro-/Snapshot-Test für die konkrete Ref-Auflösung ergänzt

Zugehörige Dateien:
- `docs/3d-viewer-iteration-03.md`
- `tests/test_DockableSolarModel.cpp`

## Aktive Referenzmodelle
Siehe auch:
- `docs/3d-viewer-reference-models.md`

Aktuell aktiv:
- `DATA/SOLAR/DOCKABLE/TLR_lod.3db`
- `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`
- `DATA/SOLAR/MISC/space_dome.cmp`
- `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`

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
- aktiven Ref-Plan noch gezielter gegen V1 vergleichen
- prüfen, ob `docking_ringx2_lod.cmp` jetzt vollständig über direkten Header-Decode läuft oder ob ein echter Family-Fallback-Fall als nächster Referenzfall sinnvoller ist
- nächste echte Abweichung an einem einzelnen Dockable-/Solar-Ref isolieren

### Mittelfristig
- Family-/Header-/Stream-Fälle weiter an V1 schließen
- komplexeren Solar-/Dockable-/Station-Fall als nächste Referenz aufnehmen
- erst danach wieder stärker auf sichtbare Viewer-Parität schauen

### Noch bewusst zurückgestellt
- MAT-Semantik
- vollständige Texture-Parität
- Viewer-Feinschliff jenseits von Debug-/Kamerathemen

## Empfohlener nächster Arbeitsschritt
Der sinnvollste nächste Schritt ist:

1. `docking_ringx2_lod.cmp` als aktiven Problemfall behalten
2. pro `VMeshRef` den gewählten Plan mit V1 vergleichen
3. den nächsten Fehlfall nicht global, sondern an genau einem Ref schließen

Praktisch:
- vorhandenen Snapshot-Test für `docking_ringx2_lod.cmp` als Guard behalten
- nächsten Problemfall wieder auf genau einen Ref/Block eingrenzen
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
