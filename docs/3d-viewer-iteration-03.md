# 3D Viewer Iteration 03

## Referenzmodell
- Neuer harter Referenzfall: `DATA/SOLAR/DOCKABLE/docking_ringx2_lod.cmp`
- Bestehende Referenzen bleiben:
  - `cv_starflier.cmp`
  - `space_dome.cmp`
  - `TLR_lod.3db`

## Ziel
Den `structured-family`-Fallback enger an den V1-Approach ziehen, indem Family-Blöcke nicht mehr nur über die grobe `familyKey`-Menge ausgewählt werden, sondern zusätzlich über die zum `VMeshRef` passenden `source_name`-Kandidaten.

## Implementierter Schritt
- `source_name`- und `familyKey`-normalisierte Blockvergleiche im Decoder
- source-name-basierte Vorfilterung innerhalb des Family-Fallbacks
- gruppenbezogene Header-/Stream-Bewertung über
  - `groupStart/groupCount`
  - `source_vertex_end`
  - `source_index_end`
- V1-nähere Pairing-Priorität:
  - `mesh-header-end-ranges-and-group-match-source`
  - dann `mesh-header-end-ranges-match-source`
  - Kapazitätsprüfung für Header-/Stream-Paare vor dem finalen Bounds-Fit
- explizite Plan-Auswahl pro `VMeshRef`
  - bevorzugter Split-Plan (`header + stream`)
  - bevorzugter Single-Block-Plan
  - keine lose Gleichrangigkeit mehr aller Family-Kandidaten
- `decode_ready`-Verhalten V1-näher:
  - gute Pläne zuerst exklusiv versuchen
  - nur bei echtem Decode-Fehlschlag auf schwächere Pläne zurückfallen
  - aktiver Plan landet jetzt im Warning-Text des Family-Fallbacks
- neuer Dockable-Solar-Regressionstest für `docking_ringx2_lod.cmp`

## Erwarteter Effekt
- weniger falsche Header-/Stream-Blockkombinationen in größeren Dockable-CMPs
- stabilere, V1-nähere Auswahl des richtigen Family-Blocksets pro `VMeshRef`

## Nächster Block
Wenn `docking_ringx2_lod.cmp` damit noch sichtbar falsch ist, ist der nächste Schritt die noch engere V1-Annäherung bei `decode_ready`/Fallback-Verhalten:
- gute Pläne zuerst exklusiv versuchen
- nur bei echtem Decode-Fehlschlag auf schwächere Pläne zurückfallen
- den aktiven Plan pro Ref für Tests/Debug sichtbar machen
