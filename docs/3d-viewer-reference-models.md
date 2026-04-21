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
- Kandidat: erstes kleines Fighter-/Freighter-CMP aus `DATA/SHIPS`
- Typ: `.cmp`
- Zweck:
  - einfache CMP-Part-Struktur
  - Vergleich von Part-Auflösung und Startansicht

### 3. Zusammengesetztes Stations- oder Capital-Modell
- Kandidat: größeres `.cmp` mit mehreren Parts
- Typ: `.cmp`
- Zweck:
  - Hierarchie-/Parent-Child-Transform-Test
  - Bounds-/Kamera-Fit unter komplexeren Transformketten

### 4. Materialgruppentest
- Kandidat: Modell mit klar sichtbaren Submesh-/Materialwechseln
- Typ: `.cmp` oder `.3db`
- Zweck:
  - spätere MAT-/Material-Gruppen-Parität

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
