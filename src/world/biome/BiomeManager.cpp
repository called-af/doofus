#include "BiomeManager.h"

#include "../../core/Setting.h"
#include "desert/DesertBiome.h"
#include "mountain/MountainBiome.h"
#include "plain/PlainBiome.h"
#include "cliff/CliffBiome.h"
#include "hell/HellBiome.h"
#include "../terrain/HellTerrain.h"
#include "../noise/FBMNoise.h"

#include <cmath>
#include <algorithm>

// ── Singleton biome instances ─────────────────────────────────────────────
static PlainBiome    plain;
static DesertBiome   desert;
static MountainBiome mountainRock;
static CliffBiome    cliff;

// ── Dominant getBiome (untuk info debug, overlay, dsb) ─────────────────────
Biome* BiomeManager::getBiome(const TerrainSample &terrain, const ClimateSample &climate)
{
    float mountainScore = terrain.peaks;
    if (terrain.mountainMassif > Setting::massifThreshold)
    {
        float massifNorm = (terrain.mountainMassif - Setting::massifThreshold) /
                           (1.0f - Setting::massifThreshold);
        mountainScore = std::max(mountainScore, massifNorm * 0.9f);
    }

    if (mountainScore > Setting::mountainThreshold)
        return &mountainRock;

    if (climate.temperature > Setting::desertTemperature &&
        climate.humidity    < Setting::desertHumidity)
        return &desert;

    return &plain;
}

Biome* BiomeManager::getBiome(const TerrainSample &terrain, const ClimateSample &climate,
                              int worldX, int worldZ, int y)
{
    if (y <= Setting::hellCanyonRimY)
    {
        float canyonRatio = HellTerrain::getCanyonDepthRatio(
            static_cast<float>(worldX), static_cast<float>(worldZ));
        if (canyonRatio > 0.12f)
            return HellTerrain::getBiome(worldX, worldZ);
    }
    return getBiome(terrain, climate);
}

// ── Multi-biome blend weights ─────────────────────────────────────────────
BiomeBlend BiomeManager::getBlend(const TerrainSample &terrain, const ClimateSample &climate, float pDepth)
{
    // 1. Hitung kontribusi Mountain berdasarkan peaks & mountainMassif
    float mountainScore = terrain.peaks;
    if (terrain.mountainMassif > Setting::massifThreshold)
    {
        float massifNorm = (terrain.mountainMassif - Setting::massifThreshold) /
                           (1.0f - Setting::massifThreshold);
        mountainScore = std::max(mountainScore, massifNorm * 0.9f);
    }

    const float mMin = Setting::mountainThreshold - Setting::mountainBlendWidth;
    const float mMax = Setting::mountainThreshold + Setting::mountainBlendWidth;
    const float mNorm = std::clamp((mountainScore - mMin) / (mMax - mMin), 0.0f, 1.0f);
    float wMountain = mNorm * mNorm * (3.0f - 2.0f * mNorm); // smoothstep

    // 2. Hitung kontribusi Desert vs Plains dari suhu dan kelembaban
    // Desert: temperatur tinggi dan kelembaban rendah
    const float dTemp = climate.temperature - Setting::desertTemperature;
    const float dHum  = Setting::desertHumidity - climate.humidity;
    const float desertMargin = std::min(dTemp, dHum);

    const float cMin = -Setting::climateBlendWidth;
    const float cMax =  Setting::climateBlendWidth;
    const float dNorm = std::clamp((desertMargin - cMin) / (cMax - cMin), 0.0f, 1.0f);
    const float desertFactor = dNorm * dNorm * (3.0f - 2.0f * dNorm); // smoothstep

    // Sisa bobot dialokasikan ke Desert dan Plains
    float wDesert = (1.0f - wMountain) * desertFactor;
    float wPlains = (1.0f - wMountain) * (1.0f - desertFactor);

    // 3. EDGE PLAINS BOOST
    // Di tepi daratan/pulau, paksa Plains (rumput) agar tepi tebing terlihat asri
    const float edgeFactor = std::clamp(1.0f - pDepth / Setting::edgePlainsDepth, 0.0f, 1.0f);
    const float edgeBoost  = edgeFactor * edgeFactor * (3.0f - 2.0f * edgeFactor);

    if (edgeBoost > 1e-4f)
    {
        wPlains   = wPlains * (1.0f - edgeBoost) + edgeBoost;
        wMountain = wMountain * (1.0f - edgeBoost);
        wDesert   = wDesert * (1.0f - edgeBoost);
    }

    // 4. Susun BiomeBlend terurut berdasarkan bobot terbesar
    struct BiomeCandidate {
        Biome* biome;
        float  weight;
    };
    BiomeCandidate candidates[3] = {
        { &plain,        wPlains   },
        { &desert,       wDesert   },
        { &mountainRock, wMountain }
    };

    std::sort(std::begin(candidates), std::end(candidates),
              [](const BiomeCandidate &a, const BiomeCandidate &b) {
                  return a.weight > b.weight;
              });

    BiomeBlend result;
    float total = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (candidates[i].weight > 0.005f && result.count < kMaxBlendBiomes)
        {
            result.biomes[result.count]  = candidates[i].biome;
            result.weights[result.count] = candidates[i].weight;
            total += candidates[i].weight;
            ++result.count;
        }
    }

    if (result.count > 0 && total > 1e-6f)
    {
        for (int i = 0; i < result.count; ++i)
            result.weights[i] /= total;
    }
    else
    {
        result.biomes[0]  = &plain;
        result.weights[0] = 1.0f;
        result.count      = 1;
    }

    return result;
}

// ── Hash koordinat deterministik untuk per-block random dither ─────────────
static inline uint32_t hashCoords(int x, int z, uint32_t seed)
{
    uint32_t h = seed ^ (static_cast<uint32_t>(x) * 0x27d4eb2du) ^ (static_cast<uint32_t>(z) * 0x165667b1u);
    h = (h ^ (h >> 15)) * 0x85ebca6bu;
    h = (h ^ (h >> 13)) * 0xc2b2ae35u;
    return h ^ (h >> 16);
}

static inline float blockDither(int worldX, int worldZ, uint32_t seedOffset)
{
    uint32_t h = hashCoords(worldX, worldZ, static_cast<uint32_t>(Setting::seed) + seedOffset);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

// ── Pilih biome pemenang per kolom (X, Z) ─────────────────────────────────
// Menghasilkan campuran blok acak ("block random nyampur") di area perbatasan
static Biome* pickBiome(const BiomeBlend &blend, int worldX, int worldZ)
{
    if (blend.count <= 1)
        return blend.biomes[0];

    // Variasi acak per-blok (white noise) dikombinasikan dengan noise kontur halus
    // agar terbentuk campuran blok acak dengan kluster-kluster alami
    const float white = blockDither(worldX, worldZ, 3333);
    const float noise = (FBMNoise::generate(
        worldX * Setting::biomeBlendNoiseScale,
        worldZ * Setting::biomeBlendNoiseScale,
        2, 0.5f, 1.0f, Setting::seed + 4444) + 1.0f) * 0.5f;

    const float hash = std::clamp(white * 0.78f + noise * 0.22f, 0.0f, 0.9999f);

    float cumulative = 0.0f;
    for (int i = 0; i < blend.count; ++i)
    {
        cumulative += blend.weights[i];
        if (hash < cumulative)
            return blend.biomes[i];
    }
    return blend.biomes[blend.count - 1];
}

BlockType BiomeManager::blendedTopBlock(const BiomeBlend &blend, int worldX, int worldZ)
{
    Biome* b = pickBiome(blend, worldX, worldZ);
    return b ? b->getTopBlock() : BlockType::Grass;
}

BlockType BiomeManager::blendedMiddleBlock(const BiomeBlend &blend, int worldX, int worldZ)
{
    Biome* b = pickBiome(blend, worldX, worldZ);
    return b ? b->getMiddleBlock() : BlockType::Dirt;
}
