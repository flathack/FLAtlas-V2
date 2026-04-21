# 3D Viewer V1 Port - Iteration 02

## Referenzmodelle
- `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`
- `DATA/SOLAR/MISC/space_dome.cmp`

## Ziel
- einen zweiten family-lastigen `.cmp`-Fall im Solar-Bereich absichern
- den `VMeshData`-Resolver näher an den V1-Ansatz `source_names -> single-source-match` bringen

## Umgesetzter Decoder-Schritt
- `parseVMeshRefs(...)` bekommt jetzt Zugriff auf die Parts
- aus `VMeshRef.modelName` und den Parts werden echte `sourceName`-Kandidaten gebildet
- wenn genau ein `VMeshData`-Block zu diesem `sourceName` passt, wird dieser bevorzugt
- danach greifen weiterhin die bereits vorhandenen Fallbacks:
  - direkte Index-Auflösung
  - CRC-/Namensvarianten
  - Top-Level-/`Level -> lod`-Fallback

## Warum das V1-näher ist
In V1 läuft die Blockauflösung nicht nur über rohe Referenzwerte, sondern zusätzlich über aus dem Modellkontext abgeleitete `source_names`. Genau dieser Zwischenschritt ist jetzt auch in V2 vorhanden.

## Ergebnis
- `space_dome.cmp` ist jetzt als zweiter Referenzfall im Testpfad verankert
- `test_FamilySolarModel` läuft grün
- `space_dome.cmp` liefert:
  - 2 Parts
  - 5 `VMeshRef`s
  - 3 `VMeshData`-Blöcke
  - Geometrie an `Part_dome_lod1`

## Noch offen
- weitere harte `structured-family`-/Header-Stream-Fälle aus `DATA/SOLAR/DOCKABLE`
- expliziter V1-Vergleich für einen problematischeren multi-block-CMP-Fall

## Nächster Block
- einen komplexeren Solar-/Dockable-CMP-Fall dazunehmen
- gruppen- und ref-bezogene Layout-Auswahl weiter an V1 angleichen
- besonders bei `family-split-header-stream` die Priorität zwischen Header- und Stream-Quelle weiter schärfen
