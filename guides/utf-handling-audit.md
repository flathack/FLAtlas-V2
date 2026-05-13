# FLAtlas V2 UTF Handling Audit

```yaml
report_id: flatlas-v2-utf-handling-audit
created: 2026-05-13
scope:
  - UTF container parsing for CMP/3DB/MAT/TXM-style Freelancer assets
  - Material and embedded texture extraction that depends on UTF traversal
  - Test coverage around documented UTF container rules
primary_reference:
  title: The Starport - UTF (Universal Tree Format)
  url: https://the-starport.com/wiki/file-structures/utf/
  accessed: 2026-05-13
local_reference_probe:
  readonly_root: C:/Users/steve/Github/FL-Installationen/Freelancer-HD
  sample_txm: DATA/FX/animated.txm
  sample_txm_first_bytes_hex: "55 54 46 20 01 01 00 00 38 00 00 00 80 05 00 00"
  sample_cmp: DATA/BASES/BRETONIA/br_avalon_bar.cmp
  sample_cmp_first_bytes_hex: "55 54 46 20 01 01 00 00 38 00 00 00 38 33 00 00"
verdict: mostly-aligned-reader-with-known-edge-cases
```

## Reference Facts

```yaml
format:
  byte_order: little-endian
  file_types:
    - .utf
    - .ale
    - .vms
    - .3db
    - .cmp
    - .dfm
    - .sph
    - .txm
    - .mat
header:
  size_bytes: 56
  signature: "UTF "
  version: 0x101
  tree_offset: absolute byte offset
  tree_size: byte size
  entry_size: 44
  names_offset: absolute byte offset
  names_size_allocated: dictionary allocated byte size
  names_size_used: dictionary used byte size
  data_offset: absolute byte offset
  filetime: Windows 64-bit FILETIME
dictionary:
  encoding: ASCIIz
  first_string: empty string at byte 0
entry:
  size_bytes: 44
  next_offset: relative to tree block
  name_offset: relative to dictionary block
  attributes:
    file_bit: 0x80
    folder_bit: 0x10
    note: high bytes may be non-zero in vanilla files
  child_offset_for_folder: relative to tree block
  child_offset_for_file_data: relative to data block
  data_size_used: actual file data byte length
  timestamps: 32-bit DOS date/time values, not operationally important to game
```

## Implementation Map

```yaml
utf_header:
  file: src/infrastructure/io/CmpLoader.cpp
  symbol: CmpLoader::parseUtfHeader
  behavior:
    - reads 56-byte little-endian header
    - maps signature, version, tree/node block, entry size, names block, data block, and timestamp fields
utf_flat_tree:
  file: src/infrastructure/io/CmpLoader.cpp
  symbols:
    - parseFlatUtfNodes
    - buildStringOffsetLookup
    - resolveUtfDataOffset
  behavior:
    - reads entries from header.nodeBlockOffset
    - assumes entries are addressable by index * header.nodeEntrySize relative to tree block
    - treats flags with 0x80 set as data/file nodes
    - resolves file data offset as header.dataOffset + stored child/data offset
    - resolves names as Latin-1 text from the dictionary block
public_tree_api:
  file: src/infrastructure/io/CmpLoader.cpp
  symbol: CmpLoader::parseUtf
  behavior:
    - converts flat node records into UtfNode tree objects
    - creates a synthetic root named "\\"
model_loader:
  file: src/infrastructure/io/CmpLoader.cpp
  symbol: CmpLoader::loadModel
  behavior:
    - parses UTF metadata
    - extracts CMP/3DB parts, VMeshData, VMeshRef, Cmpnd/Fix records, materials, and preview bindings
material_resolver:
  file: src/infrastructure/freelancer/FreelancerMaterialResolver.cpp
  symbols:
    - FreelancerMaterialResolver::extractUtfMaterialTextureMap
    - FreelancerMaterialResolver::extractUtfEmbeddedTextures
  behavior:
    - parses MAT/CMP/TXM-like UTF containers via CmpLoader::parseUtf
    - walks Material library and Texture library nodes
texture_loader:
  file: src/infrastructure/io/TextureLoader.cpp
  symbol: TextureLoader::loadTXM
  behavior:
    - currently tries direct DDS first, then TGA
    - does not parse TXM itself as a UTF container
tests:
  file: tests/test_CmpLoader.cpp
  behavior:
    - covers empty, too-short, bad-signature parseUtf cases
    - covers findNode on manually constructed trees
    - does not build a valid synthetic UTF binary fixture
```

## Compatibility Matrix

```yaml
checks:
  - id: UTF-001
    topic: magic/version/header-size
    reference: Header is 56 bytes, little-endian, signature "UTF ", version usually 0x101.
    implementation_status: aligned
    evidence:
      - CmpLoader.cpp constants kUtfMagic, kUtfHeaderSize, kUtfNodeEntrySize
      - CmpLoader::parseUtfHeader reads the documented sequence in little-endian order
    risk: low

  - id: UTF-002
    topic: absolute block offsets
    reference: treeOffset, namesOffset, and dataOffset are absolute file offsets.
    implementation_status: aligned
    evidence:
      - parseFlatUtfNodes starts entries at header.nodeBlockOffset
      - buildStringOffsetLookup starts dictionary at header.namesOffset
      - resolveUtfDataOffset adds header.dataOffset for file data
    risk: low

  - id: UTF-003
    topic: relative entry links
    reference: nextOffset and folder childOffset are offsets relative to the tree block.
    implementation_status: mostly-aligned
    evidence:
      - peerOffset and folder childOffset are matched against offsetToIndex keys relative to the tree block
    limitation:
      - offsetToIndex only contains index * header.nodeEntrySize for entries read sequentially from treeOffset
      - the reference notes that the game does not require linked entries to be laid out exactly 44 bytes apart
    risk: medium
    impact:
      - non-standard but game-loadable UTF files with sparse, compacted, or irregular entry placement may be partially invisible to FLAtlas

  - id: UTF-004
    topic: file-vs-folder attributes
    reference: 0x80 means file, 0x10 means folder; high bytes may be non-zero.
    implementation_status: aligned-for-file-detection
    evidence:
      - parseFlatUtfNodes uses flags & 0x80 instead of exact equality
      - this tolerates extra high attribute bytes on file entries
    caveat:
      - folder detection is implicit as not-file; unusual attribute combinations are not classified separately
    risk: low

  - id: UTF-005
    topic: dictionary strings
    reference: dictionary is ASCIIz strings, first string empty, empty entry names ignored.
    implementation_status: mostly-aligned
    evidence:
      - buildStringOffsetLookup splits dictionary bytes on NUL and skips empty pieces
      - names are decoded as Latin-1, which is safe for ASCII and tolerant for extended byte values
    caveat:
      - namesSize uses max(allocated, used), so stale bytes in allocated-but-unused dictionary space can be scanned
    risk: low

  - id: UTF-006
    topic: root entry
    reference: tree root entry name should be "\\" but other names are possible.
    implementation_status: partially-aligned
    evidence:
      - parseFlatUtfNodes walks from tree-relative offset 0, so it expects the linked root entry at the start of the tree
      - parseUtf creates an additional synthetic root named "\\"
    limitation:
      - public findNode(root, "VMeshLibrary") can fail on real parsed UTF trees because actual root "\\" may be a child below the synthetic root
      - material resolver recursion still works because it walks through all children
    risk: medium

  - id: UTF-007
    topic: TXM as UTF container
    reference: .txm is one of the UTF container file types.
    implementation_status: split
    evidence:
      - FreelancerMaterialResolver::extractUtfEmbeddedTextures can parse Texture library nodes from any UTF path, including TXM-like files
      - TextureLoader::loadTXM does not parse UTF and only tries direct DDS/TGA
      - local sample DATA/FX/animated.txm begins with "UTF "
    risk: medium
    impact:
      - call sites that use TextureLoader::load("*.txm") directly will not load normal Freelancer TXM libraries
      - call sites that go through FreelancerMaterialResolver embedded texture extraction are more likely to work

  - id: UTF-008
    topic: tests
    reference: correct handling depends on offsets, root, dictionary, and data-block interpretation.
    implementation_status: under-covered
    evidence:
      - tests/test_CmpLoader.cpp validates invalid input and findNode on hand-built nodes
      - no valid binary UTF fixture asserts header, dictionary, relative tree offsets, data offsets, or real root traversal
    risk: medium
```

## Findings

```yaml
findings:
  - id: F-001
    severity: medium
    title: parseUtf creates a synthetic root above the documented UTF root
    affected:
      - src/infrastructure/io/CmpLoader.cpp
      - src/infrastructure/freelancer/FreelancerMaterialResolver.cpp
    detail: >
      The documented tree root is itself an entry, usually named "\\". FLAtlas creates another root named "\\"
      and attaches the file's real root entry under it. Deep recursive walks still see all nodes, but path-based
      lookups from the public parseUtf/findNode API can miss common paths unless the caller accounts for the
      extra root level.
    suggested_fix: >
      Normalize parseUtf so the documented root entry becomes the returned root, or teach findNode to skip an
      initial parsed "\\" child when resolving paths. Add a valid UTF fixture where findNode(root, "Material library")
      and findNode(root, "\\Material library") both behave intentionally.

  - id: F-002
    severity: medium
    title: Tree-link resolver assumes regular entry grid
    affected:
      - src/infrastructure/io/CmpLoader.cpp
    detail: >
      The parser reads nodeCount as treeSize / entrySize and builds tree-relative entry addresses as
      index * entrySize. The reference says the game follows linked offsets and does not require all entries
      to be laid out exactly as a dense 44-byte table. FLAtlas is correct for conventional Freelancer files,
      but not for every documented legal layout.
    suggested_fix: >
      Keep the current fast path for normal files, but add a linked-entry reader that starts at treeOffset and
      follows peer/child offsets directly, validating that each referenced entry has 44 readable bytes.

  - id: F-003
    severity: medium
    title: TextureLoader::loadTXM does not parse normal UTF-backed TXM files
    affected:
      - src/infrastructure/io/TextureLoader.cpp
      - src/infrastructure/freelancer/FreelancerMaterialResolver.cpp
    detail: >
      Starport classifies .txm as a UTF container. The local Freelancer-HD sample DATA/FX/animated.txm confirms
      this with "UTF " header bytes. TextureLoader::loadTXM currently treats TXM as direct DDS/TGA data, while
      embedded UTF texture extraction exists in FreelancerMaterialResolver.
    suggested_fix: >
      Either document TextureLoader::loadTXM as only handling renamed DDS/TGA blobs, or route TXM through
      CmpLoader::parseUtf and Texture library extraction when the file starts with "UTF ".

  - id: F-004
    severity: medium
    title: No positive binary UTF parser tests
    affected:
      - tests/test_CmpLoader.cpp
    detail: >
      Current tests cover invalid input and manual tree lookup, but do not assert a valid UTF container with
      dictionary, root, child folders, file nodes, relative child/peer offsets, and data payload extraction.
    suggested_fix: >
      Add a tiny in-memory UTF builder fixture to test_CmpLoader.cpp. Cover one folder, one data node, a sibling,
      and an absolute dataOffset with relative file data offsets.
```

## Recommended Next Steps

```yaml
next_steps:
  - priority: 1
    task: Add a valid synthetic UTF fixture test.
    reason: It locks down the documented offset model before changing parser behavior.
    suggested_target: tests/test_CmpLoader.cpp

  - priority: 2
    task: Decide the public parseUtf root contract.
    reason: It affects findNode callers and material/texture traversal semantics.
    suggested_target: src/infrastructure/io/CmpLoader.cpp

  - priority: 3
    task: Add UTF-aware TXM loading or rename/document the current loadTXM semantics.
    reason: Normal Freelancer TXM files are UTF containers, not plain image files.
    suggested_target:
      - src/infrastructure/io/TextureLoader.cpp
      - src/infrastructure/freelancer/FreelancerMaterialResolver.cpp

  - priority: 4
    task: Consider a linked-offset parser for irregular but game-loadable UTF trees.
    reason: Current dense-table parsing is likely fine for vanilla assets but narrower than the documented format.
    suggested_target: src/infrastructure/io/CmpLoader.cpp
```

## Agent Notes

```yaml
do_not_change:
  - local Freelancer-HD reference files
  - generated or ignored build artifacts
safe_assumptions:
  - Most vanilla CMP/3DB/MAT files use conventional dense 44-byte entry records.
  - Existing model preview behavior may be acceptable despite parseUtf root quirks because loadModel mainly uses flatNodes.
  - Embedded texture resolution is already stronger than TextureLoader::loadTXM for Material/Texture library workflows.
validation_commands:
  after_parser_changes:
    - powershell: "$env:PATH='C:\\Qt\\6.8.3\\mingw_64\\bin;C:\\Qt\\Tools\\mingw1310_64\\bin;' + $env:PATH; cmake --build build --target test_CmpLoader"
    - powershell: "$env:PATH='C:\\Qt\\6.8.3\\mingw_64\\bin;C:\\Qt\\Tools\\mingw1310_64\\bin;' + $env:PATH; ctest --test-dir build -R test_CmpLoader --output-on-failure"
  after_txm_changes:
    - powershell: "$env:PATH='C:\\Qt\\6.8.3\\mingw_64\\bin;C:\\Qt\\Tools\\mingw1310_64\\bin;' + $env:PATH; cmake --build build --target test_TextureLoader"
    - powershell: "$env:PATH='C:\\Qt\\6.8.3\\mingw_64\\bin;C:\\Qt\\Tools\\mingw1310_64\\bin;' + $env:PATH; ctest --test-dir build -R test_TextureLoader --output-on-failure"
```
