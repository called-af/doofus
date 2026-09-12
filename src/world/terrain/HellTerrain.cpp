#include "HellTerrain.h"

#include "../biome/hell/HellBiome.h"
#include "../noise/CellularNoise.h"
#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <cmath>

static HellLavaVeinBiome s_lavaVeinBiome;
static HellBasaltCragsBiome s_basaltCragsBiome;
static HellAshlandsBiome s_ashlandsBiome;

float HellTerrain::getSpineX(float worldZ)
{
    float w1 = FBMNoise::generate(worldZ * Setting::hellSpineWarpScale1, 0.0f, 3, 0.5f, 0.5f, Setting::seed + 101) * 320.0f;
    float w2 = FBMNoise::generate(worldZ * Setting::hellSpineWarpScale2, 100.0f, 2, 0.5f, 0.5f, Setting::seed + 202) * 55.0f;
    return w1 + w2;
}

float HellTerrain::getCanyonDepthRatio(float worldX, float worldZ)
{
    float spineX = getSpineX(worldZ);
    float dist = std::abs(worldX - spineX);
    if (dist >= Setting::hellCanyonWidth)
        return 0.0f;
    float norm = dist / Setting::hellCanyonWidth;
    return 1.0f - (norm * norm * (3.0f - 2.0f * norm)); // smoothstep falloff
}

float HellTerrain::getCrackIntensity(float worldX, float worldZ, float canyonRatio)
{
    if (canyonRatio <= 0.12f)
        return 0.0f;

    // 1. Large-scale Domain Warping for organic, serpentine tectonic fractures
    float warpX = FBMNoise::generate(worldX * Setting::hellCrackWarpScale, worldZ * Setting::hellCrackWarpScale, 3, 0.55f, 0.5f, Setting::seed + 311) * Setting::hellCrackWarpStrength;
    float warpZ = FBMNoise::generate(worldX * Setting::hellCrackWarpScale + 250.0f, worldZ * Setting::hellCrackWarpScale + 250.0f, 3, 0.55f, 0.5f, Setting::seed + 411) * Setting::hellCrackWarpStrength;

    float wx = worldX + warpX;
    float wz = worldZ + warpZ;

    // 2. Primary volcanic river spine channel (central molten lava river in canyon heart)
    float spineX = getSpineX(worldZ);
    float distToSpine = std::abs(worldX - spineX);
    float riverWidth = 16.0f + 8.0f * FBMNoise::generate(worldZ * 0.012f, 50.0f, 2, 0.5f, 0.5f, Setting::seed + 512);
    float riverFactor = 0.0f;
    if (distToSpine < riverWidth)
    {
        float rn = distToSpine / riverWidth;
        riverFactor = 1.0f - rn * rn * (3.0f - 2.0f * rn); // Smoothstep river channel
    }

    // 3. Interconnected Voronoi Vein Network (tectonic plate fissure borders)
    CellularSample cell = CellularNoise::generate(wx * Setting::hellCrackScale, wz * Setting::hellCrackScale, Setting::seed + 622);
    float edgeDist = cell.f2 - cell.f1; // 0.0 at vein center, larger inside crust plates
    float voronoiVein = 1.0f - std::clamp(edgeDist / Setting::hellCrackWidth, 0.0f, 1.0f);
    voronoiVein = voronoiVein * voronoiVein * (3.0f - 2.0f * voronoiVein);

    // 4. Secondary Tributary Vascular Veins (continuous branching ridge networks)
    float ridgeVein1 = RidgeNoise::generate(wx * 0.045f + 120.0f, wz * 0.045f + 120.0f, 3, 0.55f, 0.04f, Setting::seed + 733);
    float ridgeVein2 = RidgeNoise::generate(wx * 0.085f + 340.0f, wz * 0.085f + 340.0f, 2, 0.50f, 0.05f, Setting::seed + 844);
    float vascularBranch = std::max(0.0f, (ridgeVein1 * 0.70f + ridgeVein2 * 0.50f) - 0.35f) * 1.6f;

    // 5. Fine capillary fissure cracks
    float fineCracks = RidgeNoise::generate(wx * 0.14f + 70.0f, wz * 0.14f + 70.0f, 2, 0.5f, 0.06f, Setting::seed + 955);
    float capillaries = std::max(0.0f, fineCracks - 0.48f) * 1.5f;

    // Combine all vein hierarchies into one rich, continuous vascular magma network
    float interconnectedVeins = std::max(voronoiVein * 0.95f, vascularBranch * 0.88f);
    interconnectedVeins = std::max(interconnectedVeins, capillaries * 0.75f);
    float totalCrack = std::max(riverFactor * 0.98f, interconnectedVeins);

    // Modulate with canyon depth ratio so veins flourish on the canyon floor
    float canyonMod = std::clamp((canyonRatio - 0.10f) / 0.35f, 0.0f, 1.0f);
    return totalCrack * canyonMod;
}

int HellTerrain::sampleFloorHeight(int worldX, int worldZ)
{
    float canyonRatio = getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    const int floorY = Setting::hellCanyonFloorY;
    int baseFloorY = 14;

    if (canyonRatio > 0.0f)
    {
        baseFloorY = std::max(floorY, static_cast<int>(baseFloorY - canyonRatio * (baseFloorY - floorY)));
    }

    if (canyonRatio > 0.12f)
    {
        const float crackIntensity = getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
        
        // Sunken Magma Trench: where lava veins flow, carve 1-3 blocks deep into basalt bedrock
        if (crackIntensity >= Setting::hellLavaCrackThreshold)
        {
            baseFloorY = std::max(4, baseFloorY - 2);
        }
        else if (crackIntensity >= Setting::hellCinderThreshold)
        {
            baseFloorY = std::max(4, baseFloorY - 1);
        }

        // Basalt / Obsidian crags and spires rise on the center of cooled crust plates (away from veins)
        if (canyonRatio > 0.35f && crackIntensity < 0.25f)
        {
            const float spikeNoise = RidgeNoise::generate(
                worldX * Setting::hellSpikeNoiseScale, worldZ * Setting::hellSpikeNoiseScale, 2, 0.5f, 0.05f, Setting::seed + 666);
            if (spikeNoise > 0.58f)
            {
                int spikeHeight = baseFloorY + static_cast<int>((spikeNoise - 0.58f) * 45.0f);
                spikeHeight = std::min(spikeHeight, Setting::hellCanyonRimY - 4);
                if (spikeHeight > baseFloorY)
                    baseFloorY = spikeHeight;
            }
        }
    }

    return baseFloorY;
}

BlockType HellTerrain::sampleBlock(int worldX, int worldZ, int surfaceHeight)
{
    float canyonRatio = getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    if (canyonRatio > 0.12f)
    {
        float crackIntensity = getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
        if (crackIntensity >= Setting::hellLavaCrackThreshold)
            return BlockType::Lava;
        if (crackIntensity >= Setting::hellCinderThreshold)
            return BlockType::Cinder;
        if (crackIntensity >= Setting::hellObsidianThreshold)
            return BlockType::Obsidian;
        if (crackIntensity >= Setting::hellAshThreshold)
            return BlockType::Ash;
        return BlockType::Basalt;
    }
    return BlockType::Basalt;
}

Biome* HellTerrain::getBiome(int worldX, int worldZ)
{
    float canyonRatio = getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    float crackIntensity = getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
    if (crackIntensity >= Setting::hellCinderThreshold)
        return &s_lavaVeinBiome;
    if (crackIntensity >= Setting::hellAshThreshold)
        return &s_ashlandsBiome;
    return &s_basaltCragsBiome;
}

void HellTerrain::generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                     int floorH, float canyonRatio, float crackIntensity,
                                     int &outBaseFloorY)
{
    const int floorY = Setting::hellCanyonFloorY;
    int baseFloorY = floorH;
    if (canyonRatio > 0.0f)
    {
        baseFloorY = std::max(floorY, static_cast<int>(floorH - canyonRatio * (floorH - floorY)));
    }

    if (canyonRatio > 0.12f)
    {
        if (crackIntensity >= Setting::hellLavaCrackThreshold)
            baseFloorY = std::max(4, baseFloorY - 2);
        else if (crackIntensity >= Setting::hellCinderThreshold)
            baseFloorY = std::max(4, baseFloorY - 1);
    }

    for (int y = 0; y <= baseFloorY; ++y)
    {
        if (canyonRatio > 0.18f)
        {
            if (crackIntensity >= Setting::hellLavaCrackThreshold && y >= baseFloorY - 1)
                chunk.blocks[x][y][z] = BlockType::Lava;
            else if (crackIntensity >= Setting::hellObsidianThreshold)
                chunk.blocks[x][y][z] = BlockType::Obsidian;
            else
                chunk.blocks[x][y][z] = BlockType::Basalt;
        }
        else
        {
            chunk.blocks[x][y][z] = BlockType::Stone;
        }
    }

    // Spires and Jagged Crags on solid crust plates
    if (canyonRatio > 0.35f && crackIntensity < 0.25f)
    {
        const float spikeNoise = RidgeNoise::generate(
            worldX * Setting::hellSpikeNoiseScale, worldZ * Setting::hellSpikeNoiseScale, 2, 0.5f, 0.05f, Setting::seed + 666);
        if (spikeNoise > 0.58f)
        {
            int spikeHeight = baseFloorY + static_cast<int>((spikeNoise - 0.58f) * 45.0f);
            spikeHeight = std::min(spikeHeight, Setting::hellCanyonRimY - 4);
            for (int y = baseFloorY + 1; y <= spikeHeight; ++y)
            {
                chunk.blocks[x][y][z] = (spikeNoise > 0.72f) ? BlockType::Obsidian : BlockType::Basalt;
            }
            if (spikeHeight > baseFloorY)
                baseFloorY = spikeHeight;
        }
    }

    outBaseFloorY = baseFloorY;
}

void HellTerrain::generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                        float canyonRatio, float crackIntensity)
{
    if (crackIntensity >= Setting::hellLavaCrackThreshold)
        chunk.blocks[x][y][z] = BlockType::Lava;
    else if (crackIntensity >= Setting::hellCinderThreshold)
        chunk.blocks[x][y][z] = BlockType::Cinder;
    else if (crackIntensity >= Setting::hellObsidianThreshold)
        chunk.blocks[x][y][z] = BlockType::Obsidian;
    else if (crackIntensity >= Setting::hellAshThreshold)
        chunk.blocks[x][y][z] = (y % 2 == 0) ? BlockType::Ash : BlockType::Basalt;
    else
        chunk.blocks[x][y][z] = BlockType::Basalt;
}
