// ==============================
// Setting.h
// ==============================

#pragma once
#include <string>

class Setting {
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

  static inline float nearPlane = 0.1f;
  static inline float farPlane = 8000.0f;

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

  static inline int renderDistance = 24;

  // LOD ring distances (in chunk units, measured from player chunk center).
  // LOD1 = 2x downsampled  (4 source chunks → 1 LOD chunk, covers 2×2 chunks)
  // LOD2 = 4x downsampled  (16 source chunks → 1 LOD chunk, covers 4×4 chunks)
  // LOD3 = 8x downsampled  (64 source chunks → 1 LOD chunk, covers 8×8 chunks)
  static inline int lod1Start = 24;  // lod1Start MUST == renderDistance
  static inline int lod1End   = 48;
  static inline int lod2Start = 48;  // lod2Start MUST == lod1End
  static inline int lod2End   = 80;
  static inline int lod3Start = 80;  // lod3Start MUST == lod2End
  static inline int lod3End   = 128;

  // Kept for compatibility; equals lod3End.
  static inline int lodRenderDistance = 96;

  static inline int shadowDistance = 8;
  static inline bool enableShadows = true;

  // Shadow map 1024: worldTexel = (shadowDist*16*2)/1024 = 0.25 blocks per texel.
  // Sufficient for smooth PCF 3x3 without being too heavy on the GPU.
  static int shadowMapSize() { return 1024; }

  /*
      WORLD HEIGHT
  */

  static constexpr int worldHeight = 256;

  /*
      SEED
  */

  static inline int seed = 1233;

  static inline float daySpeed = 50.0f;

  /*
      BIOME
  */

  static inline float biomeScale = 0.005f;

  static inline float plainsChance = 0.5f;

  /*
      TERRAIN
  */

  static inline int baseTerrainHeight = 12;

  static inline float terrainScale = 0.008f;

  static inline int terrainAmplitude = 90;

  /*
      MOUNTAIN
  */

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
  static constexpr float mountainCoreThreshold = 0.88f;

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

  // PEAKS & VALLEYS
  static inline float peaksScale = 0.010f; // slightly wider
  static inline int peakHeight =
      60; // raised from 75, lower slopes already handle mass

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

  static constexpr float pillarScale = 0.010f;
  static constexpr float pillarThreshold = 0.45f;

  // =================================═════════════════════════════════
  //  PLATEAU SETTINGS (main controls for island shape)
  // =================================═════════════════════════════════
  static constexpr float plateauScale = 0.0012f;
  // Keep island wide: threshold lowered to 0.52f so the island
  // spreads out immediately
  static constexpr float plateauThreshold = 0.52f;

  static constexpr int plateauBaseHeight = 85;
  static constexpr int plateauHeight = 45;

  // Terrace
  static constexpr int terraceCount = 3;
  static constexpr int terraceHeight = 12;

  // Pillar
  static constexpr int pillarMinHeight = 20;

  // Cliff
  static constexpr float cliffErosionScale = 0.015f;
  static constexpr float cliffErosionStr = 5.0f;

  // Valley
  static constexpr float valleyDepth = 45.0f;

  // ======================
  // STALACTITE & STALAGMITE
  // ======================

  // How often spike clusters appear (noise scale)
  static inline float spikeNoiseScale = 1.2f;

  // Reduced further: to make noise basin area spread wider (pillar
  // footprint grows larger)
  static inline float spikeSpawnThreshold = 0.01f;

  static inline float spikeDepthFalloff = 1.0f;
  static inline float spikeDepthCutoff = 0.0f;

  // Increased: grants extra massive thickness bonus when near the
  // island center
  static inline float spikeDepthThicknessMult = 0.65f;

  // Tip cutoff set to 0.60f so tips remain sharp
  // and clean
  static inline float spikeTaperCurve = 0.60f;

  // KEY PARAMETER (NEW): High exponent makes spike body rigid and straight
  // downward/upward like a pillar!
  static inline float spikeTaperExponent = 4.5f;

  // Maximum length for stalactites & stalagmites
  static inline float stalactiteMaxLen = 65.0f;
  static inline float stalagmiteMaxLen = 55.0f;

  // island
  static constexpr float islandCorePillarThreshold = 0.40f;
  static constexpr float islandEdgeCutoff = 0.12f;
  static constexpr int islandFloorGuard = 22;

  // ======================
  // FOG
  // ======================

  static inline float fogStart = 800.0f;
  static inline float fogEnd = 2000.0f;

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed = 40.0f;

  static inline float jumpForce = 70.0f;

  static inline float gravity = -20.0f;

  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};