# FLAtlas V2 3D Reference Models

## Ziel
Diese Referenzmodelle dienen als feste Vergleichsbasis für die V1-Paritätsarbeit im 3D-Viewer. Sie sind bewusst klein gehalten, damit Decoder-, Transform- und Materialänderungen schnell an echten Freelancer-Dateien überprüft werden können.

## Aktiver Kontext
- Erwarteter Testkontext: `TESTMOD1`

## Referenzmodelle

### 1. Trade Lane Ring
- Archetype: `Trade_Lane_Ring`
- Model: `DATA/SOLAR/DOCKABLE/TLR_lod.3db`
- Typ: `.3db`
- Zweck:
  - einfacher, stabiler Geometry-First-Repro
  - Basistest für Viewer-Stabilität
  - Basistest für Bounds, Kamera-Fit und Decoder-Konsistenz

### 2. Einfaches Schiff
- Iteration-1-Referenz: `DATA/SHIPS/CIVILIAN/CV_STARFLIER/cv_starflier.cmp`
- Archetype: `cv_starflier`
- Typ: `.cmp`
- Zweck:
  - einfache CMP-Part-Struktur
  - Vergleich von Part-Auflösung und Startansicht
  - erster gezielter V1-/V2-Vergleich für ein kleines Schiff
- Erwartung nach V1-Beobachtung:
  - kleiner zentraler Rumpf
  - klar erkennbare seitliche Hauptteile statt zerrissener Triangle-Suppe
  - mehrere sichtbare Teilknoten statt eines einzigen rohen Fallback-Meshes
  - plausible Startausrichtung und Bounds im Viewer

### 3. Zusammengesetztes Stations- oder Capital-Modell
- Iteration-2-Referenz: `DATA/SOLAR/MISC/space_dome.cmp`
- Archetype: `space_dome`
- Typ: `.cmp`
- Zweck:
  - Hierarchie-/Parent-Child-Transform-Test
  - Family-/Header-Stream-Test im Solar-Bereich
  - Bounds-/Kamera-Fit unter komplexeren Transformketten

### 4. Härterer Dockable-Solar-Fall
- Iteration-3-Referenz: `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`
- Archetype: `docking_ringx2_lod`
- Typ: `.cmp`
- Zweck:
  - härterer `structured-family`-/Header-Stream-Fall im Dockable-Bereich
  - ref-/source-name-genaue Blockauswahl gegen V1 prüfen
  - Regression für größere zusammengesetzte Solar-Modelle
### 5. Materialgruppentest
- Iteration-5-Referenz: `DATA/BASES/GENERIC/ocean_blue.cmp`
- Archetype: `ocean_blue`
- Typ: `.cmp`
- Zweck:
  - erster kleiner Testfall mit expliziten Texture-Library-/Material-Library-Referenzen
  - Regression für Preview-Binding-Kandidatenlisten ohne doppelte Textureinträge
  - jetzt zusätzlich erster abgesicherter `token-match`-Fall im Preview-Binding-Pfad
  - Referenz dafür, dass bessere Namenszuordnung die Texture-Auswahl schärft, ohne die bisherige Referenzreihenfolge umzudrehen

### 6. Part-Aware Binding-Fall
- Iteration-6-Referenz: `DATA/BASES/BRETONIA/br_barbican_station_deck.cmp`
- Archetype: `br_barbican_station_deck`
- Typ: `.cmp`
- Zweck:
  - zweiter expliziter Texture-Binding-Fall mit größerer Material-Library
  - Regression für part-aware Preview-Bindings auf Base-/Interior-CMPs
  - sichert, dass Partkontext über Part-`fileName`/`sourceName`/`objectName` nicht verloren geht, auch wenn der Ref-Pfad allein nicht reicht
  - jetzt zusätzlich Referenz für den Restfall `global-texture-library-fallback`, bei dem die Texture-Verweise nur global aus der Material-Library stammen und deshalb noch keinen echten Matchaufstieg erlauben

### 7. Komplexerer Ship-No-Texture-Fall
- Iteration-7-Referenz: `DATA/SHIPS/ORDER/OR_ELITE/or_elite.cmp`
- Archetype: `or_elite`
- Typ: `.cmp`
- Zweck:
  - zusätzlicher Schiffs-Guard mit deutlich mehr Refs als `cv_starflier.cmp`
  - Regression für direkte `VMeshRef -> VMeshData`-Auflösung über mehrere LOD-Stufen in einem komplexeren Ship-CMP
  - Regression für part-aware Preview-Bindings auch dann, wenn keine expliziten Material-/Texture-Referenzen vorhanden sind

## Vergleichskriterien

### Decoder
- lädt ohne Crash
- liefert Geometrie
- keine offensichtlich leeren Parts

### Transform
- Part-Positionen relativ zueinander plausibel
- Parent-/Child-Kette wirkt korrekt
- Bounds nicht offensichtlich falsch

### Viewer
- Modell startet nicht extrem falsch skaliert oder orientiert
- Kamera-Fit ist plausibel
- keine doppelte oder zerrissene Geometrie

## Nächster Schritt
Für die nächste Decoder-Stufe sollten diese Referenzmodelle jeweils mit einem kurzen V1-Screenshot oder einer knappen Sollbeschreibung ergänzt werden, damit Veränderungen nicht nur technisch, sondern auch visuell gegen V1 geprüft werden können.
