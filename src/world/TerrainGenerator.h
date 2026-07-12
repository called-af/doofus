#pragma once

#include "Chunk.h"
#include "terrain/TerrainSample.h"

#include <array>

//  Per-column precomputed data — calculated once at the start of generate(),
//  used by all passes (base, surface, cave) without re-sampling noise.
struct ColumnCache {
    TerrainSample terrain;   // noise sample (continentalness, plateau, etc.)
    float         pRaw;      // (plateau - threshold) / (1 - threshold), clamped [0,1]
    float         pDepth;    // smoothstep(pRaw)
    int           floorH;    // computeBaseHeight result
    bool          isIsland;  // plateau >= plateauThreshold
};

class TerrainGenerator
{
public:
    static void generate(Chunk& chunk);

    // ── Standalone height and block samplers (does not require loaded Chunk) ──────────
    // Used by LOD level 4 and 5 which are too far away to require chunk data
    // Returns the highest solid block Y coordinate or -1 if empty
    static int       sampleHeightAt(int worldX, int worldZ);
    // LOD surface sampler.  Coarse heightmap LODs deliberately omit the
    // 22-block floating-cone signal; sampling that signal on a 128+ block grid
    // aliases it into random mountains.
    static int       sampleLODHeightAt(int worldX, int worldZ, int level);
    // Returns the block type at the highest point at (worldX, worldZ)
    static BlockType sampleBlockAt (int worldX, int worldZ, int surfaceHeight);

private:
    // Cache helper
    // Computes all column-level data once, stored in a 2D grid.
    using ColumnGrid = std::array<std::array<ColumnCache, Chunk::SIZE>, Chunk::SIZE>;
    static ColumnGrid buildColumnCache(const Chunk& chunk);

    // Main passes
    static void generateBaseTerrain(Chunk& chunk, const ColumnGrid& cache);
    static void generateSurface    (Chunk& chunk, const ColumnGrid& cache);
    static void generateCaves      (Chunk& chunk, const ColumnGrid& cache);

    // Height pipeline
    static int  computeBaseHeight (const TerrainSample& t);
    static int  applyPlateauLift  (int baseH, const TerrainSample& t);
    static int  applyTerrace      (int h,     const TerrainSample& t);
    static int  applyMountainTop  (int h,     const TerrainSample& t);
    static int  applyErosion      (int h,     const TerrainSample& t);

    // Pillar support
    static bool shouldSpawnPillar (const TerrainSample& t);
    static void fillPillar        (Chunk& chunk, int lx, int lz, int topH, int bottomH);

    // Island geometry helpers
    // Estimates bodyBottom from pDepth (used by base & cave passes).
    static int estimateBodyBottom (int flatPlateauH, float pDepth);
};
