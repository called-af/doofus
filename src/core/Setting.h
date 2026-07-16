// ==============================
// Setting.h
// Global engine settings
// ==============================

#pragma once
#include <string>

class Setting {
public:
  // ======================
  // WINDOW
  // ======================

  static inline int windowWidth  = 1280;
  static inline int windowHeight = 720;

  static inline bool fullscreen = false;
  static inline bool vsync      = false;

  // ======================
  // FONT
  // ======================

  static inline std::string fontPath =
      "assets/fonts/JetBrains_Mono,Space_Grotesk/JetBrains_Mono/static/"
      "JetBrainsMono-Regular.ttf";

  static inline int fontSize = 20;

  // ======================
  // CAMERA
  // ======================

  static inline float fov = 90.0f;

  // Small near plane so close objects are not clipped
  static inline float nearPlane = 0.1f;
  // Far plane large enough to cover the entire LOD5 ring
  static inline float farPlane = 11000.0f;

  static inline float mouseSensitivity = 0.1f;

  static inline float cameraEyeHeight = 1.7f;

  static inline float defaultYaw   = -90.0f;
  static inline float defaultPitch =   0.0f;

  static inline float spawnX = 0.0f;
  static inline float spawnY = 80.0f;
  static inline float spawnZ = 0.0f;

  // ======================
  // WORLD
  // ======================

  // Full-detail chunk render distance. Must equal lod1Start.
  static inline int renderDistance = 12;

  // ─────────────────────────────────────────────────────────────────────────
  //  5-Level LOD System
  //
  //  Each level doubles the sampling step relative to the previous level:
  //    LOD1 → step=2  (2×2  chunks per tile, ~32 blocks per cell)
  //    LOD2 → step=4  (4×4  chunks per tile, ~64 blocks per cell)
  //    LOD3 → step=8  (8×8  chunks per tile, ~128 blocks per cell)
  //    LOD4 → step=16 (16×16 chunks per tile, ~256 blocks per cell)
  //    LOD5 → step=32 (32×32 chunks per tile, ~512 blocks per cell)
  //
  //  Critical rules:
  //    lod1Start MUST == renderDistance
  //    lodNStart MUST == lod(N-1)End  (no gaps or overlaps allowed)
  // ─────────────────────────────────────────────────────────────────────────

  // LOD1: high resolution, nearest ring (2×2 chunks per tile)
  static inline int lod1Start = 12;
  static inline int lod1End   = 36;

  // LOD2: medium resolution (4×4 chunks per tile)
  static inline int lod2Start = 36;
  static inline int lod2End   = 72;

  // LOD3: low resolution (8×8 chunks per tile)
  static inline int lod3Start = 72;
  static inline int lod3End   = 144;

  // LOD4: very low resolution (16×16 chunks per tile)
  static inline int lod4Start = 144;
  static inline int lod4End   = 288;

  // LOD5: ultra low resolution, very far distance (32×32 chunks per tile)
  static inline int lod5Start = 288;
  static inline int lod5End   = 576;

  // Total LOD render distance — must equal lod5End
  static inline int lodRenderDistance = 576;

  // ─────────────────────────────────────────────────────────────────────────
  //  Worker threads
  //
  //  Thread count is capped to avoid overloading the CPU while the game runs.
  //  Optimal value: (physical CPU count - 1), minimum 2.
  //  This value is read by ChunkWorker during initialisation.
  // ─────────────────────────────────────────────────────────────────────────
  // 0 = auto (up to four workers, leaving CPU time for render/main thread).
  static inline int maxWorkerThreads = 0;

  // Maximum chunk meshes dispatched per frame (prevents hitching)
  static inline int maxMeshDispatchPerFrame = 6;

  // ======================
  // SHADOW
  // ======================

  static inline int  shadowDistance  = 8;
  static inline bool enableShadows   = true;

  // 512 keeps the near shadow stable while reducing depth-pass fill cost by
  // 75% compared with 1024; PCF masks the lower resolution well.
  static int shadowMapSize() { return 512; }

  // ======================
  // WORLD HEIGHT
  // ======================

  static constexpr int worldHeight = 256;

  // ======================
  // SEED
  // ======================

  static inline int seed = 1233;

  static inline float daySpeed = 1.0f;

  // ======================
  // BIOME
  // ======================

  static inline float biomeScale    = 0.005f;
  static inline float plainsChance  = 0.5f;

  // ======================
  // TERRAIN
  // ======================

  static inline int   baseTerrainHeight = 12;
  static inline float terrainScale      = 0.008f;
  static inline int   terrainAmplitude  = 90;

  // ======================
  // MOUNTAIN
  // ======================

  static inline float mountainScale  = 0.0002f;
  static inline int   mountainHeight = 130;

  // ======================
  // CLIMATE
  // ======================

  static inline float temperatureScale = 0.0015f;
  static inline float humidityScale    = 0.0015f;

  // ======================
  // BIOME RULES
  // ======================

  static inline float mountainThreshold      = 0.74f;
  static constexpr float mountainCoreThreshold = 0.88f;

  static inline float desertTemperature = 0.55f;
  static inline float desertHumidity    = 0.45f;

  // ======================
  // CONTINENTALNESS
  // ======================

  static inline float continentalScale  = 0.0018f;
  static inline int   continentalHeight = 8;

  // ======================
  // PEAKS & VALLEYS
  // ======================

  static inline float peaksScale = 0.010f;
  static inline int   peakHeight = 60;

  // ======================
  // EROSION
  // ======================

  static inline float erosionScale    = 0.003f;
  static inline float erosionStrength = 5.0f;

  // ======================
  // RIVERS
  // ======================

  static inline float riverScale     = 0.004f;
  static inline float riverThreshold = 0.015f;
  static inline int   riverDepth     = 20;

  // ======================
  // CAVES
  // ======================

  static inline float caveScale     = 0.0009f;
  static inline float caveThreshold = 0.1f;
  static inline int   caveMinY      = 5;
  static inline int   caveMaxY      = 90;

  // ======================
  // PILLARS
  // ======================

  static constexpr float pillarScale     = 0.010f;
  static constexpr float pillarThreshold = 0.45f;

  // ─────────────────────────────────────────────────────────────────────────
  //  PLATEAU — primary control for island shape
  // ─────────────────────────────────────────────────────────────────────────

  static constexpr float plateauScale     = 0.0012f;
  // Low threshold so the island is wide from the start
  static constexpr float plateauThreshold = 0.52f;

  static constexpr int plateauBaseHeight = 85;
  static constexpr int plateauHeight     = 45;

  // Terraces
  static constexpr int terraceCount  = 3;
  static constexpr int terraceHeight = 12;

  // Pillars
  static constexpr int pillarMinHeight = 20;

  // Cliffs
  static constexpr float cliffErosionScale = 0.015f;
  static constexpr float cliffErosionStr   = 5.0f;

  // Valleys
  static constexpr float valleyDepth = 45.0f;

  // ======================
  // STALACTITES & STALAGMITES
  // ======================

  // How frequently spike clusters appear (noise scale)
  static inline float spikeNoiseScale    = 1.2f;
  static inline float spikeSpawnThreshold = 0.01f;

  static inline float spikeDepthFalloff          = 1.0f;
  static inline float spikeDepthCutoff           = 0.0f;
  static inline float spikeDepthThicknessMult    = 0.65f;
  static inline float spikeTaperCurve            = 0.60f;
  // High exponent makes the spike body rigid downward like a pillar
  static inline float spikeTaperExponent         = 4.5f;

  // Maximum length of stalactites & stalagmites
  static inline float stalactiteMaxLen = 65.0f;
  static inline float stalagmiteMaxLen = 55.0f;

  // Island
  static constexpr float islandCorePillarThreshold = 0.40f;
  static constexpr float islandEdgeCutoff          = 0.12f;
  static constexpr int   islandFloorGuard          = 22;

  // ======================
  // FOG
  // ======================

  // fogEnd is aligned with farPlane so fog covers the far render boundary of LOD5
  static inline float fogStart = 5000.0f;
  static inline float fogEnd   = 9000.0f;

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed     = 40.0f;
  static inline float jumpForce     = 70.0f;
  static inline float gravity       = -20.0f;
  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};
