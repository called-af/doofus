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
  static inline float farPlane = 500.0f;

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

  /*
      WORLD HEIGHT
  */

  static constexpr int worldHeight = 256;

  /*
      SEED
  */

  static inline int seed = 1234;

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
  static inline float peaksScale = 0.010f; // sedikit lebih lebar
  static inline int peakHeight =
      60; // naikkan dari 75, lereng bawah udah handle massa

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
  //  PLATEAU SETTING (KUNCI UTAMA BENTUK PULAU LOYANG PIZZA)
  // =================================═════════════════════════════════
  static constexpr float plateauScale = 0.0012f;
  // LOYANG PIZZA TETAP LUAS: Threshold diturunkan ke 0.52f agar pulau yang
  // muncul langsung melebar luas
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

  // Seberapa sering cluster spike muncul (noise scale)
  static inline float spikeNoiseScale = 1.2f;

  // DIKECILKAN LAGI: Supaya area cekungan noise makin melebar luas (footprint
  // pilar makin raksasa)
  static inline float spikeSpawnThreshold = 0.01f;

  static inline float spikeDepthFalloff = 1.0f;
  static inline float spikeDepthCutoff = 0.0f;

  // DINAIKKAN: Memberikan bonus ketebalan ekstra masif saat berada di pusat
  // pulau
  static inline float spikeDepthThicknessMult = 0.65f;

  // Batas pemotongan ujung ditekankan ke 0.60f agar ujungnya tetep dapet lancip
  // yang clean
  static inline float spikeTaperCurve = 0.60f;

  // KUNCI UTAMA (BARU): Pangkat tinggi membuat bodi duri kokoh lurus ke
  // bawah/atas seperti pilar!
  static inline float spikeTaperExponent = 4.5f;

  // Panjang maksimum stalactite & stalagmite
  static inline float stalactiteMaxLen = 65.0f;
  static inline float stalagmiteMaxLen = 55.0f;

  // island
  static constexpr float islandCorePillarThreshold = 0.40f;
  static constexpr float islandEdgeCutoff = 0.12f;
  static constexpr int islandFloorGuard = 22;

  // ======================
  // FOG
  // ======================

  static inline float fogStart = 10.0f;
  static inline float fogEnd = 300.0f;

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed = 100.0f;

  static inline float jumpForce = 70.0f;

  static inline float gravity = -20.0f;

  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};