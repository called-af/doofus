#pragma once

#include "Chunk.h"
#include "terrain/TerrainSample.h"
#include "terrain/HellTerrain.h"
#include "terrain/ContinentTerrain.h"
#include "biome/BiomeManager.h"

#include <array>
#include <vector>

// Per-column precomputed cache (calculated once per chunk per column)
struct ColumnCache
{
    TerrainSample terrain;  // noise sample (continentalness, plateau, etc.)
    float pRaw;             // (plateau - threshold) / (1 - threshold), clamped [0,1]
    float pDepth;           // smoothstep(pRaw)
    int floorH;             // base height
    bool isIsland;          // plateau >= plateauThreshold

    // Tier 1 Hell Canyon data
    float canyonDepthRatio; // 1.0 at spine center, 0.0 outside canyon
    float crackIntensity;   // 0.0 = solid basalt bedrock, 1.0 = deep molten lava crack/river

    // Biome blend — cached per kolom, tidak perlu hitung ulang per Y layer
    BiomeBlend biomeBlend;
};

class TerrainGenerator
{
public:
    static void generate(Chunk &chunk);


    static inline float getHellSpineX(float worldZ)
    {
        return HellTerrain::getSpineX(worldZ);
    }
    static inline float getHellCanyonDepthRatio(float worldX, float worldZ)
    {
        return HellTerrain::getCanyonDepthRatio(worldX, worldZ);
    }
    static inline float getHellCrackIntensity(float worldX, float worldZ, float canyonRatio)
    {
        return HellTerrain::getCrackIntensity(worldX, worldZ, canyonRatio);
    }

    // Standalone height and block samplers (does not require loaded Chunk)
    static int sampleHeightAt(int worldX, int worldZ);
    static bool isSolidAt(int worldX, int worldZ, int y);
    static BlockType sampleBlockAt(int worldX, int worldZ, int surfaceHeight);

    // Standalone multi-tier samplers for LOD & physics
    static int sampleHellFloorAt(int worldX, int worldZ);
    static int sampleContinentHeightAt(int worldX, int worldZ);
    static int sampleContinentBodyBottomAt(int worldX, int worldZ);
    static inline int estimateBodyBottom(int topHeight, float pDepth, int worldX = 0, int worldZ = 0)
    {
        return ContinentTerrain::estimateBodyBottom(topHeight, pDepth, worldX, worldZ);
    }

private:
    using ColumnGrid = std::array<std::array<ColumnCache, Chunk::SIZE>, Chunk::SIZE>;
    static ColumnGrid buildColumnCache(const Chunk &chunk);

    // Main passes
    static void generateBaseTerrain(Chunk &chunk, const ColumnGrid &cache);
    static void generateSurface(Chunk &chunk, const ColumnGrid &cache);
    static void generateCaves(Chunk &chunk, const ColumnGrid &cache);
};
