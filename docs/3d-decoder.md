# FL Atlas V2 — 3D-Decoder Technische Dokumentation

> Stand: April 2026  
> Bezug: aktuell funktionierender Codestand nach Behebung aller drei bekannten Bugs (LOD, Hierarchie, FVF-Stride)

---

## Inhaltsverzeichnis

1. [Überblick](#überblick)
2. [Unterstützte Dateiformate](#unterstützte-dateiformate)
3. [Architektur](#architektur)
4. [Datenfluss](#datenfluss)
5. [CMP-spezifische Logik](#cmp-spezifische-logik)
6. [3DB-spezifische Logik](#3db-spezifische-logik)
7. [LOD-Verhalten](#lod-verhalten)
8. [Installationsübergreifende Kompatibilität](#installationsübergreifende-kompatibilität)
9. [Konkreter Referenzfall: li_dreadnought.cmp](#konkreter-referenzfall-li_dreadnoughtcmp)
10. [Fehlerquellen und typische Stolperfallen](#fehlerquellen-und-typische-stolperfallen)
11. [Debugging-Anleitung](#debugging-anleitung)
12. [Teststrategie](#teststrategie)
13. [Lessons Learned aus den behobenen Bugs](#lessons-learned-aus-den-behobenen-bugs)

---

## Überblick

Der 3D-Decoder von FL Atlas V2 liest Freelancer-native 3D-Modelle und überführt sie in eine interne `ModelNode`-Hierarchie, die anschließend im Qt3D-Viewport dargestellt wird. Er ist der **einzige Einstiegspunkt** für alle geometriebezogenen Operationen in der 3D-Vorschau.

**Zweck:**
- Binäre Freelancer-Modelldateien (`.cmp`, `.3db`) vollständig parsen
- Geometrie, Texturverweise und Transformationshierarchien extrahieren
- Ergebnis an den 3D-Renderer übergeben, ohne mod-spezifische Sonderfälle zu benötigen

**Grundsatz:** Der Decoder arbeitet rein datengetrieben. Alle Entscheidungen (Stride, Hierarchie, LOD) werden aus den Dateiinhalten selbst abgeleitet – nie hartcodiert für eine bestimmte Installation.

---

## Unterstützte Dateiformate

| Format | Status | Beschreibung |
|--------|--------|--------------|
| `.cmp` | ✅ vollständig | Compound Model – mehrere Parts mit Hierarchie und Transformationen |
| `.3db` | ✅ vollständig | Single-Part-Modell (Sonderfall eines UTF-Containers) |
| `.mat` | ⚠️ teilweise | Materialreferenzen werden extrahiert, aber keine eigene Geometry |
| Textur-Auflösung | ⚠️ teilweise | `FreelancerMaterialResolver` versucht Texturen aus dem Installationspfad zu finden; scheitert bei Pfadunterschieden zwischen Mods |
| Skelette / Animationen | ❌ nicht unterstützt | Rev- und Pris-Gelenke werden nur für Transformationen genutzt, nicht animiert |

### Was vollständig unterstützt wird
- UTF-Container-Parsing (`UtfFileHeader`, `FlatUtfNode`-Baum)
- VMeshData-Dekodierung mit korreker FVF-Auswertung (inkl. 3-UV-Sets für Crossfire)
- CMP-Hierarchieaufbau aus Fix-, Rev- und Pris-Records
- LOD-Filterung (nur beste Stufe)
- Bounds-Berechnung für Kamera-Framing

### Was bewusst noch nicht gelöst ist
- Skelettanimationen (Rev/Pris als Gelenke animierbar)
- Vollständige Texturauflösung bei Mods mit abweichender Verzeichnisstruktur
- `.3db`-Dateien mit mehreren VMeshData-Blöcken ohne VMeshRef (Fallback-Decoder)

---

## Architektur

### Beteiligte Dateien

```
src/infrastructure/io/
  CmpLoader.h              – Öffentliche API, alle Datenstrukturen (Structs)
  CmpLoader.cpp            – Vollständige Decoder-Implementierung (~3600 Zeilen)
  VmeshDecoder.h           – FVF-Konstanten, DecodedMesh-Struct, Decoder-API
  VmeshDecoder.cpp         – Roh-Vertex-Dekodierung und Stride-Berechnung

src/rendering/view3d/
  ModelViewport3D.h/cpp    – Qt3D-Viewport-Widget; ruft CmpLoader auf, rendert Ergebnis
  ModelGeometryBuilder.h/cpp – Übersetzt MeshData → Qt3D-Geometrie-Objekte
  MaterialFactory.h/cpp    – Erstellt Qt3D-Materialien (Phong, texturiert)

src/infrastructure/freelancer/
  FreelancerMaterialResolver – Löst Texturnamen zu Dateipfaden auf
```

### Verantwortlichkeiten

| Komponente | Verantwortung |
|---|---|
| `CmpLoader::loadModel()` | Haupteinstieg: öffnet Datei, dispatcht alle Phasen, gibt `DecodedModel` zurück |
| `parseFlatUtfNodes()` | UTF-Baum zu flacher Node-Liste (intern als `FlatUtfNode`) |
| `buildPartsFromNodes()` | Extrahiert CMP-Parts aus `/Cmpnd/Root` und `Part_*`-Knoten |
| `parseVMeshDataBlocks()` | Findet alle `VMeshData`-Knoten, liest Header-Hints incl. FVF |
| `parseVMeshRefs()` | Liest alle `VMeshRef`-Knoten (60-Byte-Records mit Vertex/Index-Ranges) |
| `parseCmpFixRecords()` | Liest Fix-Records (176-Byte: Translation + Rotation je Part-Pair) |
| `buildCmpTransformHints()` | Baut vollständige Transformationshierarchie aus Fix + Rev + Pris |
| `buildMeshesFromVMesh()` | Dekodiert Geometrie für einen VMeshRef gegen einen VMeshDataBlock |
| `decodeStructuredSingleBlockMeshFromHeaders()` | Präziser Decoder für VMeshData-Blöcke mit Mesh-Header-Tabelle |
| `VmeshDecoder::vertexStride()` | Berechnet Stride aus FVF-Wert – **kritische Funktion** |
| `VmeshDecoder::uvSetCount()` | Liest UV-Anzahl als numerisches Feld aus FVF – **fix 2026** |
| `ModelViewport3D::addNodeRecursive()` | Übersetzt ModelNode-Baum in Qt3D-Entity-Baum, LOD-Filter |
| `ModelGeometryBuilder` | Baut Qt3D-Buffer, Attribute, GeometryRenderer aus MeshData |

### Wo Binär-Parsing endet und Rendering beginnt

```
CmpLoader::loadModel()
  → gibt DecodedModel / ModelNode zurück
  → ModelNode enthält: name, origin (QVector3D), rotation (QQuaternion),
                       meshes (MeshData[]), children (ModelNode[])

ModelViewport3D::loadModel()
  → ruft CmpLoader::loadModel() auf
  → ruft addNodeRecursive() auf mit dem root ModelNode

addNodeRecursive()
  → erstellt Qt3DCore::QEntity pro Node
  → setzt Qt3DCore::QTransform mit origin/rotation
  → für jedes MeshData: ModelGeometryBuilder::buildTriangleRenderer()
  → fügt Material hinzu (texturiert oder Phong-Fallback)
```

Ab `addNodeRecursive()` ist alles Qt3D – kein Freelancer-Parsing mehr.

---

## Datenfluss

```
Dateipfad (z.B. li_dreadnought.cmp)
  │
  ▼
CmpLoader::loadModel(filePath)
  │
  ├─ QFile::readAll() → raw: QByteArray
  ├─ Dateiendung → NativeModelFormat (Cmp / Db3)
  ├─ parseUtfHeader() → UtfFileHeader
  │     Magic: "UTF " (4 Bytes), Version, Offsets für Node-Block und Daten
  │
  ├─ parseFlatUtfNodes() → QVector<FlatUtfNode>
  │     Flache Liste aller UTF-Knoten mit name, parentName, path, dataOffset
  │
  ├─ buildPartsFromNodes() → QVector<NativeModelPart>
  │     Sucht /Cmpnd/Root und Part_* Knoten
  │     Liest: name, objectName, fileName, sourceName (.vms), cmpIndex
  │
  ├─ parseVMeshDataBlocks() → QVector<VMeshDataBlock>
  │     Alle "VMeshData"-Knoten (Roh-Geometriedaten)
  │     Liest: familyKey, strideHint, FVF aus Header
  │
  ├─ parseVMeshRefs() → QVector<VMeshRefRecord>
  │     Alle "VMeshRef"-Knoten (60 Bytes je Record)
  │     Felder: meshDataReference (CRC32 oder Index),
  │             vertexStart/Count, indexStart/Count, groupStart/Count,
  │             bounds (min/max/radius)
  │
  ├─ parseCmpFixRecords() → QVector<CmpFixRecord>
  │     Fix-Records: je 176 Bytes, parentName, childName,
  │                  Translation (3 floats), Rotation (3x3 Matrix → QQuaternion)
  │
  ├─ buildCmpTransformHints() → QVector<CmpTransformHint>
  │     Zusammenführung von Fix + Rev + Pris Transformationen
  │     Normalisierung der Part-Namen
  │     Hierarchie: parentByPart[partName] = parentPartName
  │
  ├─ extractMaterialReferences() + buildPreviewMaterialBindings()
  │     Ordnet Texturnamen den VMeshRef-Gruppen zu
  │
  └─ Hierarchieaufbau (CMP-Pfad):
        for each Part → extractPart() → ModelNode
        buildHierarchy() → rekursiver Aufbau per parentByPart-Map
        Ergebnis: root.children[0] = "Root"-ModelNode mit allen Part_*-Kindern

  ▼
DecodedModel.rootNode: ModelNode (Baum)

  ▼
ModelViewport3D::addNodeRecursive(root, ...)
  │
  ├─ QEntity + QTransform (origin, rotation)
  ├─ LOD-Filter: bestLodIdx = min(lodIndex) aller Meshes
  └─ für jedes Mesh (nur bestLod):
       ModelGeometryBuilder::buildTriangleRenderer()
       → QGeometry + QBuffer + QAttribute (position, normal, texcoord, indices)
       MaterialFactory oder FreelancerMaterialResolver
       → Qt3DRender::QMaterial

  ▼
Darstellung im Qt3D-Viewport
```

### VMeshRef → VMeshData Auflösung

`parseVMeshRefs()` speichert in jedem `VMeshRefRecord` das Feld `meshDataReference` (ein `uint32`). Das ist entweder:
- Ein **direkter Index** (0-basiert) in die VMeshDataBlock-Liste, oder
- Ein **CRC32** des VMeshData-Quellnamens (z.B. `freelancerCrc32("li_dreadnought_lod0.vms")`)

Die Funktion `resolveVMeshDataBlockForRef()` versucht beide Interpretationen und gibt den passenden Block zurück. Scheitert das, greift ein Fallback über den obersten UTF-Pfadsegment-Vergleich.

---

## CMP-spezifische Logik

### UTF-Baumstruktur einer typischen CMP-Datei

```
/Cmpnd/
  Root/
    Index        (uint32: 0)
    Object name  (string: "li_dreadnought")
    File name    (string: "li_dreadnought_lod0.3db")
  Part_Li_frnt_lod1/
    Index        (uint32: 1)
    Object name  (string: "Part_Li_frnt")
    File name    (string: "li_frnt_lod0.3db")
    li_frnt_lod0.vms/
      (VMesh source name reference)
/VMeshLibrary/
  li_dreadnought_lod0.vms/
    VMeshData    (Roh-Geometriedaten)
  li_dreadnought_lod0.vms\Level0/
    VMeshRef     (60-Byte Record)
    VMeshRef     (weitere Refs für weitere Material-Gruppen)
  ... (weitere LOD-Levels)
/Fix/
  (176-Byte Records: je Part, Translation + Rotation)
/Rev/ oder /Pris/
  (208-Byte Records: je Gelenk)
```

### Fix-Records (176 Bytes)

```
Offset  Typ       Beschreibung
  0     char[64]  Elternteil (parent part name, null-terminated)
 64     char[64]  Kind (child part name, null-terminated)
128     float[3]  Translation (x, y, z)
140     float[9]  Rotationsmatrix 3×3 (row-major)
```

Die Rotation wird in `parseCmpFixRecords()` direkt in einen `QQuaternion` umgewandelt.

### Hierarchieaufbau

`buildCmpTransformHints()` führt drei Quellen zusammen:

1. **Fix-Records**: Liefern `parentName → childName`-Relation sowie Translation/Rotation  
   Schlüssel: **Object-Name** des Kinds (z.B. `"Part_Li_frnt"`)
2. **Rev-Records** (208 Bytes): Gelenke für rotierende Teile (z.B. Turbinen)  
   Schlüssel: **Part-Name** (z.B. `"part_li_engine_lod1"`)
3. **Pris-Records** (208 Bytes): Prismatische Gelenke (Schiebeverbindungen)  
   Schlüssel: **Part-Name**

**Kritisches Problem (behoben):** Rev/Pris-Records speichern im "parent"-Feld den **Gelenkachsen-Namen**, nicht den echten Eltern-Part-Namen. Wenn dieser auf denselben normalisierten Key wie ein Fix-Record abbildet, würde die (korrekte) Fix-Hierarchie überschrieben werden. Die Lösung:

```cpp
// In buildCmpTransformHints() – normalization loop:
if (existing != normalizedParentByPart.cend() && !existing.value().isEmpty() && value.isEmpty())
    continue; // Don't replace a valid parent with an empty one
```

Diese Zeile verhindert, dass ein leerer Rev-Elternname einen bereits gesetzten, gültigen Fix-Elternnamen überschreibt.

### Warum kein mod-spezifisches Sonderverhalten

Unterschiede zwischen Vanilla FL und Crossfire-Dateien liegen ausschließlich in:
- FVF-Werten (z.B. `0x112` vs. `0x312`)
- Anzahl der VMeshData-Blöcke
- Polygon-Anzahl (Crossfire hat höher aufgelöste Geometrie)

All diese Unterschiede werden aus den binären Headern der Dateien selbst gelesen. Es gibt keine `if (crossfire)` Verzweigung im Code.

---

## 3DB-spezifische Logik

Eine `.3db`-Datei ist strukturell ein UTF-Container ohne `Cmpnd`-Knoten. Sie enthält typischerweise:

```
/Root/
  VMeshData    (direkte Geometrie)
  VMeshRef     (oder mehrere, je Level)
```

`CmpLoader::loadModel()` erkennt `.3db` über die Dateiendung und setzt `NativeModelFormat::Db3`. Die gesamte Parse-Pipeline ist identisch mit CMP – der Unterschied liegt im Hierarchieaufbau: Da kein `Cmpnd`-Knoten existiert, fällt der Code in den Fallback-Pfad:

```cpp
if (root.children.isEmpty()) {
    // Suche nach Root- oder Basisname-Knoten direkt
    QString partPath = firstPathForName(result.utfNodes, QFileInfo(filePath).baseName());
    ...
    ModelNode direct = extractPart(...);
    if (!direct.meshes.isEmpty() || !direct.children.isEmpty())
        root = direct;
}
```

`.3db`-Dateien haben in der Regel **keine LOD-Metadaten** (`lodIndex = -1` für alle Meshes). Der LOD-Filter in `addNodeRecursive()` ist in diesem Fall inaktiv (`lodFilterActive = false`), sodass alle Meshes gerendert werden.

---

## LOD-Verhalten

### Wie LOD-Levels in CMP-Dateien kodiert sind

CMP-Dateien enthalten für jedes Part mehrere „Level"-Unterknoten im VMeshLibrary-Ast:

```
/VMeshLibrary/li_dreadnought_lod0.vms/
  Level0/
    VMeshRef   ← LOD 0 (höchste Qualität)
  Level1/
    VMeshRef   ← LOD 1
  Level2/
    VMeshRef   ← LOD 2
```

`parseVMeshRefs()` extrahiert aus dem Pfad (`levelSegmentFromUtfPath()`) den Level-Index und schreibt ihn als `MeshData::lodIndex`.

### LOD-Filter im Renderer

**Problem (behoben):** Früher wurden alle LOD-Meshes gleichzeitig in die Qt3D-Szene eingefügt, was die typischen „überlagerten Silhouetten"-Artefakte erzeugte.

**Aktuelle Lösung** in `ModelViewport3D::addNodeRecursive()`:

```cpp
int bestLodIdx = std::numeric_limits<int>::max();
for (const auto &m : node.meshes) {
    if (m.lodIndex >= 0 && m.lodIndex < bestLodIdx)
        bestLodIdx = m.lodIndex;
}
const bool lodFilterActive = bestLodIdx < std::numeric_limits<int>::max();

for (const auto &mesh : node.meshes) {
    if (lodFilterActive && mesh.lodIndex >= 0 && mesh.lodIndex != bestLodIdx)
        continue;  // Überspringe schlechtere LOD-Stufen
    // ... render mesh
}
```

**Regeln:**
- `lodIndex = -1` → kein LOD-Metadatum (typisch für `.3db`), immer rendern
- `lodIndex = 0` → höchste Qualität, bevorzugt
- Nur die Meshes mit `lodIndex == bestLodIdx` werden gerendert
- Dieser Filter wird auch in `ModelGeometryBuilder::boundsForNode()` angewendet, damit Kamera-Framing nur auf dem echten Geometry-Bereich basiert

---

## Installationsübergreifende Kompatibilität

### Beobachtete Unterschiede zwischen TESTMOD1 (Vanilla) und Crossfire

| Eigenschaft | TESTMOD1 | Crossfire |
|---|---|---|
| FVF `li_dreadnought.cmp` | `0x112` | `0x312` |
| UV-Sets | 1 | 3 |
| Vertex-Stride | 32 Bytes | 48 Bytes |
| Dateigröße `.cmp` | 1,29 MB | 2,95 MB |
| Vertex-Count (Root LOD0) | ~1913 | ~1483 (nach Fix) |
| VMeshData-Blöcke | 7 | 8 |

### FVF-Interpretation (der entscheidende Unterschied)

Das D3D Flexible Vertex Format kodiert die UV-Set-Anzahl als **numerisches Feld** in Bits 8–11:

```
FVF = 0x312
     ^^^^
     0x300 → (0x300 >> 8) = 3 UV-Sets
     0x010 → Normal-Flag
     0x002 → Position-Flag
```

**Falsche (alte) Implementierung:**
```cpp
if (fvf & FVF_TEX2) return 2;  // 0x312 & 0x200 = true → FALSCH: gibt 2 statt 3
```

**Korrekte (aktuelle) Implementierung** in `VmeshDecoder::uvSetCount()`:
```cpp
return static_cast<int>((fvf & 0x0F00u) >> 8);  // 0x312 → 3
```

Mit falschem Stride (40 statt 48 Bytes) las der Decoder ab Vertex 1 Bytes aus dem UV3-Bereich des vorherigen Vertex als XY-Position. UV-Koordinaten sind normiert (0.0–1.0), deshalb sahen die Positionen wie Einheitsvektoren aus → sternförmige Artefakte.

---

## Konkreter Referenzfall: li_dreadnought.cmp

```
Dateipfade:
  TESTMOD1:  C:\...\FL-Installationen\TESTMOD1\DATA\SHIPS\LIBERTY\LI_DREADNOUGHT\li_dreadnought.cmp
  Crossfire: C:\...\FL-Installationen\Freelancer Crossfire\DATA\SHIPS\LIBERTY\LI_DREADNOUGHT\li_dreadnought.cmp
```

### Aufgetretene Fehler und deren Ursachen

#### Bug 1: Mehrere LOD-Stufen gleichzeitig sichtbar
**Symptom:** Das Modell zeigte überlagerte, halb-transparente Schichten  
**Ursache:** `addNodeRecursive()` renderte alle `MeshData`-Objekte ohne LOD-Filterung  
**Fix:** LOD-Filter in `ModelViewport3D::addNodeRecursive()` und `ModelGeometryBuilder::boundsForNode()` — nur `lodIndex == bestLodIdx` wird gerendert

#### Bug 2: Fehlende Heck-Parts in TESTMOD1 (Hierarchy-Bug)
**Symptom:** `Part_Li_reactor_core`, `Part_Li_engine` und die 8 Leaf-Parts wurden nicht angezeigt, obwohl sie in der Datei vorhanden sind  
**Ursache:** Rev-Records für die Turbinengelenke hatten denselben normalisierten Part-Schlüssel wie Fix-Records, aber enthielten einen leeren Elternnamen (Rev speichert Gelenkachsennamen, nicht Part-Namen). Der leere Wert überschrieb den korrekten Fix-Elternnamen in `parentByPart`.  
**Fix:** In `buildCmpTransformHints()` wird beim Normalisieren der `parentByPart`-Map ein Eintrag nur dann überschrieben, wenn der neue Wert **nicht leer** ist oder der bestehende Wert **leer** war:
```cpp
if (existing != normalizedParentByPart.cend() && !existing.value().isEmpty() && value.isEmpty())
    continue;
```

#### Bug 3: Sternförmige Artefakte in Crossfire (FVF-Stride-Bug)
**Symptom:** Crossfire zeigte `li_dreadnought.cmp` als Starburst-Explosion, TESTMOD1 war korrekt  
**Ursache:** Crossfire verwendet FVF `0x312` (3 UV-Sets). `VmeshDecoder::uvSetCount()` behandelte die UV-Anzahl als Bitmask statt als numerisches Feld und gab fälschlich 2 zurück → Stride 40 statt 48 → jeder Vertex ab Index 1 wurde um 8 Bytes falsch ausgerichtet gelesen  
**Fix:** `uvSetCount()` liest das UV-Feld jetzt korrekt: `return (fvf & 0x0F00u) >> 8`  
**Validierung:** Beide Installationen liefern identische World Bounds:  
`X[-46.76..48.05]  Y[-57.34..63.80]  Z[-266.82..229.34]  D=496.2`

---

## Fehlerquellen und typische Stolperfallen

### 1. Falsche Annahmen über Reihenfolge in CMP-Dateien

**Risiko:** Der Code an mehreren Stellen (z.B. in `buildPartsFromNodes()`) enthält Fallback-Schleifen, die auf die sequenzielle Reihenfolge von Nodes im UTF-Baum vertrauen, wenn die reguläre Parent-Path-Suche versagt. Das ist ein Workaround für CMPs, die inkonsistente Baumstrukturen haben.

**Symptom:** Ein Part „sieht" keine Kindknoten und wird ohne Geometry gebaut  
**Abhilfe:** Zuerst prüfen, ob `buildChildrenByParentPath()` alle erwarteten Knoten findet

### 2. Unvollständig aufgelöste Hierarchien (parentByPart)

**Risiko:** Wenn `normalizePartKey()` für einen Part einen anderen Schlüssel erzeugt als für seine Eltern-Referenz (z.B. durch Groß-/Kleinschreibung oder Sonderzeichen), landet der Part als impliziter Root-Knoten statt als Kind.

**Symptom:** Subteile erscheinen losgelöst vom Hauptmodell oder fehlen ganz  
**Diagnose:** `result.cmpTransformHints` im `DecodedModel` auslesen – `parentPartName` für den betreffenden Part prüfen

### 3. VMeshRef findet keinen passenden VMeshDataBlock

**Risiko:** `resolveVMeshDataBlockForRef()` kann fehlschlagen, wenn:
- CRC32 des Quellennamens nicht mit dem Referenz-Wert übereinstimmt
- Mehrere VMeshData-Blöcke denselben Top-Level-Pfad haben

**Symptom:** Part wird ohne Geometrie gerendert (leere `ModelNode`)  
**Diagnose:** `ref.debugHint` und `ref.matchedSourceName` im `VMeshRefRecord` prüfen; leer = keine Zuordnung gefunden

### 4. Falscher Vertex-Stride durch unbekannten FVF-Wert

**Risiko:** Neue Mods könnten FVF-Werte verwenden, die `decodeStructuredSingleBlockVertices()` nicht kennt oder für die `vertexStride()` 0 zurückgibt.

**Symptom:** Geometry erscheint als Starburst/Explosion oder fehlt ganz  
**Diagnose:** FVF-Wert aus `VMeshDataBlock::headerHint.flexibleVertexFormatHint` auslesen und mit bekannten Werten vergleichen:
- `0x112` = Position + Normal + 1 UV-Set → Stride 32 (Vanilla)
- `0x312` = Position + Normal + 3 UV-Sets → Stride 48 (Crossfire)
- `0x142` = Position + Diffuse + 1 UV-Set → Stride 24

### 5. LOD-Probleme durch fehlende Level-Annotation

**Risiko:** VMeshRef-Knoten ohne erkennbares `Level`-Segment im UTF-Pfad erhalten `lodIndex = -1`. Wenn dann andere Refs im selben Part `lodIndex >= 0` haben, wird der Mesh ohne LOD-Annotation immer gerendert (er wird nicht gefiltert, weil `lodFilterActive && mesh.lodIndex >= 0` für ihn `false` ergibt).

**Symptom:** Extra-Geometrie erscheint zusätzlich zur korrekten LOD-Stufe  
**Abhilfe:** Prüfen, ob alle VMeshRefs eines Parts ein konsistentes `levelName`-Feld haben

### 6. Risiken bei späteren Refactorings

| Bereich | Risiko |
|---|---|
| `uvSetCount()` | Jede Änderung hier beeinflusst Stride aller VMeshData-Blöcke weltweit |
| `normalizePartKey()` | Änderungen können die Hierarchie für Crossfire oder andere Mods brechen |
| `parseCmpFixRecords()` — Offset 128/140 | Falsche Offset-Annahmen brechen alle Transformationen |
| `parseVMeshRefs()` — 60-Byte-Record-Layout | Felder sind nach Position, nicht nach Namen adressiert |
| LOD-Filter-Logik | `lodIndex = -1` als "kein Filter" ist eine Konvention, nicht explizit dokumentiert im Freelancer-Format |

---

## Debugging-Anleitung

### Schritt 1: Symptom klassifizieren

| Symptom | Wahrscheinliche Ursache | Prüfe zuerst |
|---|---|---|
| Starburst / Explosion | Falscher Vertex-Stride (FVF-Bug) | `uvSetCount()`, FVF-Wert im Header |
| Overlappping Silhouetten | Alle LOD-Stufen gleichzeitig | `addNodeRecursive()` LOD-Filter |
| Teile fehlen komplett | Hierarchie-Auflösung gescheitert | `parentByPart`-Map, `CmpTransformHint` |
| Teile am falschen Ort | Falsche Translation/Rotation | Fix-Record-Parsing, `localTranslations`-Merge |
| Modell unsichtbar/leer | VMeshRef → VMeshData Zuordnung fehlgeschlagen | `ref.debugHint`, `ref.matchedSourceName` |
| Korrekt in Vanilla, falsch in Crossfire | FVF-Unterschied | FVF-Wert vergleichen |

### Schritt 2: Diagnostic-Probe verwenden

Im Projektverzeichnis liegt `build/li_dreadnought_probe.cpp` (nicht im Quellbaum). Dieser Probe:
- Lädt beide Installationen
- Gibt weltkoordinaten-basierte Vertex-Positionen (erste 3 pro Node) aus
- Zeigt World Bounds mit Gesamtgröße

Kompilieren (nach Rebuild der Infrastructure):
```powershell
$env:PATH = "C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
& "C:\Qt\Tools\mingw1310_64\bin\g++.exe" .\build\li_dreadnought_probe.cpp `
  -std=c++20 -I.\src `
  -I"C:\Qt\6.8.3\mingw_64\include" -I"C:\Qt\6.8.3\mingw_64\include\QtCore" `
  -I"C:\Qt\6.8.3\mingw_64\include\QtGui" `
  -L.\build\src\infrastructure -L"C:\Qt\6.8.3\mingw_64\lib" `
  -lflatlas_infrastructure -lQt6Core -lQt6Gui `
  -o .\build\li_dreadnought_probe.exe
.\build\li_dreadnought_probe.exe
```

**Referenz-World-Bounds nach korrekter Dekodierung:**
```
WORLD BOUNDS: X[-46.76..48.05]  Y[-57.34..63.80]  Z[-266.82..229.34]
SIZE: W=94.81  H=121.15  D=496.17
```
→ Diese Werte müssen für **beide** Installationen identisch sein.

### Schritt 3: FVF-Werte eines beliebigen CMP dumpen

Das Skript `build/dump_fvf.py` durchsucht eine `.cmp`-Datei nach VMeshData-Blöcken und gibt FVF und berechneten Stride aus:

```powershell
cd "C:\Users\steve\Github\FLAtlas-V2"
python build\dump_fvf.py
```

### Schritt 4: Interne Strukturen inspizieren

`CmpLoader::loadModel()` gibt ein `DecodedModel` zurück. Für tiefgehende Diagnose kann dieser direkt ausgewertet werden:

```cpp
auto model = CmpLoader::loadModel(path);
// Parts:
for (const auto &part : model.parts)
    qDebug() << part.name << "parent:" << part.parentPartName;
// Transforms:
for (const auto &hint : model.cmpTransformHints)
    qDebug() << hint.partName << "→" << hint.parentPartName
             << "localT:" << hint.localTranslation;
// VMeshRefs:
for (const auto &ref : model.vmeshRefs)
    qDebug() << ref.nodePath << "matched:" << ref.matchedSourceName
             << "hint:" << ref.debugHint;
```

### Welche Logs nützlich sind

Die `warnings`-Liste im `DecodedModel` enthält Hinweise auf:
- Nicht aufgelöste VMeshRef-Zuordnungen
- Fehlende Fix-Records
- Direkte VMeshData-Fallback-Nutzung

---

## Teststrategie

### Pflicht-Testinstallationen

| Installation | Pfad |
|---|---|
| TESTMOD1 (Vanilla-Basis) | `C:\Users\steve\Github\FL-Installationen\TESTMOD1` |
| Crossfire | `C:\Users\steve\Github\FL-Installationen\Freelancer Crossfire` |

### Referenzmodelle

| Modell | Zweck | Bekannte Herausforderung |
|---|---|---|
| `SHIPS/LIBERTY/LI_DREADNOUGHT/li_dreadnought.cmp` | **Hauptreferenz** | FVF-Stride, Hierarchie, LOD |
| `SHIPS/ORDER/OR_OSIRIS/or_osiris.cmp` | Mehrteiliges Kampfschiff | Komplexe Fix-Hierarchie |
| `SHIPS/KUSARI/KU_BATTLESHIP/ku_battleship.cmp` | Großes Schlachtschiff | LOD-Testfall |
| `SHIPS/BOUNTY_HUNTER/BH_ELITE/bh_elite.cmp` | Vanilla-Jäger | Basis-Sanity-Check |

### Nach jeder Änderung

1. **Rebuild Infrastructure:**
   ```powershell
   $env:PATH = "C:\Qt\6.8.3\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
   cmake --build build --target flatlas_infrastructure --config Debug
   ```

2. **Probe ausführen** — World Bounds für beide Installationen müssen identisch sein

3. **Vollständigen App-Build:**
   ```powershell
   cmake --build build --target FLAtlas --config Debug
   ```

4. **Visuelle Prüfung** für TESTMOD1 und Crossfire `li_dreadnought.cmp`

### Regressionserkennung

Ein Dekodierungs-Fehler ist wahrscheinlich, wenn:
- World Bounds zwischen TESTMOD1 und Crossfire **nicht übereinstimmen**
- World Bounds D (Tiefe) deutlich kleiner als ~490 ist (fehlende Teile)
- World Bounds D deutlich größer als ~600 ist (falsche Positionen)
- Einzelne Vertices haben Koordinaten mit Betrag < 2.0 in 2 von 3 Achsen (UV-Werte als Positionen)

---

## Lessons Learned aus den behobenen Bugs

### Lesson 1: D3D FVF ist ein numerisches Feld, keine Bitmask

**Problem:** Der UV-Count-Teil des FVF (`bits 8–11`) wird als `(fvf & 0x0F00) >> 8` dekodiert, nicht als einzelne Flags. `0x300` bedeutet 3 UV-Sets, nicht "TEX2 + TEX1".

**Gefährlicher Irrtum:** Jedes Tutorial und jede Doku zeigt `FVF_TEX1 = 0x100`, `FVF_TEX2 = 0x200` usw. Das verleitet zur Annahme, dass es unabhängige Bits sind. Tatsächlich kodieren diese Konstanten den Wert 1 bzw. 2 im numerischen Feld.

**Konsequenz:** Wenn ein zukünftiger Mod FVF `0x412` (4 UV-Sets) oder `0x512` (5 UV-Sets) verwendet und die Bitmask-Logik zurückgebracht wird, bricht das für alle Mods außer denen mit exakt 1, 2, 4 oder 8 UV-Sets.

**Schutz:** `VmeshDecoder::uvSetCount()` ist jetzt mit einem erklärenden Kommentar versehen. Die FVF-Enum-Einträge im Header haben klarstellende Kommentare.

---

### Lesson 2: Rev/Pris-Records überschreiben Fix-Hierarchien

**Problem:** Fix-Records und Rev/Pris-Records verwenden unterschiedliche Schlüssel (Objekt-Name vs. Part-Name), die nach `normalizePartKey()` auf denselben Wert mappen können. Rev/Pris-Records speichern im "parent"-Feld den **Gelenkachsen-Namen**, nicht den Part-Namen → nach Normalisierung ein leerer String.

**Konsequenz:** Ohne die Guard-Logik würde jeder Rev/Pris-Record, der auf denselben normalisierten Schlüssel mappt wie ein Fix-Record, den korrekten Elternnamen mit `""` überschreiben. Das Part wird dann als Root-Level-Orphan behandelt.

**Schutz:** Die Guard-Zeile in `buildCmpTransformHints()` darf **niemals** entfernt werden:
```cpp
if (existing != normalizedParentByPart.cend() && !existing.value().isEmpty() && value.isEmpty())
    continue;
```

---

### Lesson 3: Zwei Installationen gleichzeitig testen

**Problem:** Ein Decoder-Bug kann in einer Installation nicht sichtbar sein (weil die Datei einen FVF/Stride verwendet, bei dem die falsche Logik zufällig korrekte Ergebnisse liefert) und in einer anderen dramatische Artefakte produzieren.

**Konsequenz:** Validierung nur gegen TESTMOD1 ist **unzureichend**. Crossfire enthält Modelle mit anderen FVF-Werten, mehr UV-Sets und teilweise anderen Byte-Layouts.

**Schutz:** Der Diagnostic-Probe lädt immer **beide** Installationen und vergleicht World Bounds. Dieser Test muss nach jeder Änderung an `VmeshDecoder`, `CmpLoader`, oder der Transformationslogik ausgeführt werden.

---

### Lesson 4: World Bounds als Sanity-Check

Absolutkoordinaten einzelner Vertices sind schwer manuell zu validieren. World Bounds (min/max aller transformierten Vertices) sind jedoch robust:
- Ein bekanntes Schiff hat eine bekannte physische Größe
- Wenn D (Tiefe) deutlich falsch ist, fehlen Teile oder sind Koordinaten falsch
- Beide Installationen desselben Modells **müssen** identische Bounds liefern (sie sind dasselbe Schiff)

---

## Geometrie-Decoder-Kaskade: `buildMeshesFromVMesh()`

`buildMeshesFromVMesh()` ist der zentrale Fallback-Mechanismus für die Geometriedekodierung. Sie wird für jeden `VMeshRef` aufgerufen und versucht mehrere Strategien in fester Prioritätsreihenfolge. Die erste Strategie, die valide Geometrie liefert, gewinnt.

### Prioritätskaskade

```
1. structured-mesh-headers     ← Präzisester Pfad (Structured Header-Tabelle vorhanden)
2. exact-fit-windowed           ← Kandidaten-Stride + Bounds-Score
3. decoded                      ← VmeshDecoder::decode() (FVF-basiert)
4. structured-single-block@N   ← Offset-Scan im Block
5. [leeres Ergebnis]            ← Kein Mesh für diesen Ref
```

### Strategie 1: `structured-mesh-headers`

Greift, wenn der VMeshData-Block eine **Mesh-Header-Tabelle** enthält (d.h. `parseStructuredMeshHeaders()` gibt mindestens einen Eintrag zurück). In diesem Fall ruft die Kaskade `decodeStructuredSingleBlockMeshFromHeaders()` auf.

**Block-Layout** (alle Little-Endian):
```
Offset  Typ      Beschreibung
 0      uint16   Unbekannt (Versionskennzeichen)
 2      uint16   Unbekannt
 4      uint16   Unbekannt
 6      uint16   Unbekannt
 8      uint16   meshCount – Anzahl Mesh-Header-Einträge
10      uint16   numRefVertices – Gesamt-Referenz-Vertices (= Dreiecke × 3)
12      uint16   FVF – Flexible Vertex Format (D3D-Codierung)
14      uint16   numVertices – Anzahl tatsächliche Vertex-Definitionen
16      ...      meshCount × 12 Bytes Mesh-Header-Einträge
16 + meshCount*12  ...  Dreiecksdaten (uint16-Triples, 6 Bytes je Dreieck)
nach Dreiecksdaten  ...  Vertex-Daten (nach FVF decodiert)
```

Jeder Mesh-Header-Eintrag (12 Bytes):
```
Offset  Typ      Beschreibung
 0      uint16   startVertex
 2      uint16   endVertex (inklusiv)
 4      uint16   startIndex
 6      uint16   numRefIndices
 8      uint32   Materialidentifikation
```

Die Validierung prüft, ob `expectedVertexTotal == ref.vertexCount` und `expectedIndexTotal == ref.indexCount`. Stimmt beides, ist das Ergebnis deterministisch korrekt – kein Score-Vergleich nötig.

### Strategie 2: `exact-fit-windowed` (Stride-Kandidaten + Bounds-Score)

Greift, wenn keine Header-Tabelle vorhanden ist. `candidateVertexStridesForRef()` liefert eine geordnete Liste möglicher Vertex-Strides:

```
Priorität  Stride           Herkunft
1.         strideHintFromSourceName()   Aus dem Quellnamen des VMeshRefs abgeleitet
2.         12,16,20,24,28,32,36,40,44,48,52,56,60,64,72,80,96,112   Fester Kandidaten-Satz
```

Für jeden Stride wird `decodeExactFitWindowedMesh()` mit `indexSize ∈ {2, 4}` versucht. Der **Bounds-Score** bestimmt den Gewinner:

```cpp
float meshBoundsScore(const MeshData &mesh, const ModelBounds &expectedBounds) {
    // Berechnet: (actualMin - expectedMin)² + (actualMax - expectedMax)² + |actualRadius - expectedRadius|
    // Kleinerer Wert = besser. 0 = perfekte Übereinstimmung.
}
```

Der Score misst die quadratische Abweichung der berechneten Bounds von den im `VMeshRef` gespeicherten erwarteten Bounds. Damit ist der Algorithmus robust gegenüber unbekannten FVF-Werten: Er kann die korrekte Interpretation durch geometrische Plausibilität erkennen.

**Grenzen:** 
- Scheitert, wenn der VMeshRef keine validen Bounds-Daten enthält (`ref.bounds.valid == false`)
- Bei sehr kleinen Meshes (< 12 Bytes Vertex-Daten) gibt `decodeExactFitWindowedMesh()` nullopt zurück

**Layout-Annahme** von `decodeExactFitWindowedMesh()`:
```
Offset 0..15      Block-Header (16 Bytes, übersprungen)
Offset 16..N      Vertex-Daten: (ref.vertexStart + ref.vertexCount) × vertexStride Bytes
Offset N..M       Index-Daten: ab ref.indexStart × indexSize
```

`decodeRawWindowVertices()` verwirft Vertices mit nicht-finiten Koordinaten oder Beträgen > 2 000 000. Damit wird verhindert, dass falsch interpretierte Bytes als Positionen akzeptiert werden.

### Strategie 3: `VmeshDecoder::decode()` (FVF-basiert)

Ruft `VmeshDecoder::decode()` auf, welcher den FVF-Wert aus dem Block-Header liest und `vertexStride()` korrekt berechnet (inkl. UV-Set-Count als Zahl). Schlägt fehl, wenn kein valider FVF-Wert erkannt wird.

### Strategie 4: Offset-Scan via `findStructuredSingleBlockOffsets()`

Letzter Fallback: scannt nach bekannten Byte-Mustern im Block und versucht dort `decodeStructuredSingleBlock()`. Wird selten gebraucht; tritt auf bei ungewöhnlich formatierten VMeshData-Knoten ohne Standard-Header.

### `decodeStructuredFamilyForRef()`: Mehrblocк-Fallback

Wenn `buildMeshesFromVMesh()` kein Ergebnis liefert, ruft `extractPart()` zusätzlich `decodeStructuredFamilyForRef()` auf. Dieser Pfad greift bei Modellen, bei denen die Geometrie über **zwei separate UTF-Knoten** verteilt ist:

- **Header-Block** (`structureKind = "structured-header"`): Enthält Mesh-Header-Tabelle und Dreiecksdaten
- **Stream-Block** (`structureKind = "vertex-stream"`): Enthält nur Vertex-Daten (Positions/Normals/UVs)

Die Zuordnung von Header zu Stream erfolgt über den gemeinsamen `familyKey` (abgeleitet aus dem UTC-Pfad-Präfix beider Blöcke). Gibt es mehrere Header- oder Stream-Blöcke für eine Familie, werden sie per Score sortiert (`scoreStructuredHeaderBlock()`, `scoreStructuredStreamBlock()`). Der Algorithmus versucht alle sinnvollen Header×Stream-Kombinationen und wählt diejenige mit dem niedrigsten `meshBoundsScore` plus semantischer Strafpunkte.

---

## `extractPart()`: Vollständige Part-Dekodierung

`extractPart()` ist der Einstiegspunkt für die Dekodierung eines einzelnen CMP-Parts in einen `ModelNode`. Sie wird einmal pro Part aufgerufen und koordiniert alle Geometrie-Fallbacks.

### Ablauf

```
1. Suche den Part-Knoten im UtfNode-Index nach partPath
2. Lies Transformation aus cmpTransformHints (origin, rotation)
3. Für jeden VMeshRef dieses Parts:
   a. Versuche VMeshDataBlock per matchedSourceName aufzulösen
   b. Falls nicht gefunden: resolveVMeshDataBlockForRef() (CRC32 oder Index)
   c. Schlägt Auflösung fehl: Warning in `warnings` Liste, weiter
   d. Lade blockBytes = raw.mid(dataOffset, usedSize)
   e. Finde PreviewMaterialBinding für diesen Ref
   f. Rufe buildMeshesFromVMesh() auf → Kaskade (Strategien 1–4)
   g. Falls kein Ergebnis: rufe decodeStructuredFamilyForRef() auf
   h. Falls noch kein Ergebnis: decodeMeshVariantsFromBlock() (letzter Brute-Force)
   i. Annotiere alle resultierenden Meshes mit Ref-Metadaten (lodIndex, debugHint)
4. Falls node.meshes nach allen Refs noch leer:
   → Suche direkt nach VMeshData-Knoten unterhalb von partPath
   → decodeAllMeshVariants() auf erstem gefundenen Block
```

### Fallback-Hierarchie für einen einzelnen VMeshRef

| Priorität | Methode | Bedingung |
|---|---|---|
| 1 | `buildMeshesFromVMesh()` Kaskade | Standard-Pfad |
| 2 | `decodeStructuredFamilyForRef()` | Wenn Kaskade leer zurückkehrt |
| 3 | `decodeMeshVariantsFromBlock()` (erste Variante) | Wenn Familie auch fehlschlägt |
| 4 | Direkter VMeshData-Scan | Wenn gar keine Refs Geometrie liefern |

### Ref-Metadaten-Annotation (`annotateMeshWithRefMetadata()`)

Überträgt folgende Felder aus dem `VMeshRefRecord` auf jedes erzeugte `MeshData`:
- `lodIndex` — aus dem `Level`-Segment im UTF-Pfad
- `debugHint` — der verwendete Strategie-Pfad (z.B. `"direct:structured-header:li_dreadnought_lod0.vms/structured-mesh-headers"`)
- Bounds-Metadaten aus `ref.bounds`

### Wann `extractPart()` einen leeren `ModelNode` zurückgibt

- Der Part-Knoten wurde im UTF-Index nicht gefunden
- Kein VMeshRef ist diesem Part zugeordnet (seltener, tritt bei Dummy-Parts auf)
- Alle VMeshRefs scheitern in allen Fallbacks und kein direkter VMeshData-Knoten ist unterhalb des partPath vorhanden

---

## `FreelancerMaterialResolver`: Texturauflösung

`FreelancerMaterialResolver` ist eine statische Utility-Klasse, die Texturnamen aus `MeshData` zu ladbaren `QImage`-Objekten auflöst. Alle Lookup-Ergebnisse werden thread-sicher in statischen `QHash`-Caches gespeichert.

### Auflösungspipeline: `loadTextureForMesh()`

```
1. resolveEmbeddedTextureForMesh()
   → textureCandidatesForMesh() → Kandidatenliste
   → extractUtfEmbeddedTextures(modelPath)  ← gecacht
      → UTF-Baum nach /Texture Library/.../MipN durchsuchen
      → Niedrigste MipLevel-Zahl gewinnt
      → DDS oder TGA decodiert via TextureLoader
   → Versuche auch .mat-Datei wenn materialValue auf .mat endet
   → Gibt erste nicht-null QImage zurück
   
2. Falls kein eingebettetes Bild: resolveTexturePathForMesh()
   → textureCandidatesForMesh() → Kandidatenliste
   → resolveTextureValue() für jeden Kandidaten:
      1. Absoluter Pfad? → direkt prüfen
      2. Relativ zu Modellverzeichnis?
      3. Relativ zu DATA/-Root?
      4. DATA/-Root mit Pfadnormalisierung?
      5. Rekursiver Scan unterhalb DATA/ (QDirIterator)
   → TextureLoader::load(gefundener Pfad)
```

### Kandidatenliste: `textureCandidatesForMesh()`

Die Reihenfolge der Kandidaten entscheidet, welche Textur bevorzugt wird:

```
1. mesh.textureName       (aus PreviewMaterialBinding, direkt)
2. mesh.textureCandidates (aus PreviewMaterialBinding, Liste)
3. materialMap[normalizedKey] aus extractUtfMaterialTextureMap(modelPath)
   → Liest /Material Library/*/dt_name und /et_name Knoten aus dem Modell selbst
4. matMap[normalizedKey] aus .mat-Datei (wenn materialValue auf .mat zeigt)
   Falls .mat nur ein Material enthält: alle dessen Textur-Einträge
```

### `extractUtfMaterialTextureMap()`: Material-Textur-Index aus UTF

Durchsucht den UTF-Baum nach allen `dt_name` und `et_name`-Knoten unterhalb von `/Material Library/`. Gibt eine Map `materialName → [textureName, ...]` zurück. Ergebnis wird pro Dateipfad gecacht.

Schlüssel-Normalisierung: `normalizeMaterialKey()` wandelt Pfade in Dateinamen ohne Erweiterung um, alles lowercase. Damit matched `"FLEAT_EX05"` auf `"fleat_ex05"` und `"EQUIPMENT/fleat_ex05.mat"` auf `"fleat_ex05"`.

### `extractUtfEmbeddedTextures()`: Eingebettete Texturen

Durchsucht `/Texture Library/TextureName/MipN`-Knoten. Pro Texturname wird der niedrigste verfügbare Mip-Level ausgewählt (Mip0 = volle Auflösung). Das Blob wird per `TextureLoader::loadDDSFromData()` oder `loadTGAFromData()` decodiert.

### `findDataRoot()`: Installationsverzeichnis

Steigt im Verzeichnisbaum aufwärts, bis ein Ordner namens `DATA` (case-insensitive) gefunden wird. Gibt den absoluten Pfad dieses `DATA`-Ordners zurück. Ermöglicht mod-unabhängiges Textur-Lookup: Eine Textur `DATA/EQUIPMENT/foo.dds` wird auch dann gefunden, wenn das Modell in `DATA/SHIPS/LIBERTY/...` liegt.

### Bekannte Grenzen

| Situation | Verhalten |
|---|---|
| Textur liegt außerhalb von `DATA/` | Nicht gefunden; Phong-Fallback |
| Mod hat anderen `DATA/`-Pfad als `/DATA/` | Scheitert bei relativer Normalisierung; rekursiver Scan hilft |
| `.mat`-Datei mit mehreren Materialien ohne passenden Key | Scheitert (außer die .mat hat genau ein Material) |
| Textur als `.tga` referenziert, aber nur `.dds` vorhanden | `textureKeysForName()` generiert beide Erweiterungen → gefunden |
| Cache-Invalidierung bei Mod-Wechsel | Cache ist static, lebt für die gesamte App-Laufzeit; Neustart nötig |
