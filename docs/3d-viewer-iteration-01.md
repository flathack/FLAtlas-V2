# 3D Viewer V1 Port - Iteration 01

## Referenzmodell
- `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`
- Typ: kleines Schiffs-`.cmp`

## V1-Sollbild
- kleiner zentraler Rumpf
- klar erkennbare seitliche Hauptteile
- mehrere sichtbare Part-Knoten statt eines einzigen rohen Fallback-Meshes
- plausible Bounds und Startausrichtung

## Beobachteter V2-Ausgangszustand
- Part-Hierarchie war sichtbar
- aber kein einziges Mesh hing an den Parts
- `VMeshRef`-Einträge blieben für das Modell vollständig unresolved

## Umgesetzter Decoder-Fix
- transform-only CMP-Parents bleiben jetzt in der Hierarchie erhalten
- `VMeshRef`-Matching nutzt jetzt zusätzlich:
  - `objectName`
  - erweiterten `VMeshData`-Lookup im echten `extractPart(...)`-Pfad
- `VMeshData`-Auflösung wurde für diesen V1-nahen Fall erweitert:
  - Top-Level-UTF-Pfad
  - `LevelN -> lodN`-Zuordnung

## Aktiver Decoderpfad
- `cv_starflier.cmp`
  - echte CMP-Part-Hierarchie
  - `VMeshRef`-Zuordnung über `Level -> lod`
  - Mesh-Build über den normalen `extractPart(...)`-Pfad
- `TLR_lod.3db`
  - weiterhin `Used direct VMeshData fallback decode.`

## Sichtbare Verbesserung
- das kleine Referenz-`.cmp` liefert jetzt echte Meshes pro Part
- der neue Regressionstest zeigt:
  - Meshes an `Part_port_wing_lod1`
  - Meshes an `Part_star_wing_lod1`
  - Meshes an `Part_engine_lod1`
  - Meshes an `Part_glass_lod1`
  - Meshes an beiden Baydoors

## Noch offene Abweichung
- visuelle Parität zu V1 ist noch nicht erreicht
- der Viewer zeigt jetzt Geometrie, aber wir haben noch keinen direkten V1-Bildvergleich für:
  - genaue Formtreue
  - eventuelle falsche Group-/Window-Auswahl
  - Bounds-/Startansicht-Feinschliff

## Validierung
- `test_SimpleShipModel` grün
- `test_CmpLoader` grün
- `test_TradeLaneModel` grün
- `FLAtlas.exe` baut sauber

## Nächster Block
- Iteration 2:
  - `structured-family` / Header-Stream weiter an V1 angleichen
  - zweites family-lastiges Referenzmodell hinzufügen
  - gruppen- und ref-bezogene Layout-Auswahl weiter verfeinern
