#pragma once

#include "Biome.h"
#include "../terrain/TerrainSample.h"
#include "../climate/ClimateSample.h"
#include "../block/BlockType.h"

#include <array>

// Maksimal 3 biome aktif sekaligus di satu kolom (triple-border case)
static constexpr int kMaxBlendBiomes = 3;

struct BiomeBlend
{
    std::array<Biome*, kMaxBlendBiomes> biomes  = {};
    std::array<float,  kMaxBlendBiomes> weights = {};
    int count = 0;  // berapa biome aktif (1..kMaxBlendBiomes)
};

class BiomeManager
{
public:
    // Legacy — masih dipake sampleBlockAt di TerrainGenerator
    static Biome* getBiome(const TerrainSample &terrain, const ClimateSample &climate);
    static Biome* getBiome(const TerrainSample &terrain, const ClimateSample &climate, int worldX, int worldZ, int y);

    // Distance-based weighted blend untuk semua biome normal (non-Hell)
    // pDepth: smoothstep island depth (0=tepi, 1=tengah) — tepi dipaksa plains
    static BiomeBlend getBlend(const TerrainSample &terrain, const ClimateSample &climate, float pDepth = 1.0f);

    // Pilih block dengan noise-dither berdasarkan blend weights
    static BlockType blendedTopBlock   (const BiomeBlend &blend, int worldX, int worldZ);
    static BlockType blendedMiddleBlock(const BiomeBlend &blend, int worldX, int worldZ);
};
