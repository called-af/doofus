#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "climate/ClimateSampler.h"
#include "terrain/TerrainSampler.h"
#include "noise/FBMNoise.h"
#include "../core/Setting.h"

#include <algorithm>
#include <cmath>

TerrainGenerator::ColumnGrid
TerrainGenerator::buildColumnCache(const Chunk &chunk)
{
    ColumnGrid grid{};

    for (int x = 0; x < Chunk::SIZE; ++x)
    {
        for (int z = 0; z < Chunk::SIZE; ++z)
        {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            ColumnCache &c = grid[x][z];
            c.terrain = TerrainSampler::sample(worldX, worldZ);

            const float raw = (c.terrain.plateau - Setting::plateauThreshold) /
                              (1.0f - Setting::plateauThreshold);
            c.pRaw = std::clamp(raw, 0.0f, 1.0f);
            c.pDepth = c.pRaw * c.pRaw * (3.0f - 2.0f * c.pRaw); // smoothstep

            c.floorH = ContinentTerrain::computeBaseHeight(c.terrain);
            c.isIsland = (c.terrain.plateau >= Setting::plateauThreshold);

            c.canyonDepthRatio = HellTerrain::getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
            c.crackIntensity = HellTerrain::getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), c.canyonDepthRatio);
            c.heavenSeed = HeavenTerrain::findNearestSeed(static_cast<float>(worldX), static_cast<float>(worldZ), c.heavenDistance);
        }
    }

    return grid;
}

void TerrainGenerator::generate(Chunk &chunk)
{
    for (int x = 0; x < Chunk::SIZE; ++x)
        for (int z = 0; z < Chunk::SIZE; ++z)
        {
            chunk.heightMap[x][z] = 0;
            for (int y = 0; y < Chunk::HEIGHT; ++y)
                chunk.blocks[x][y][z] = BlockType::Air;
        }

    const ColumnGrid cache = buildColumnCache(chunk);

    generateBaseTerrain(chunk, cache);
    generateSurface(chunk, cache);
    generateCaves(chunk, cache);
}

int TerrainGenerator::sampleHellFloorAt(int worldX, int worldZ)
{
    return HellTerrain::sampleFloorHeight(worldX, worldZ);
}

int TerrainGenerator::sampleContinentHeightAt(int worldX, int worldZ)
{
    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    return ContinentTerrain::sampleHeightAt(worldX, worldZ, terrain);
}

int TerrainGenerator::sampleContinentBodyBottomAt(int worldX, int worldZ)
{
    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    int baseFloorY = sampleHellFloorAt(worldX, worldZ);
    return ContinentTerrain::sampleBodyBottomAt(worldX, worldZ, terrain, baseFloorY);
}

int TerrainGenerator::sampleHeightAt(int worldX, int worldZ)
{
    // Tier 3: Heaven floating islands
    int heavenH = HeavenTerrain::sampleHeightAt(static_cast<float>(worldX), static_cast<float>(worldZ));
    if (heavenH > 0)
        return heavenH;

    // Tier 2: Normal terrain / Continent
    int continentH = sampleContinentHeightAt(worldX, worldZ);
    if (continentH > 0)
        return continentH;

    // Tier 1: Hell Floor & Canyon
    return sampleHellFloorAt(worldX, worldZ);
}

BlockType TerrainGenerator::sampleBlockAt(int worldX, int worldZ, int surfaceHeight)
{
    if (surfaceHeight >= 270)
    {
        return HeavenTerrain::sampleBlock(static_cast<float>(worldX), static_cast<float>(worldZ), surfaceHeight);
    }

    if (surfaceHeight <= Setting::hellCanyonRimY)
    {
        return HellTerrain::sampleBlock(worldX, worldZ, surfaceHeight);
    }

    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
    return BiomeManager::getBiome(terrain, climate, worldX, worldZ, surfaceHeight)->getTopBlock();
}

bool TerrainGenerator::isSolidAt(int worldX, int worldZ, int y)
{
    // Tier 3: Heaven floating islands
    float hDist = 0.0f;
    FeatureSeed hSeed = HeavenTerrain::findNearestSeed(static_cast<float>(worldX), static_cast<float>(worldZ), hDist);
    if (hSeed.exists)
    {
        int totalIslands = 1 + hSeed.subIsletCount;
        for (int i = 0; i < totalIslands; ++i)
        {
            IslandSlice slice = HeavenTerrain::evaluateSlice(static_cast<float>(worldX), static_cast<float>(worldZ), hSeed, i);
            if (slice.valid && y >= slice.botY && y <= slice.topY)
                return true;
        }
    }

    // Hell Underworld & Canyon Floor
    int baseFloorY = sampleHellFloorAt(worldX, worldZ);
    if (y <= baseFloorY)
        return true;

    // Tier 2: Continent & stem support
    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    return ContinentTerrain::isSolidAt(worldX, worldZ, y, terrain, baseFloorY);
}

void TerrainGenerator::generateBaseTerrain(Chunk &chunk, const ColumnGrid &cache)
{
    for (int x = 0; x < Chunk::SIZE; ++x)
    {
        for (int z = 0; z < Chunk::SIZE; ++z)
        {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            const ColumnCache &col = cache[x][z];

            // TIER 1 — HELL UNDERWORLD, CANYON & LAVA VEIN NETWORK
            int baseFloorY = 0;
            HellTerrain::generateColumnBase(chunk, x, z, worldX, worldZ,
                                            col.floorH, col.canyonDepthRatio, col.crackIntensity,
                                            baseFloorY);
            chunk.heightMap[x][z] = baseFloorY;

            // TIER 2 — CONTINENT REALM & HOURGLASS STEM PILLARS
            ContinentTerrain::generateColumnBase(chunk, x, z, worldX, worldZ,
                                                 col.terrain, col.pDepth, baseFloorY);

            // TIER 3 — HEAVEN FLOATING ARCHIPELAGO CLUSTERS
            if (col.heavenSeed.exists)
            {
                HeavenTerrain::generateColumnBase(chunk, x, z, worldX, worldZ, col.heavenSeed);
            }
        }
    }
}

void TerrainGenerator::generateSurface(Chunk &chunk, const ColumnGrid &cache)
{
    for (int x = 0; x < Chunk::SIZE; ++x)
    {
        for (int z = 0; z < Chunk::SIZE; ++z)
        {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            const ColumnCache &col = cache[x][z];
            const TerrainSample &terrain = col.terrain;

            ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
            Biome *biome = BiomeManager::getBiome(terrain, climate, worldX, worldZ, chunk.heightMap[x][z]);

            int layerDepth = 0;

            for (int y = Chunk::HEIGHT - 1; y >= 0; --y)
            {
                BlockType bt = chunk.blocks[x][y][z];
                if (bt == BlockType::Air)
                    continue;

                const bool airAbove = (y + 1 >= Chunk::HEIGHT) ||
                                      (chunk.blocks[x][y + 1][z] == BlockType::Air);

                // Tier 3 — Heavenstone surface
                if (bt == BlockType::Heavenstone)
                {
                    if (airAbove)
                    {
                        HeavenTerrain::generateColumnSurface(chunk, x, z, y, worldX, worldZ, col.heavenSeed, layerDepth);
                    }
                    else if (layerDepth > 0)
                    {
                        chunk.blocks[x][y][z] = BlockType::Dirt;
                        --layerDepth;
                    }
                }
                // Tier 1 — Hell Canyon / Underworld surface
                else if (bt == BlockType::Basalt || bt == BlockType::Obsidian || bt == BlockType::Lava || (y <= Setting::hellCanyonFloorY + 2 && col.canyonDepthRatio > 0.12f))
                {
                    if (airAbove)
                    {
                        HellTerrain::generateColumnSurface(chunk, x, z, y, worldX, worldZ, col.canyonDepthRatio, col.crackIntensity);
                    }
                }
                // Tier 2 — Normal continent surface
                else if (bt == BlockType::Stone)
                {
                    if (airAbove)
                    {
                        ContinentTerrain::generateColumnSurface(chunk, x, z, y, worldX, worldZ, biome, layerDepth);
                    }
                    else if (layerDepth > 0)
                    {
                        chunk.blocks[x][y][z] = biome->getMiddleBlock();
                        --layerDepth;
                    }
                }
            }
        }
    }
}

void TerrainGenerator::generateCaves(Chunk &chunk, const ColumnGrid &cache)
{
    for (int x = 0; x < Chunk::SIZE; ++x)
    {
        for (int z = 0; z < Chunk::SIZE; ++z)
        {
            const int worldX = chunk.chunkX * Chunk::SIZE + x;
            const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

            const ColumnCache &col = cache[x][z];
            const int surfaceH = chunk.heightMap[x][z];

            int caveMax = std::min(90, static_cast<int>(surfaceH * 0.70f));

            if (col.isIsland)
            {
                constexpr int conservativeFlatPlateauH = 100;
                const int estimatedBodyBottom =
                    ContinentTerrain::estimateBodyBottom(conservativeFlatPlateauH, col.pDepth);

                caveMax = std::min(caveMax, estimatedBodyBottom - 2);
            }

            if (caveMax <= 5)
                continue;

            for (int y = 5; y < caveMax; ++y)
            {
                const float cave = FBMNoise::generate(worldX + y * 2, worldZ + y * 2, 3,
                                                      0.5f, 0.045f, Setting::seed);

                if (cave > 0.72f)
                {
                    const bool hasNeighborStone =
                        (y > 0 && chunk.blocks[x][y - 1][z] == BlockType::Stone) ||
                        (y < Chunk::HEIGHT - 1 &&
                         chunk.blocks[x][y + 1][z] == BlockType::Stone);

                    if (hasNeighborStone)
                        chunk.blocks[x][y][z] = BlockType::Air;
                }
            }
        }
    }
}
