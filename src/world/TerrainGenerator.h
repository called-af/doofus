#pragma once

#include "Chunk.h"
#include "terrain/TerrainSample.h"

#include <array>

// Per-column precomputed data. Deliberately minimal for the flat baseline —
// add more fields (plateau, pDepth, isIsland, etc.) here as old features
// get reintroduced, so noise is still only sampled once per column.
struct ColumnCache {
    TerrainSample terrain;
    int           surfaceH; // final height for this column
    int   topHeight;       // dulu "surfaceH" — permukaan atas island
    int   floorHeight;     // dasar slab island (buat floating gap)
    bool  isFloating;      // false = kolom ini void, no island
    float radialFalloff;   // 0 = center pulau, 1 = edge — dipakai buat neck tapering
};

struct IslandHeight {
    int   top        = 0;
    int   floor       = 0;
    bool  isFloating  = false;
    float radialFalloff = 1.0f;
};

class TerrainGenerator
{
public:
    static void generate(Chunk& chunk);

    static int       sampleHeightAt(int worldX, int worldZ);
    static int       sampleLODHeightAt(int worldX, int worldZ, int level);
    static BlockType sampleBlockAt(int worldX, int worldZ, int surfaceHeight);

private:
    using ColumnGrid = std::array<std::array<ColumnCache, Chunk::SIZE>, Chunk::SIZE>;
    static ColumnGrid buildColumnCache(const Chunk& chunk);

    // Pipeline — each pass is independent and only reads from `cache`.
    // Add new passes here (caves, pillars, islands...) instead of piling
    // everything back into generateBaseTerrain.
    static void generateBaseTerrain(Chunk& chunk, const ColumnGrid& cache);
    static void generateSurface    (Chunk& chunk, const ColumnGrid& cache);

    // Single source of truth for column height — used by generate(),
    // sampleHeightAt(), and sampleLODHeightAt() alike. Change this first
    // when you want terrain to stop being flat.
    static int computeHeight(int worldX, int worldZ);
    static IslandHeight computeIslandHeight(int worldX, int worldZ, const TerrainSample &terrain);
};