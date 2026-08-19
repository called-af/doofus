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
  // Far plane aligned with active LOD distance
  static inline float farPlane = 2000.0f;

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

  // Full-detail chunk render distance.
  static inline int renderDistance = 12;

  // ======================
  // LOD SYSTEM (Level 1 to 5)
  // ======================

  // Maximum active LOD level:
  //   0 = Disabled (only regular chunks)
  //   1 = LOD 1 only (~256m)
  //   2 = LOD 1 & 2 (voxel blocky, ~352m)
  //   3 = LOD 1, 2 & 3 (analytical multi-span, ~512m, recommended default)
  //   4 = Up to LOD 4 (~768m)
  //   5 = Up to LOD 5 (~1088m)
  static inline int maxLODLevel = 3;

  // Ring distance offsets in chunks (relative to renderDistance).
  // Kept moderate and close for high FPS, fast loading, and immediate visual clarity.
  static inline int lod1Distance = 4;   // LOD 1: rd + 4  chunks (~256 blocks)
  static inline int lod2Distance = 10;  // LOD 2: rd + 10 chunks (~352 blocks)
  static inline int lod3Distance = 20;  // LOD 3: rd + 20 chunks (~512 blocks)
  static inline int lod4Distance = 36;  // LOD 4: rd + 36 chunks (~768 blocks)
  static inline int lod5Distance = 56;  // LOD 5: rd + 56 chunks (~1088 blocks)

  static inline int getLODMaxChunkDistance(int level)
  {
      switch (level)
      {
      case 1: return renderDistance + lod1Distance;
      case 2: return renderDistance + lod2Distance;
      case 3: return renderDistance + lod3Distance;
      case 4: return renderDistance + lod4Distance;
      case 5: return renderDistance + lod5Distance;
      default: return renderDistance;
      }
  }

  static inline int getLODMinChunkDistance(int level)
  {
      switch (level)
      {
      case 1: return renderDistance;
      case 2: return renderDistance + lod1Distance;
      case 3: return renderDistance + lod2Distance;
      case 4: return renderDistance + lod3Distance;
      case 5: return renderDistance + lod4Distance;
      default: return renderDistance;
      }
  }

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

  static inline int shadowDistance = 14;
  static inline bool enableShadows = true;

  // 2048 gives crisp, accurate high-fidelity shadows for player, entities, and blocks
  static int shadowMapSize() { return 2048; }

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

  static constexpr float plateauScale = 0.0009f;
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

  // Fog start and end aligned with active LOD distances for smooth horizon fading
  static inline float fogStart = 350.0f;
  static inline float fogEnd = 550.0f;

  static inline float getFogEnd()
  {
      int maxChunks = (maxLODLevel > 0) ? getLODMaxChunkDistance(std::min(5, maxLODLevel)) : renderDistance;
      return static_cast<float>(maxChunks * 16);
  }

  static inline float getFogStart()
  {
      return getFogEnd() * 0.70f;
  }

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed = 100.0f;
  static inline float jumpForce = 80.0f;
  static inline float gravity = -20.0f;
  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};
