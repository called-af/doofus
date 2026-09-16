// Setting.h
// Global engine settings

#ifndef DOOFUS_SETTING_H
#define DOOFUS_SETTING_H

#pragma once
#include <string>

class Setting
{
public:
  // WINDOW

  static inline int windowWidth = 1280;
  static inline int windowHeight = 720;

  static inline bool fullscreen = false;
  static inline bool vsync = false;

  // FONT

  static inline std::string fontPath =
      "assets/fonts/JetBrains_Mono,Space_Grotesk/JetBrains_Mono/static/"
      "JetBrainsMono-Regular.ttf";

  static inline int fontSize = 20;

  // CAMERA

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

  // WORLD

  // Full-detail chunk render distance.
  static inline int renderDistance = 12;

  // LOD SYSTEM (Level 1 to 5)

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

  //  Worker threads
  //
  //  Thread count is capped to avoid overloading the CPU while the game runs.
  //  Optimal value: (physical CPU count - 1), minimum 2.
  //  This value is read by ChunkWorker during initialisation.
  // 0 = auto (up to four workers, leaving CPU time for render/main thread).
  static inline int maxWorkerThreads = 0;

  // Maximum chunk meshes dispatched per frame (prevents hitching)
  static inline int maxMeshDispatchPerFrame = 6;

  // SHADOW

  static inline int shadowDistance = 14;
  static inline bool enableShadows = true;

  // 2048 gives crisp, accurate high-fidelity shadows for player, entities, and blocks
  static int shadowMapSize() { return 2048; }

  // WORLD HEIGHT

  static constexpr int worldHeight = 640;

  // SEED

  static inline int seed = 1233;

  static inline float daySpeed = 4.0f;

  // BIOME

  static inline float biomeScale = 0.00167f;  // 0.005 / 3 → 3× wider biomes
  static inline float plainsChance = 0.5f;

  // TERRAIN

  static inline int baseTerrainHeight = 12;
  static inline float terrainScale = 0.008f;
  static inline int terrainAmplitude = 90;

  // MOUNTAIN

  static inline float mountainScale = 0.0002f;
  static inline int mountainHeight = 130;

  // CLIMATE

  static inline float temperatureScale = 0.0015f;
  static inline float humidityScale = 0.0015f;

  // Domain warp untuk climate sampling — membuat garis batas biome bergelombang alami
  static constexpr float climateWarpScale    = 0.0008f;  // frekuensi warp (lebih kecil = warp lebih lebar)
  static constexpr float climateWarpStrength = 60.0f;    // amplitudo warp dalam blok

  // BIOME RULES

  static inline float mountainThreshold = 0.74f;
  static constexpr float mountainCoreThreshold = 0.0088f;

  static inline float desertTemperature = 0.55f;
  static inline float desertHumidity = 0.45f;

  // Lebar zona transisi antar biome dalam satuan noise unit
  // Lebih besar = zona blend lebih lebar dan halus
  static constexpr float mountainBlendWidth = 0.08f;
  static constexpr float climateBlendWidth  = 0.035f;
  static constexpr float biomeBlendRadius   = 0.7f;

  // Scale noise untuk dithering blok di zona transisi
  static constexpr float biomeBlendNoiseScale = 0.12f;

  // Di tepi island (pDepth < edgePlainsDepth), weight plains di-boost ke 1
  // pDepth range 0..1 (0=tepi, 1=tengah). 0.35 = zona edge di schemata lama
  static constexpr float edgePlainsDepth = 0.35f;

  // CONTINENTALNESS

  static inline float continentalScale = 0.0018f;
  static inline int continentalHeight = 8;

  // PEAKS & VALLEYS

  static inline float peaksScale = 0.010f;
  static inline int peakHeight = 100;  // lebih tinggi dari 60

  // EROSION

  static inline float erosionScale = 0.003f;
  static inline float erosionStrength = 5.0f;

  // RIVERS

  static inline float riverScale = 0.004f;
  static inline float riverThreshold = 0.015f;
  static inline int riverDepth = 20;

  // CAVES

  static inline float caveScale = 0.0009f;
  static inline float caveThreshold = 0.1f;
  static inline int caveMinY = 5;
  static inline int caveMaxY = 90;

  // PILLARS

  static constexpr float pillarScale = 0.000010f;
  static constexpr float pillarThreshold = 0.00045f;

  //  PLATEAU — primary control for island shape

  static constexpr float plateauScale = 0.00055f;  // lebih kecil = plateau lebih lebar
  // Low threshold so the island is wide from the start
  static constexpr float plateauThreshold = 0.42f;  // lebih rendah = lebih banyak area jadi daratan

  static constexpr int plateauBaseHeight = 85;
  static constexpr int plateauHeight = 70;  // lebih tinggi dari 45

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
  static constexpr float islandEdgeCutoff = 0.08f;  // lebih rendah = tepi pulau lebih lebar
  static constexpr int islandFloorGuard = 22;

  // ISLAND (cellular placement)

  static constexpr float islandCellScale = 0.004f;  // lebih kecil = tiap pulau lebih besar

  static constexpr float islandEdgeRadius = 0.70f;  // lebih besar = daratan lebih lebar sebelum drop-off

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

  // MOUNTAIN MASSIF

  // Low-freq scale untuk base gunung yang lebar (lebih kecil = gunung lebih lebar)
  static constexpr float massifScale = 0.00035f;
  // Ridge scale untuk puncak tajam di atas massif base
  static constexpr float massifRidgeScale = 0.0018f;
  // Threshold: kolom di bawah ini tidak tumbuh jadi gunung
  static constexpr float massifThreshold = 0.18f;
  // Ketinggian maksimal tambahan di atas flat plateau
  static constexpr int massifMaxHeight = 220;
  // Seberapa tajam transisi dari bukit ke gunung (power curve)
  static constexpr float massifSharpness = 2.2f;

  // ISLAND EDGE VARIATION

  // FBM medium-freq untuk undulasi naik-turun di tepi piringan
  static constexpr float edgeUndulationScale = 0.012f;
  // Amplitudo undulasi dalam blok (seberapa jauh naik/turun)
  static constexpr int edgeUndulationAmp = 28;
  // Ridge noise untuk cliff crack / tebing terjal di tepi
  static constexpr float edgeCliffScale = 0.008f;
  // Kedalaman drop cliff maksimal
  static constexpr int edgeCliffDrop = 22;
  // Noise kecil untuk bump/tonjolan acak di tepi
  static constexpr float edgeBumpScale = 0.025f;
  static constexpr int edgeBumpAmp = 14;

  // ISLAND RIM / SADDLE FILL

  // Tepi piringan mulai tipis dari pDepth ini ke bawah (power curve makin tajam)
  static constexpr float rimThinPower = 3.2f;   // lebih besar = tepi makin tipis/lancip
  // Ramp dari flat ke bukit: smoothDepth di mana ramp mulai
  static constexpr float hillRampStart = 0.15f;
  // Smoothing amount (blok) dari flat ke bukit
  static constexpr int hillRampHeight = 30;
  // Fill valley antar bukit: angkat floor dari saddle dip
  // 0 = tidak ada fill, 1 = penuh rata dengan puncak
  static constexpr float saddleFillStrength = 0.65f;

  // Variasi ketebalan rim atas per-kolom (noise modulation)
  // Scale rendah = variasi lebar/organik, bukan pixelated
  static constexpr float rimVariationScale = 0.006f;
  // Amplitudo variasi dalam blok — di tepi bisa mencapai nilai ini, di tengah lebih kecil
  static constexpr int rimVariationAmp = 30;

  // 2-TIER VERTICAL REALM ( Normal / Hell)

  // Tier 1 — Hell (Bottom Fiery Underworld & Canyon Realm)
  static constexpr float hellCanyonWidth = 130.0f;
  static constexpr float hellSpineWarpScale1 = 0.0018f;
  static constexpr float hellSpineWarpScale2 = 0.012f;
    static constexpr int hellCanyonFloorY = 8;
  static constexpr int hellCanyonRimY = 45;
  static constexpr float hellLavaThreshold = 0.55f;
  static constexpr float hellSpikeNoiseScale = 0.06f;

  // Crack / Fissure Network Parameters (Interconnected Magma Vein Network)
  static constexpr float hellCrackScale = 0.055f;       // lebih rapat, banyak vein / sungai lava
  static constexpr float hellCrackWarpScale = 0.014f;   // Frequency of domain warping for jagged fracture lines
  static constexpr float hellCrackWarpStrength = 12.5f; // Distortion strength of tectonic cracks
  static constexpr float hellCrackWidth = 0.22f;        // lebih lebar agar vein terasa padat dan berkelok
  static constexpr float hellLavaCrackThreshold = 0.68f; // lebih longgar, banyak jalur lava
  static constexpr float hellCinderThreshold = 0.52f;    // cinder lebih cepat muncul di tepian vein
  static constexpr float hellObsidianThreshold = 0.34f;  // obsidian lebih sering di tepian
  static constexpr float hellAshThreshold = 0.16f;       // ash deposits near veins

  // Hell Floor Base (Global underworld bedrock) 
  static constexpr int   hellFloorBaseY    = 14;
  static constexpr int   hellFloorDepth    = 6;
  static constexpr float hellFloorWarpAmp  = 3.0f;
  static constexpr float hellFloorWarpScale = 0.018f;

  // Base Terrain Small Hills
  static constexpr float baseHillScale1    = 0.007f; 
  static constexpr float baseHillScale2    = 0.018f; 
  static constexpr int   baseHillHeight    = 20;

  // Hell Dataran (Wide flat plains of Obsidian/Basalt/Ash)
  static constexpr float hellPlainScale     = 0.003f;  // lower scale = bigger plains
  static constexpr float hellPlainThreshold = 0.15f;   // lower = more plain coverage (~60% of floor)
  static constexpr int   hellPlainHeight    = 18;
  static constexpr int   hellPlainDepth     = 5;

  // Hell Lava Pool (Large pools placed on grid)
  static constexpr float hellPoolGridSize    = 256.0f;
  static constexpr float hellPoolSpawnChance = 0.50f;
  static constexpr float hellPoolMinRadius   = 20.0f;
  static constexpr float hellPoolMaxRadius   = 48.0f;
  static constexpr int   hellPoolSurfaceY    = 12;
  static constexpr int   hellPoolDepth       = 5;

  // Hell Ravine Void (Elongated jagged ravines instead of round holes)
  static constexpr float hellRavineScale      = 0.007f;  // frequency of ravine ridge noise
  static constexpr float hellRavineScale2     = 0.011f;  // second direction for crossing ravines
  static constexpr float hellRavineThreshold  = 0.80f;   // how sharp/thin the ravine ridge is
  static constexpr float hellRavineWarpScale  = 0.018f;
  static constexpr float hellRavineWarpStr    = 12.0f;   // warp strength → jagged non-straight walls
  static constexpr float hellRavineDensity    = 0.28f;   // fraction of floor that has ravines (lower = rarer)


  // FOG

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

  // PLAYER

  static inline float moveSpeed = 100.0f;
  static inline float jumpForce = 80.0f;
  static inline float gravity = -20.0f;
  static inline float reachDistance = 6.0f;

  static inline int breakCooldown = 5;
  static inline int placeCooldown = 5;
};

#endif
