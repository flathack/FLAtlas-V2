# FLAtlas V2 3D Viewer Parity Plan

## Ziel
Die aktuelle 3D-Implementierung in `FLAtlas-V2` ist jetzt deutlich stabiler und als wiederverwendbares Modul angelegt, erreicht aber noch nicht den Darstellungsstandard von V1. Dieser Plan beschreibt die nächsten Schritte, um die native Freelancer-3D-Darstellung systematisch näher an V1 zu bringen.

Der Fokus ist bewusst:

1. Decoder-/Loader-Parität
2. korrekte Szenen- und Transform-Auflösung
3. Material-/MAT-Struktur
4. Render-Parität im Viewer

Nicht der Fokus der ersten nächsten Iteration:

- High-End-Viewer-Features
- UI-Politur
- vollständiger Texture-Feinschliff vor Decoder-Parität

## Aktueller Stand

### Bereits vorhanden
- stabilerer `3D Model Viewer`
- wiederverwendbarer `ModelViewport3D`
- `decoder first`-Pfad mit:
  - `.3db`
  - `.cmp`
  - `VMeshData`
  - `VMeshRef`
  - erste `structured-single-block`-Unterstützung
  - erste `structured-family`- und Header/Stream-Fallbacks
  - Triangle-Soup-Normalisierung
- Lazy-/deferred init im Viewer
- stabilerer Qt3D-Backendpfad

### Noch sichtbare Abweichungen zu V1
- Modelle sehen noch deutlich anders aus als in V1
- CMP-Part-/Hierarchy-Verhalten ist vermutlich noch unvollständig
- Material-/MAT-Logik ist noch nur vorbereitet
- Parent-/Child-Transforms sind wahrscheinlich noch nicht vollständig V1-paritätisch
- Submesh-/Materialgruppen-Verhalten ist noch vereinfacht
- Shading-/Render-Defaults weichen noch von V1 ab

## Annahmen

- V1 bleibt funktionale Referenz, aber nicht Architekturvorlage.
- V2 soll die 3D-Logik sauber modular behalten:
  - Decoder
  - Datenmodell
  - Asset-/Materialauflösung
  - Viewport/Renderer
  - Viewer-UI
- Material-/Texture-Support wird vorbereitet, aber nicht vor robuster Geometrie- und Transform-Parität priorisiert.

## Leitprinzipien

### Decoder first
Alle noch offenen Sonderfälle zuerst im Decoder/Loader lösen, nicht im Viewer mit Sonderlogik kaschieren.

### Referenzmodelle statt Blindflug
Für jede Stufe mindestens ein konkretes Referenzmodell gegen V1 vergleichen.

### Keine parallelen Pfade
Bestehende Pfade in `CmpLoader`, `VmeshDecoder`, `ModelCache`, `ModelGeometryBuilder`, `ModelViewport3D` erweitern, nicht daneben neu bauen.

### Renderer entkoppelt halten
Qt3D bleibt Konsument eines internen, bereits sauber dekodierten Modellformats.

## Umsetzungsphasen

## Phase 1: Referenzfälle festlegen

### Ziel
Einen kleinen festen Satz echter Freelancer-Modelle definieren, gegen die V2 und V1 verglichen werden.

### Aufgaben
- mindestens 1 einfache `.3db`
- mindestens 1 einfache `.cmp`
- mindestens 1 problematisches zusammengesetztes Modell
- mindestens 1 Modell mit auffälliger Material-/Submesh-Struktur
- mindestens 1 Modell mit sichtbaren Transform-/Hierarchy-Besonderheiten

### Vorschlag
- `TLR_lod.3db`
- ein einfaches Schiff
- ein zusammengesetztes Capital- oder Station-Modell
- ein Modell mit mehreren Materialgruppen

### Ergebnis
- feste Liste von Referenzmodellen
- pro Modell dokumentierte Soll-Beobachtung aus V1

## Phase 2: CMP-Hierarchie und Transform-Kette vervollständigen

### Ziel
Die Modellstruktur soll geometrisch an V1 heranrücken, bevor Materialien vertieft werden.

### Aufgaben
- Parent-/Child-Transform-Kette vollständig prüfen
- `Fix`-/`Cons`-/Constraint-Daten genauer gegen V1 abgleichen
- Rotationen/Translationen pro Part validieren
- Bounds-Berechnung für verschachtelte Nodes prüfen
- Hierarchie-Auflösung bei `.cmp` auf echte Part-Struktur abgleichen

### Wahrscheinliche Arbeitspunkte
- `CmpLoader.cpp`
- internes `ModelNode`-Format
- Transform-Weitergabe im `ModelGeometryBuilder`
- Szenenaufbau in `ModelViewport3D`

### Akzeptanzkriterium
Referenz-CMPs liegen in Position, Ausrichtung und relativer Teilstruktur deutlich näher an V1.

## Phase 3: VMesh-/Family-Sonderfälle weiter nachziehen

### Ziel
Noch fehlende V1-Sonderfälle im nativen Geometriepfad schließen.

### Aufgaben
- `structured-family` weiter vervollständigen
- echte mehrblockige `header-stream`-Fälle sauber abbilden
- zusätzliche Family- bzw. Stream-Metadaten aus V1 übernehmen
- CRC-/Block-Zuordnung weiter härten
- Geometrie-Sanitizer nur dort anwenden, wo V1-Logik das auch erwarten lässt

### Wahrscheinliche Arbeitspunkte
- `VmeshDecoder.cpp`
- `CmpLoader.cpp`

### Akzeptanzkriterium
Problematische Referenzmodelle dekodieren ohne sichtbare Geometrie-Aussetzer und ohne Viewer-Sonderbehandlung.

## Phase 4: Materialgruppen und MAT-Referenzen vorbereiten

### Ziel
Materialstruktur korrekt erkennen, auch wenn vollständige Texturparität noch nicht sofort kommt.

### Aufgaben
- Materialnamen sauber je Mesh/Submesh mitführen
- `.mat`-Referenzen aus Model-/Archetype-Kontext erfassen
- Materialgruppen vom Decoder bis in die Renderrepräsentation erhalten
- Fallback-Farben je Materialgruppe stabil und reproduzierbar machen

### Bewusst noch nachrangig
- vollständige Texture-Sampling-Parität
- Shader-Parität

### Wahrscheinliche Arbeitspunkte
- `CmpLoader`
- internes Modellformat
- `MaterialFactory`
- `ModelGeometryBuilder`

### Akzeptanzkriterium
Submesh- und Materialgrenzen stimmen sichtbar besser mit V1 überein, auch wenn Texturen zunächst noch vereinfacht sind.

## Phase 5: V1-nahe Render-Defaults

### Ziel
Der Viewer soll optisch näher an V1 wirken, sobald Decoder und Struktur belastbar genug sind.

### Aufgaben
- Shading-Defaults mit V1 vergleichen
- Culling-/Normalen-Verhalten prüfen
- Mesh-/Wireframe-/Bounds-Defaults evaluieren
- Kamera-Fit und Startansicht an V1 annähern
- Hintergrund-/Licht-Defaults überprüfen

### Wahrscheinliche Arbeitspunkte
- `ModelViewport3D`
- `MaterialFactory`
- `OrbitCamera`

### Akzeptanzkriterium
Referenzmodelle wirken bei Standardansicht deutlich näher an V1, ohne manuelle Viewer-Korrekturen.

## Phase 6: Material-/Texture-Ausbau

### Ziel
Nach stabiler Geometrie- und Strukturparität die Darstellung weiter verbessern.

### Aufgaben
- `.mat`-Dateien gezielt auswerten
- Texturpfade auflösen
- vorbereitete Materialdaten in Qt3D-Materialien überführen
- fehlende Texturen robust abfangen

### Wichtig
Dies ist bewusst nachgeordnet. Geometrie- und Hierarchie-Parität haben Vorrang.

## Phase 7: Regressionstests und Vergleichs-Workflow

### Ziel
Die 3D-Entwicklung soll nicht wieder regressieren.

### Aufgaben
- gezielte Decoder-Tests pro Referenzmodell
- Viewport-Repro-Tests für problematische Modelle
- Crash-Repro-Fälle beibehalten
- Ausgabe von Decoder-Warnungen strukturieren

### Bereits vorhanden
- gezielte Tests für `Trade Lane`-Modelle

### Ausbau
- mehr Referenzmodelle in Tests aufnehmen
- bei Bedarf Snapshot-/Metadatenvergleich statt Bildvergleich

## Konkrete Reihenfolge der nächsten Arbeit

1. Referenzmodelle fest definieren und dokumentieren
2. CMP-Hierarchie/Transforms gegen V1 angleichen
3. fehlende `structured-family`-/`header-stream`-Fälle weiter portieren
4. Materialgruppen sauber durch die Pipeline tragen
5. Render-Defaults näher an V1 bringen
6. erst danach Material-/Texture-Ausbau priorisieren

## Betroffene Kernkomponenten

### Decoder / Loader
- `src/infrastructure/io/CmpLoader.cpp`
- `src/infrastructure/io/CmpLoader.h`
- `src/infrastructure/io/VmeshDecoder.cpp`
- `src/infrastructure/io/VmeshDecoder.h`

### Modell-/Cache-Schicht
- `src/rendering/preview/ModelCache.cpp`
- `src/rendering/preview/ModelCache.h`

### Render-/Viewport-Schicht
- `src/rendering/view3d/ModelGeometryBuilder.cpp`
- `src/rendering/view3d/ModelGeometryBuilder.h`
- `src/rendering/view3d/ModelViewport3D.cpp`
- `src/rendering/view3d/ModelViewport3D.h`
- `src/rendering/view3d/MaterialFactory.cpp`
- `src/rendering/view3d/MaterialFactory.h`
- `src/rendering/view3d/OrbitCamera.cpp`
- `src/rendering/view3d/OrbitCamera.h`

### Viewer-UI
- `src/rendering/preview/ModelViewerPage.cpp`
- `src/rendering/preview/ModelViewerPage.h`
- `src/rendering/preview/ModelPreview.cpp`
- `src/rendering/preview/ModelPreview.h`

## Risiken

- V1 enthält vermutlich implizites Wissen über Sonderfälle, das nicht 1:1 im aktuellen V2-Code sichtbar ist.
- Einige visuelle Unterschiede können nicht nur aus dem Decoder, sondern auch aus Materials, Lighting und Shading stammen.
- Zu aggressive Sanitizer-/Fallback-Logik kann Modelle zwar stabilisieren, aber von V1 optisch weiter entfernen.

## Definition of Done für die nächste große 3D-Stufe

Die nächste Stufe gilt als erreicht, wenn:

- mehrere Referenzmodelle ohne Crash geladen werden
- mindestens eine `.3db` und eine `.cmp` visuell deutlich näher an V1 liegen
- CMP-Part-Struktur und Transform-Kette nicht mehr offensichtlich falsch wirken
- der Viewer ohne Sonderbehandlung auf demselben Decoderpfad aufsetzt
- Materialgruppen im internen Modellformat sauber vorhanden sind, auch wenn Texturen noch nicht vollständig umgesetzt sind
