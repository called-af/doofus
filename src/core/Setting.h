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
  static inline float spawnY = 60.0f;
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

  static inline int seed = 1222;

  /*
      BIOME
  */

  static inline float biomeScale = 0.005f;

  static inline float plainsChance = 0.5f;

  /*
      TERRAIN
  */

  static inline int baseTerrainHeight = 45;

  static inline float terrainScale = 0.008f;

  static inline int terrainAmplitude = 100;

  /*
      MOUNTAIN
  */

  static inline float mountainScale = 0.035f;

  static inline int mountainHeight = 130;

  // ======================
  // CLIMATE
  // ======================

  static inline float temperatureScale  = 0.0015f; 
  static inline float humidityScale     = 0.0015f;

  // ======================
  // BIOME RULES
  // ======================

  static inline float mountainThreshold = 0.54f;

  static inline float desertTemperature = 0.55f;
  static inline float desertHumidity = 0.45f;

  // ======================
  // CONTINENTALNESS
  // ======================

  static inline float continentalScale = 0.0005f;
  static inline int continentalHeight = 20;

  // ======================
  // PEAKS & VALLEYS
  // ======================

  static inline float peaksScale = 0.003f; 
  static inline int peakHeight = 100;

  // ======================
  // EROSION
  // ======================

  static inline float erosionScale = 0.003f;
  static inline int erosionStrength = 0.10f;

  // ======================
  // RIVERS
  // ======================

  static inline float riverScale = 0.0015f;
  static inline float riverThreshold = 0.003f;
  static inline int riverDepth = 40;

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

  static inline float pillarScale = 0.025f;
  static inline float pillarThreshold = 1.15f;

  // ======================
  // FOG
  // ======================

  static inline float fogStart = 10.0f;
  static inline float fogEnd = 300.0f;

  // ======================
  // PLAYER
  // ======================

  static inline float moveSpeed = 300.0f;

  static inline float jumpForce = 100.0f;

  static inline float gravity = -20.0f;

  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};