#include "BiomeManager.h"

#include "../../core/Setting.h"
#include "desert/DesertBiome.h"
#include "mountain/MountainBiome.h"
#include "plain/PlainBiome.h"
#include "cliff/CliffBiome.h"
#include "heaven/HeavenBiome.h"
#include "hell/HellBiome.h"
#include "../terrain/HellTerrain.h"
#include "../terrain/HeavenTerrain.h"

static PlainBiome plain;
static DesertBiome desert;
static MountainBiome mountain;
static CliffBiome cliff;

Biome* BiomeManager::getBiome(const TerrainSample &terrain,
                              const ClimateSample &climate)
{
    if (terrain.peaks > Setting::mountainThreshold)
    {
        return &mountain;
    }

    if (climate.temperature > Setting::desertTemperature &&
        climate.humidity < Setting::desertHumidity)
    {
        return &desert;
    }

    return &plain;
}

Biome* BiomeManager::getBiome(const TerrainSample &terrain,
                              const ClimateSample &climate,
                              int worldX, int worldZ, int y)
{
    // Tier 3: Heaven Realm
    if (y >= 270)
    {
        return HeavenTerrain::getBiome(static_cast<float>(worldX), static_cast<float>(worldZ), y);
    }

    // Tier 1: Hell Canyon / Underworld
    if (y <= Setting::hellCanyonRimY)
    {
        float canyonRatio = HellTerrain::getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
        if (canyonRatio > 0.12f)
        {
            return HellTerrain::getBiome(worldX, worldZ);
        }
    }

    // Tier 2: Normal Biomes (Mountain / Desert / Plain)
    return getBiome(terrain, climate);
}
