#pragma once

#include "Biome.h"
#include "../terrain/TerrainSample.h"
#include "../climate/ClimateSample.h"

class BiomeManager
{
public:
    static Biome* getBiome(const TerrainSample &terrain, const ClimateSample &climate);
    static Biome* getBiome(const TerrainSample &terrain, const ClimateSample &climate, int worldX, int worldZ, int y);
};
