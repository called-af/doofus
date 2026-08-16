# Task: LOD System untuk Terrain 3-Tier (doofus)

## Context

Terrain generator (`TerrainGenerator.cpp`) menghasilkan world dengan **3 layer yang mostly-terpisah secara vertikal per kolom**, bukan single-surface heightmap:

1. **Tier 1 — Hell Floor** (Y 0 → `baseFloorY`): solid dari dasar, monoton, heightmap-friendly.
2. **Tier 2 — Continent**: plateau body floating, nyambung ke Hell Floor lewat hourglass stem tipis & hollow (`waistRatio 0.32`). Ada gap udara antara `baseFloorY` dan `bodyBottom` di banyak kolom.
3. **Tier 3 — Heaven Islands** (Y 280–480): floating penuh, underbelly cone, gak nyambung apapun di bawah.
4. **(Backlog)** Tier 2 thin pillar variant — pillar tinggi-kurus terpisah (feature-seed based, mirip `generateHeavenSeed` tapi versi Tier 2). Belum diimplementasi, dibahas terpisah.

Single-value heightmap akan menghilangkan efek floating di LOD3+ (continent & heaven island jadi keliatan solid nyambung ke tanah). LOD harus pakai **multi-span** approach.

## Known Gaps Sebelum Mulai

- [x] `ChunkWorker::run()` — job `hasLOD` di-dequeue tapi **tidak pernah diproses** (tidak ada blok `if (hasLOD) {...}` yang build mesh & push ke `finishedLODMeshes`). LOD job masuk antrian tapi hasil gak pernah keluar.
- [x] `World.cpp` — fungsi-fungsi LOD yang dideklarasikan di `World.h` (`updateLOD`, `requestLODTile`, `drawLOD`, `inLODRing`, `chunkToTile`, `getLODKey`) **belum ada implementasinya sama sekali** di `World.cpp` (0 hasil grep "lod").
- [x] Height formula Tier 2 (plateau height calc) dan `estimateBodyBottom()` masih nempel private di `TerrainGenerator::generateBaseTerrain` — belum reusable buat LODMesher.

---

## Phase 0 — Perbaikan Fondasi

- [x] Tambah blok pemrosesan `if (hasLOD)` di `ChunkWorker::run()`: panggil `LODMesher::build(...)`, isi `LODMeshResult`, push ke `finishedLODMeshes` (skip kalau `lodReq.generation != lodGeneration.load()`, sama pola kayak mesh/terrain job).
- [x] Refactor `TerrainGenerator`: jadikan public static helper baru
  - `sampleContinentHeightAt(worldX, worldZ) -> int` (ekstrak logic Tier 2 height dari `generateBaseTerrain`)
  - `sampleContinentBodyBottomAt(worldX, worldZ) -> int` (wrap `estimateBodyBottom`, publicly callable tanpa Chunk)
  - `sampleHellFloorAt(worldX, worldZ) -> int` (ekstrak `baseFloorY` calc + canyon blend, tanpa perlu generate chunk penuh)
  - Fungsi-fungsi ini harus **standalone** (gak butuh `Chunk&`), sama pola kayak `sampleHeightAt` / `evaluateIslandSlice` yang udah ada buat Heaven tier.

## Phase 1 — Data Structures

- [x] Definisikan `struct LODSpan { int topY; int bottomY; BlockType surfaceType; bool needsBottomCap; bool needsSkirt; };`
- [x] Definisikan `struct LODColumn { std::vector<LODSpan> spans; };` — hasil scan 1 kolom XZ pada suatu LOD level.
- [x] Tentukan mapping level → stride & metode:
  | Level | Chunk/tile | XZ stride | Metode |
  |---|---|---|---|
  | 1 | 1 | 1 block | Voxel blocky (`isSolidAt`, full res) |
  | 2 | 2 | 2 block | Voxel blocky (`isSolidAt`, half res) |
  | 3 | 4 | 4 block | Analytical multi-span |
  | 4 | 8 | 8 block | Analytical multi-span (simplified) |
  | 5 | 16 | 16 block | Analytical multi-span (flat cap, no peak/crystal detail) |

## Phase 2 — LOD1–2: Voxel Blocky Mesher

- [x] Implement scan pakai `solidQuery` (`isSolidAt`) callback dari `LODMeshRequest`, stride sesuai level.
- [x] Reuse pola greedy-merge dari `GreedyMesher.cpp` kalau memungkinkan (merge quad sejenis) supaya vertex count gak meledak di LOD2.
- [x] Pastikan hourglass stem & underbelly cone Heaven ke-capture natural karena basisnya solid-query 3D, bukan heightmap.

## Phase 3 — LOD3–5: Analytical Multi-Span Mesher

- [x] Untuk tiap sample point (XZ, stride sesuai level):
  1. Span Hell Floor: `bottomY = 0`, `topY = sampleHellFloorAt(...)`. Gak perlu skirt bawah (nempel dasar dunia).
  2. Span Continent (kalau `pDepth >= islandEdgeCutoff`): `topY = sampleContinentHeightAt(...)`, `bottomY = sampleContinentBodyBottomAt(...)`. **Skip hourglass stem** — jangan coba render stem-nya, biar span ini keliatan sebagai slab floating yang bersih (stem terlalu tipis buat keliatan di LOD3+, dan rawan flicker antar level kalau dipaksa).
  3. Span Heaven (kalau `heavenSeed.exists` dekat titik ini): pakai `findNearestHeavenSeed()` + `evaluateIslandSlice()` langsung (sudah standalone & presisi float, gak perlu voxel scan).
- [x] Tiap span floating (Continent, Heaven) dapat: top cap quad, bottom cap quad, full skirt di sisi yang exposed.
- [x] Terapkan `kMaxWallDrop` clamp buat batasin skirt height (biar gak nembak jauh ke bawah pas ada lonjakan noise antar sample point yang berdekatan).
- [x] Level 4–5: matiin detail Heaven peak/crystal (`isAnchorPeak`), pakai flat top/bottom saja untuk hemat triangle.

## Phase 4 — Wiring ke World.cpp

- [x] Implement `World::updateLOD()` — iterasi ring tile per level pakai `inLODRing()` + `chunkToTile()` yang formula-nya udah ada di `World.h`.
- [x] Implement `World::requestLODTile()` — build `LODMeshRequest` dgn callback `blockQuery`/`heightQuery`/`solidQuery` yang bind ke `World::isSolid` / `World::getHeight`.
- [x] Implement `World::drawLOD()` — render `finishedLODMeshes`, uniform `uIsLOD=1` (shader-nya kayaknya udah siap, cek `uIsLODLoc` cache di `World.h`).
- [x] Tangani `lodGeneration` bump saat player pindah jarak jauh (biar tile lama di-discard, sama pola `flushFinished()` yang udah ada di `ChunkWorker`).

## Phase 5 — Validasi & Tuning

- [x] Test transisi LOD2 → LOD3: pastikan continent/heaven gak "pop" jadi solid pas masuk analytical span (bandingkan siluet).
- [x] Test area canyon (Tier 1 heavy): pastikan gak ada regresi, karena tier ini paling heightmap-friendly dari awal.
- [x] Test area dgn banyak Heaven cluster berdekatan: cek performa `findNearestHeavenSeed()` dipanggil per sample — kalau lambat, cache per-tile.
- [x] Cek triangle count per tile di tiap level, target budget berapa (isi sendiri sesuai target device).

## Backlog (Belum Prioritas)

- [ ] Tier 2 thin pillar variant (feature-seed based, terpisah dari continent) — perlu span type baru + LOD handling sendiri kalau jadi diimplementasi.