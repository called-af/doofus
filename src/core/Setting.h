// ==============================
// Setting.h
// Global engine settings
// ==============================

#pragma once
#include <string>

class Setting
{
public:
  // ======================
  // WINDOW
  // ======================

  static inline int windowWidth = 1280;
  static inline int windowHeight = 720;

  static inline bool fullscreen = false;
  static inline bool vsync = false;

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

  static inline float defaultYaw = -90.0f;
  static inline float defaultPitch = 0.0f;

  static inline float spawnX = 0.0f;
  static inline float spawnY = 80.0f;
  static inline float spawnZ = 0.0f;

  // ======================
  // WORLD
  // ======================

  // Full-detail chunk render distance. Must equal lod1Start.
  static inline int renderDistance = 12;

  // Flat baseline height while TerrainGenerator is being rebuilt from
  // scratch. Used by computeHeight() until real noise-based shaping
  // (continentalness, plateau, etc.) is added back in.
  static inline int flatHeight = 64;

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
  static inline int lod1End = 36;

  // LOD2: medium resolution (4×4 chunks per tile)
  static inline int lod2Start = 36;
  static inline int lod2End = 100;

  // LOD3: low resolution (8×8 chunks per tile)
  static inline int lod3Start = 100;
  static inline int lod3End = 600;

  // LOD4: very low resolution (16×16 chunks per tile)
  static inline int lod4Start = 600;
  static inline int lod4End = 700;

  // LOD5: ultra low resolution, very far distance (32×32 chunks per tile)
  static inline int lod5Start = 700;
  static inline int lod5End = 800;

  // Total LOD render distance — must equal lod5End
  static inline int lodRenderDistance = 800;

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

  static inline int shadowDistance = 8;
  static inline bool enableShadows = true;

  // 512 keeps the near shadow stable while reducing depth-pass fill cost by
  // 75% compared with 1024; PCF masks the lower resolution well.
  static int shadowMapSize() { return 512; }

  // ======================
  // WORLD HEIGHT
  // ======================

  static constexpr int worldHeight = 480;

  // ======================
  // SEED
  // ======================

  static inline int seed = 1233;

  static inline float daySpeed = 1.0f;

  // ======================
  // BIOME
  // ======================

  static inline float biomeScale = 0.005f;
  static inline float plainsChance = 0.5f;

  // ======================
  // TERRAIN
  // ======================

  static inline int baseTerrainHeight = 12;
  static inline float terrainScale = 0.008f;
  static inline int terrainAmplitude = 90;

  // ======================
  // MOUNTAIN
  // ======================

  static inline float mountainScale = 0.0002f;
  static inline int mountainHeight = 130;

  // ======================
  // CLIMATE
  // ======================

  static inline float temperatureScale = 0.0015f;
  static inline float humidityScale = 0.0015f;

  // ======================
  // BIOME RULES
  // ======================

  static inline float mountainThreshold = 0.74f;
  static constexpr float mountainCoreThreshold = 0.0088f;

  static inline float desertTemperature = 0.55f;
  static inline float desertHumidity = 0.45f;

  // ======================
  // CONTINENTALNESS
  // ======================

  static inline float continentalScale = 0.0018f;
  static inline int continentalHeight = 8;

  // ======================
  // PEAKS & VALLEYS
  // ======================

  static inline float peaksScale = 0.010f;
  static inline int peakHeight = 60;

  // ======================
  // EROSION
  // ======================

  static inline float erosionScale = 0.003f;
  static inline float erosionStrength = 5.0f;

  // ======================
  // RIVERS
  // ======================

  static inline float riverScale = 0.004f;
  static inline float riverThreshold = 0.015f;
  static inline int riverDepth = 20;

  // ======================
  // CAVES
  // ======================

  static inline float caveScale = 0.0009f;
  static inline float caveThreshold = 0.1f;
  static inline int caveMinY = 5;
  static inline int caveMaxY = 90;

  // ======================
  // PILLARS
  // ======================

  static constexpr float pillarScale = 0.000010f;
  static constexpr float pillarThreshold = 0.00045f;

  // ─────────────────────────────────────────────────────────────────────────
  //  PLATEAU — primary control for island shape
  // ─────────────────────────────────────────────────────────────────────────

  static constexpr float plateauScale = 0.0012f;
  // Low threshold so the island is wide from the start
  static constexpr float plateauThreshold = 0.52f;

  static constexpr int plateauBaseHeight = 85;
  static constexpr int plateauHeight = 45;

  // Terraces
  static constexpr int terraceCount = 3;
  static constexpr int terraceHeight = 12;

  // Pillars
  static constexpr int pillarMinHeight = 20;

  // Cliffs
  static constexpr float cliffErosionScale = 0.015f;
  static constexpr float cliffErosionStr = 5.0f;

  // Valleys
  static constexpr float valleyDepth = 45.0f;

  // Island
  static constexpr float islandCorePillarThreshold = 0.40f;
  static constexpr float islandEdgeCutoff = 0.12f;
  static constexpr int islandFloorGuard = 22;

  // ======================
  // ISLAND (cellular placement)
  // ======================

  static constexpr float islandCellScale = 0.006f;

  static constexpr float islandEdgeRadius = 0.55f;

  static constexpr float islandMinRadiusScale = 0.5f; 
  static constexpr float islandMaxRadiusScale = 1.8f;  

  static constexpr int islandMinTopHeight = 70;
  static constexpr int islandMaxTopHeight = 130;
  static constexpr int islandMinThickness = 8;
  static constexpr int islandMaxThickness = 40;

  static constexpr float islandMountainScale = 0.02f;
  static constexpr int islandMountainMaxHeight = 60;
  static constexpr float islandMountainCellIdGate = 0.6f;

  static constexpr float islandWarpScale = 0.003f;
  static constexpr float islandWarpStrength = 40.0f;

  static constexpr float pillarMinSpawnStrength = 0.18f;

  // ======================
  // 3-TIER VERTICAL REALM (Heaven / Normal / Hell)
  // ======================

  // Tier 3 — Heaven (Heaven Archipelago Cluster Floating Realm — Rare, Super High Archipelagos)
  static constexpr float heavenClusterSpacing  = 420.0f;  // Spacing between archipelago cluster centers (blocks)
  static constexpr float heavenGridSize        = heavenClusterSpacing; // Alias for backward compatibility
  static constexpr float heavenMinBaseY        = 320.0f;  // Base Y altitude for floating heaven realm
  static constexpr float heavenMaxBaseY        = 410.0f;  // Max Y altitude for floating heaven realm
  static constexpr float heavenSpawnChance     = 0.45f;   // Spawn probability per grid cell (45%)

  // Size categories for Heaven Islands (Giant, Medium, Small)
  static constexpr float heavenGiantRadiusMin  = 90.0f;
  static constexpr float heavenGiantRadiusMax  = 135.0f;
  static constexpr float heavenMediumRadiusMin = 40.0f;
  static constexpr float heavenMediumRadiusMax = 75.0f;
  static constexpr float heavenSmallRadiusMin  = 12.0f;
  static constexpr float heavenSmallRadiusMax  = 28.0f;

  // Silhouette, Plateau & Underbelly Tapering Parameters
  static constexpr float heavenPlateauRatio    = 0.55f;   // Inner buildable plateau ratio before edge rim slope
  static constexpr float heavenBellyExponent   = 1.10f;   // Inverted cone tapering exponent (cone terbalik)
  static constexpr float heavenWarpScale       = 0.0035f; // Domain warp frequency for organic coastlines
  static constexpr float heavenWarpStrength    = 0.75f;   // Domain warp intensity factor per column (high variation, non-circular)
  static constexpr float heavenAnchorPeakHeight = 35.0f;  // Landmark mountain peak / crystal spire height on anchor islands
  static constexpr float heavenSubIsletYMin     = -35.0f;  // Min Y elevation offset for satellite islets
  static constexpr float heavenSubIsletYMax     = 10.0f;   // Max Y elevation offset for satellite islets

  // Tier 1 — Hell (Bottom Fiery Underworld & Canyon Realm)
  static constexpr float hellCanyonWidth = 95.0f;
  static constexpr float hellSpineWarpScale1 = 0.002f;
  static constexpr float hellSpineWarpScale2 = 0.015f;
  static constexpr int hellCanyonFloorY = 8;
  static constexpr int hellCanyonRimY = 45;
  static constexpr float hellLavaThreshold = 0.55f;
  static constexpr float hellObsidianThreshold = 0.38f;
  static constexpr float hellSpikeNoiseScale = 0.035f;

  // ======================
  // FOG
  // ======================

  // fogEnd is aligned with farPlane so fog covers the far render boundary of LOD5
  static inline float fogStart = 5000.0f;
  static inline float fogEnd = 9000.0f;

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed = 500.0f;
  static inline float jumpForce = 100.0f;
  static inline float gravity = -20.0f;
  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};
