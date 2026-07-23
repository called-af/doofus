#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "climate/ClimateSampler.h"
#include "terrain/TerrainSampler.h"

#include "../core/Setting.h"

// ───────────────────────────────────────────────────────────
//  FLAT BASELINE
//  Height is a constant (Setting::flatHeight) — no plateau/erosion/
//  pillar/cave logic yet. This is the starting point for a gradual rebuild.
//
//  Suggested rebuild order:
//    1. computeHeight()      -> swap constant for continentalness noise
//    2. generateBaseTerrain  -> add island/plateau shaping
//    3. generateSurface      -> already biome-aware, fine as-is
//    4. new pass: caves
//    5. new pass: pillar/spike/floating island
// ───────────────────────────────────────────────────────────

int TerrainGenerator::computeHeight(int worldX, int worldZ) {
    (void)worldX;
    (void)worldZ;
    return Setting::flatHeight;
}

TerrainGenerator::ColumnGrid
TerrainGenerator::buildColumnCache(const Chunk &chunk) {
    ColumnGrid grid{};

    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            ColumnCache &c = grid[x][z];
            c.terrain  = TerrainSampler::sample(worldX, worldZ);
            c.surfaceH = computeHeight(worldX, worldZ);
        }
    }

    return grid;
}

void TerrainGenerator::generate(Chunk &chunk) {
    for (int x = 0; x < Chunk::SIZE; ++x)
        for (int z = 0; z < Chunk::SIZE; ++z) {
            chunk.heightMap[x][z] = 0;
            for (int y = 0; y < Chunk::HEIGHT; ++y)
                chunk.blocks[x][y][z] = BlockType::Air;
        }

    const ColumnGrid cache = buildColumnCache(chunk);

    generateBaseTerrain(chunk, cache);
    generateSurface(chunk, cache);
}

int TerrainGenerator::sampleHeightAt(int worldX, int worldZ) {
    return computeHeight(worldX, worldZ);
}

int TerrainGenerator::sampleLODHeightAt(int worldX, int worldZ, int level) {
    (void)level; // flat baseline: every LOD level returns the same height
    return computeHeight(worldX, worldZ);
}

BlockType TerrainGenerator::sampleBlockAt(int worldX, int worldZ, int surfaceHeight) {
    (void)surfaceHeight;
    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    const ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
    return BiomeManager::getBiome(terrain, climate)->getTopBlock();
}

//  PASS 1 — BASE TERRAIN (flat stone slab up to surfaceH)

void TerrainGenerator::generateBaseTerrain(Chunk &chunk, const ColumnGrid &cache) {
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            const int h = cache[x][z].surfaceH;
            chunk.heightMap[x][z] = h;

            for (int y = 0; y <= h; ++y)
                chunk.blocks[x][y][z] = BlockType::Stone;
        }
    }
}

//  PASS 2 — SURFACE (top/middle block from biome)

void TerrainGenerator::generateSurface(Chunk &chunk, const ColumnGrid &cache) {
    for (int x = 0; x < Chunk::SIZE; ++x) {
        for (int z = 0; z < Chunk::SIZE; ++z) {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            const ColumnCache &col = cache[x][z];
            const ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
            Biome *biome = BiomeManager::getBiome(col.terrain, climate);

            const int h = col.surfaceH;
            constexpr int middleDepth = 4;

            for (int mountain = 0; mountain < 2; ++ mountain) {
              if (h > Setting::mountainThreshold) {
                biome = BiomeManager::getBiome(col.terrain, climate);
              }
            }

            for (int depth = 0; depth <= middleDepth && h - depth >= 0; ++depth) {
                const int y = h - depth;
                chunk.blocks[x][y][z] =
                    (depth == 0) ? biome->getTopBlock() : biome->getMiddleBlock();
            }
        }
    }
}