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
  static inline bool vsync = true;

  // ======================
  // FONT
  // ======================

  static inline std::string fontPath =
      "assets/fonts/JetBrains_Mono,Space_Grotesk/JetBrains_Mono/static/JetBrainsMono-Regular.ttf";

  static inline int fontSize = 5;

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

  static inline int renderDistance = 8;

  /*
      WORLD HEIGHT
  */

  static inline int worldHeight = 256;

  /*
      SEED
  */

  static inline int seed = 1111;

  /*
      BIOME
  */

  static inline float biomeScale = 0.01f;

  static inline float plainsChance = 0.5f;

  /*
      TERRAIN
  */

  static inline int baseTerrainHeight = 45;

  static inline float terrainScale = 0.008f;

  static inline int terrainAmplitude = 18;

  /*
      MOUNTAIN
  */

  static inline float mountainScale = 0.0035f;

  static inline int mountainHeight = 40;

  /*
      CAVES
  */

  static inline float caveScale = 0.045f;

  static inline float caveThreshold = 0.72f;

  /*
      PILLARS
  */

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

  static inline float moveSpeed = 100.0f;

  static inline float jumpForce = 50.0f;

  static inline float gravity = -20.0f;

  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};