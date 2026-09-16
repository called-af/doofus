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

            // Cache biome blend — dihitung sekali, dipakai ulang di generateSurface
            // ClimateSampler sudah include domain warp di dalamnya
            const ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
            c.biomeBlend = BiomeManager::getBlend(c.terrain, climate, c.pDepth);
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
    // Tier 2: Normal terrain / Continent
    int continentH = sampleContinentHeightAt(worldX, worldZ);
    if (continentH > 0)
        return continentH;

    // Tier 1: Hell Floor & Canyon
    return sampleHellFloorAt(worldX, worldZ);
}

BlockType TerrainGenerator::sampleBlockAt(int worldX, int worldZ, int surfaceHeight)
{
    if (surfaceHeight <= Setting::hellCanyonRimY)
    {
        return HellTerrain::sampleBlock(worldX, worldZ, surfaceHeight);
    }

    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    const ClimateSample climate = ClimateSampler::sample(worldX, worldZ);

    const float raw = (terrain.plateau - Setting::plateauThreshold) /
                      (1.0f - Setting::plateauThreshold);
    const float pRaw = std::clamp(raw, 0.0f, 1.0f);
    const float pDepth = pRaw * pRaw * (3.0f - 2.0f * pRaw);

    const BiomeBlend blend = BiomeManager::getBlend(terrain, climate, pDepth);
    return BiomeManager::blendedTopBlock(blend, worldX, worldZ);
}

bool TerrainGenerator::isSolidAt(int worldX, int worldZ, int y)
{

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

            // Blend sudah di-cache di buildColumnCache — tidak perlu hitung ulang
            const BiomeBlend &blend = col.biomeBlend;

            int layerDepth = 0;

            for (int y = Chunk::HEIGHT - 1; y >= 0; --y)
            {
                BlockType bt = chunk.blocks[x][y][z];
                if (bt == BlockType::Air)
                    continue;

                const bool airAbove = (y + 1 >= Chunk::HEIGHT) ||
                                      (chunk.blocks[x][y + 1][z] == BlockType::Air);

               
                // Tier 1 — Hell Canyon / Underworld surface
                if (bt == BlockType::Basalt || bt == BlockType::Obsidian || bt == BlockType::Ash ||
                    bt == BlockType::Cinder || bt == BlockType::Lava ||
                    (y <= Setting::hellCanyonFloorY + 2 && col.canyonDepthRatio > 0.07f))
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
                        chunk.blocks[x][y][z] = BiomeManager::blendedTopBlock(blend, worldX, worldZ);
                        layerDepth = 3;
                    }
                    else if (layerDepth > 0)
                    {
                        chunk.blocks[x][y][z] = BiomeManager::blendedMiddleBlock(blend, worldX, worldZ);
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
                // Pakai surfaceH aktual (heightMap sudah dihitung di generateBaseTerrain)
                // supaya estimasi bodyBottom konsisten dengan generateColumnBase
                const int estimatedBodyBottom =
                    ContinentTerrain::estimateBodyBottom(surfaceH, col.pDepth, worldX, worldZ);

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
