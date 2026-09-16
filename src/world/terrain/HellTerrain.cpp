#include "HellTerrain.h"

#include "../biome/hell/HellBiome.h"
#include "../noise/CellularNoise.h"
#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "TerrainSampler.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <cmath>
#include <limits>

static HellLavaVeinBiome s_lavaVeinBiome;
static HellBasaltCragsBiome s_basaltCragsBiome;
static HellAshlandsBiome s_ashlandsBiome;

namespace
{
constexpr float kRiverWarpScale = 0.0040f;
constexpr float kRiverWarpStrength = 95.0f;
constexpr float kContinentMargin = 0.12f;
constexpr float kContinentAvoidance = 190.0f;

struct RiverSample
{
    float falloff;
    float dist;
    float width;
    float pool;
    float hellMask;
};

inline float smooth01(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float hellProvinceMask(float worldX, float worldZ)
{
    const float a = FBMNoise::generate(
        worldX, worldZ, 3, 0.5f, 0.00048f, Setting::seed + 4601);
    const float b = FBMNoise::generate(
        worldX + 4300.0f, worldZ - 2700.0f,
        2, 0.5f, 0.00112f, Setting::seed + 4602);
    const float field = (a * 0.72f + b * 0.28f) * 0.5f + 0.5f;
    return smooth01((field - 0.40f) / 0.22f);
}

inline float continentMask(float worldX, float worldZ)
{
    const TerrainSample terrain = TerrainSampler::sample(
        static_cast<int>(std::lround(worldX)),
        static_cast<int>(std::lround(worldZ)));
    return smooth01(
        (terrain.plateau - (Setting::plateauThreshold - kContinentMargin)) /
        kContinentMargin);
}

inline void warpRiverCoords(float worldX, float worldZ, float &outX, float &outZ)
{
    outX = worldX + FBMNoise::generate(
        worldX * kRiverWarpScale, worldZ * kRiverWarpScale,
        3, 0.5f, 0.5f, Setting::seed + 4101) * kRiverWarpStrength;
    outZ = worldZ + FBMNoise::generate(
        worldX * kRiverWarpScale + 311.0f,
        worldZ * kRiverWarpScale - 177.0f,
        3, 0.5f, 0.5f, Setting::seed + 4155) * kRiverWarpStrength;
}

inline float channelFalloff(float ridge, float bandWidth)
{
    const float t = std::clamp(
        (ridge - (1.0f - bandWidth)) / bandWidth, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

RiverSample sampleRiverNetwork(float worldX, float worldZ)
{
    static thread_local float cachedX = std::numeric_limits<float>::quiet_NaN();
    static thread_local float cachedZ = std::numeric_limits<float>::quiet_NaN();
    static thread_local RiverSample cached{};

    if (worldX == cachedX && worldZ == cachedZ)
        return cached;

    cachedX = worldX;
    cachedZ = worldZ;

    RiverSample sample{};
    sample.hellMask = hellProvinceMask(worldX, worldZ);
    const float continent = continentMask(worldX, worldZ);

    float sampleX = worldX;
    float sampleZ = worldZ;
    if (continent > 0.002f)
    {
        constexpr float gradientStep = 28.0f;
        float gradientX = continentMask(worldX + gradientStep, worldZ) -
                          continentMask(worldX - gradientStep, worldZ);
        float gradientZ = continentMask(worldX, worldZ + gradientStep) -
                          continentMask(worldX, worldZ - gradientStep);
        const float length = std::sqrt(gradientX * gradientX + gradientZ * gradientZ);
        if (length > 1e-5f)
        {
            gradientX /= length;
            gradientZ /= length;
            const float push = kContinentAvoidance * continent;
            sampleX += gradientX * push;
            sampleZ += gradientZ * push;
        }
    }

    float wx = 0.0f;
    float wz = 0.0f;
    warpRiverCoords(sampleX, sampleZ, wx, wz);

    const float trunkRidge = RidgeNoise::generate(
        wx, wz, 2, 0.5f, 0.0038f, Setting::seed + 4201);
    const float branchRidge = RidgeNoise::generate(
        wx + 1200.0f, wz - 860.0f,
        3, 0.5f, 0.0079f, Setting::seed + 4301);
    const float creekRidge = RidgeNoise::generate(
        wx - 2400.0f, wz + 1580.0f,
        3, 0.5f, 0.0154f, Setting::seed + 4401);
    const float capRidge = RidgeNoise::generate(
        wx + 3900.0f, wz + 2250.0f,
        2, 0.5f, 0.0290f, Setting::seed + 4451);

    const float widthMod = 0.80f + 0.45f * (
        (FBMNoise::generate(worldX, worldZ, 2, 0.5f, 0.0012f,
                             Setting::seed + 4501) + 1.0f) * 0.5f);

    const float trunk = channelFalloff(trunkRidge, 0.20f);
    const float branch = channelFalloff(branchRidge, 0.16f);
    const float creek = channelFalloff(creekRidge, 0.12f);
    const float cap = channelFalloff(capRidge, 0.10f);

    float best = trunk;
    float bestWidth = 17.0f * widthMod;
    if (branch * 0.88f > best)
    {
        best = branch * 0.88f;
        bestWidth = 11.0f * widthMod;
    }
    if (creek * 0.70f > best)
    {
        best = creek * 0.70f;
        bestWidth = 7.0f * widthMod;
    }
    if (cap * 0.68f > best)
    {
        best = cap * 0.68f;
        bestWidth = 4.5f * widthMod;
    }

    sample.pool = std::clamp(
        trunk * branch * 1.1f + branch * creek * 0.7f,
        0.0f, 1.0f);
    const float rawFalloff = std::clamp(
        std::max(best, sample.pool * 0.85f), 0.0f, 1.0f);
    // The ridge network itself is the source of truth for river existence.
    // Province controls surrounding crust materials, but must not erase rivers.
    float falloff = rawFalloff;
    if (continent > 0.75f)
    {
        falloff *= smooth01((1.0f - continent) / 0.25f);
    }
    sample.falloff = std::clamp(
        falloff, 0.0f, 1.0f);
    sample.width = bestWidth;
    sample.dist = (1.0f - sample.falloff) * std::max(3.0f, sample.width);
    cached = sample;
    return cached;
}
}

static float getNearestBranchInfo(float worldX, float worldZ, float &outWidth)
{
    const RiverSample sample = sampleRiverNetwork(worldX, worldZ);
    outWidth = sample.width;
    return sample.dist;
}

static float getMaterialTransitionRatio(float worldX, float worldZ, float canyonRatio)
{
    float branchWidth = 30.0f;
    const float nearestDist = getNearestBranchInfo(worldX, worldZ, branchWidth);
    const float transitionWidth = branchWidth + 10.0f;
    if (nearestDist >= transitionWidth)
        return canyonRatio;

    const float norm = nearestDist / transitionWidth;
    const float transitionRatio = 1.0f - (norm * norm * (3.0f - 2.0f * norm));
    return std::max(canyonRatio, transitionRatio);
}

static int sampleHellBankHeight(int worldX, int worldZ)
{
    const float hillNoise1 = FBMNoise::generate(
        worldX * Setting::baseHillScale1,
        worldZ * Setting::baseHillScale1,
        3, 0.5f, 0.5f, Setting::seed + 901);
    const float hillNoise2 = FBMNoise::generate(
        (worldX + 128.0f) * Setting::baseHillScale2,
        (worldZ + 192.0f) * Setting::baseHillScale2,
        2, 0.5f, 0.5f, Setting::seed + 902);
    const float hillField = std::clamp(
        (hillNoise1 * 0.72f + hillNoise2 * 0.28f) * 0.5f + 0.5f,
        0.0f, 1.0f);
    return Setting::hellFloorBaseY + static_cast<int>(std::round(
        std::pow(hillField, 1.3f) * static_cast<float>(Setting::baseHillHeight)));
}

float HellTerrain::getSpineX(float worldZ)
{
    float bestX = 0.0f;
    float bestRatio = -1.0f;
    for (int x = -640; x <= 640; x += 4)
    {
        const float ratio = sampleRiverNetwork(static_cast<float>(x), worldZ).falloff;
        if (ratio > bestRatio)
        {
            bestRatio = ratio;
            bestX = static_cast<float>(x);
        }
    }
    return bestX;
}

float HellTerrain::getCanyonDepthRatio(float worldX, float worldZ)
{
    return sampleRiverNetwork(worldX, worldZ).falloff;
}

float HellTerrain::getHellProvince(float worldX, float worldZ)
{
    return sampleRiverNetwork(worldX, worldZ).hellMask;
}

float HellTerrain::getCrackIntensity(float worldX, float worldZ, float canyonRatio)
{
    if (canyonRatio <= 0.12f)
        return 0.0f;

    float warpX = FBMNoise::generate(worldX * Setting::hellCrackWarpScale, worldZ * Setting::hellCrackWarpScale, 3, 0.55f, 0.5f, Setting::seed + 311) * Setting::hellCrackWarpStrength;
    float warpZ = FBMNoise::generate(worldX * Setting::hellCrackWarpScale + 250.0f, worldZ * Setting::hellCrackWarpScale + 250.0f, 3, 0.55f, 0.5f, Setting::seed + 411) * Setting::hellCrackWarpStrength;

    float wx = worldX + warpX;
    float wz = worldZ + warpZ;
    float branchWidth = 30.0f;
    const float distToSpine = getNearestBranchInfo(worldX, worldZ, branchWidth);
    float centerBias = 1.0f - std::clamp(distToSpine / (branchWidth * 0.95f), 0.0f, 1.0f);
    float centerRiver = std::pow(std::max(0.0f, centerBias), 1.6f);

    float laneA = RidgeNoise::generate(wx * 0.09f + 90.0f, wz * 0.09f + 40.0f, 3, 0.55f, 0.08f, Setting::seed + 721);
    float laneB = RidgeNoise::generate((wx + 180.0f) * 0.12f, (wz + 90.0f) * 0.12f, 3, 0.55f, 0.08f, Setting::seed + 822);
    float laneC = RidgeNoise::generate((wx + 360.0f) * 0.15f, (wz + 180.0f) * 0.15f, 3, 0.55f, 0.08f, Setting::seed + 923);
    float branchLanes = std::max(0.0f, laneA * 0.62f + laneB * 0.58f + laneC * 0.52f - 0.22f) * 1.6f;

    CellularSample cell = CellularNoise::generate(wx * Setting::hellCrackScale, wz * Setting::hellCrackScale, Setting::seed + 622);
    float edgeDist = cell.f2 - cell.f1;
    float voronoiVein = 1.0f - std::clamp(edgeDist / Setting::hellCrackWidth, 0.0f, 1.0f);
    voronoiVein = voronoiVein * voronoiVein * (3.0f - 2.0f * voronoiVein);

    float ridgeVein1 = RidgeNoise::generate(wx * 0.06f + 120.0f, wz * 0.06f + 120.0f, 3, 0.55f, 0.04f, Setting::seed + 733);
    float ridgeVein2 = RidgeNoise::generate(wx * 0.13f + 340.0f, wz * 0.13f + 340.0f, 2, 0.50f, 0.05f, Setting::seed + 844);
    float vascularBranch = std::max(0.0f, (ridgeVein1 * 0.78f + ridgeVein2 * 0.68f) - 0.22f) * 1.6f;

    float fineCracks = RidgeNoise::generate(wx * 0.18f + 70.0f, wz * 0.18f + 70.0f, 2, 0.5f, 0.06f, Setting::seed + 955);
    float capillaries = std::max(0.0f, fineCracks - 0.42f) * 1.5f;

    float basinField = std::max(0.0f, centerRiver * 1.5f + branchLanes * 0.6f + vascularBranch * 0.45f + capillaries * 0.25f - 0.25f) * 1.5f;
    float crackValue = std::max(voronoiVein * 0.9f, std::max(vascularBranch * 0.95f, std::max(branchLanes, basinField)));

    float canyonMod = std::clamp((canyonRatio - 0.08f) / 0.42f, 0.0f, 1.0f);
    return std::clamp(crackValue * canyonMod + centerRiver * 0.1f, 0.0f, 1.0f);
}

int HellTerrain::sampleFloorHeight(int worldX, int worldZ)
{
    float canyonRatio = getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    const int floorY = Setting::hellCanyonFloorY;
    int baseFloorY = sampleHellBankHeight(worldX, worldZ);

    // Small undulating hills on the base hell floor, fading out near the canyon rim.
    // This adds variation to the underworld bedrock without disturbing the main canyon profile.
    const int leftBankY = sampleHellBankHeight(worldX - 64, worldZ);
    const int rightBankY = sampleHellBankHeight(worldX + 64, worldZ);
    const int bankHeight = (leftBankY + rightBankY) / 2;
    const int adaptiveDrop = 2 + static_cast<int>(canyonRatio * 8.0f);
    baseFloorY = std::min(baseFloorY, bankHeight - adaptiveDrop);
    const int adaptiveFloorY = std::max(floorY, bankHeight - 14);

    if (canyonRatio > 0.0f)
    {
        baseFloorY = std::max(floorY, static_cast<int>(baseFloorY - canyonRatio * (baseFloorY - floorY)));
    }

    if (canyonRatio > 0.12f)
    {
        const float crackIntensity = getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);

        float branchWidth = 30.0f;
        const float distToSpine = getNearestBranchInfo(
            static_cast<float>(worldX), static_cast<float>(worldZ), branchWidth);
        const float centerBias = 1.0f - std::clamp(
            distToSpine / (branchWidth * 0.95f), 0.0f, 1.0f);

        const float poolNoise = FBMNoise::generate(
            (worldX + 78.0f) * 0.018f,
            (worldZ + 143.0f) * 0.018f,
            4, 0.55f, 0.5f, Setting::seed + 881);
        const float mergedCenter = std::clamp(centerBias * 1.3f + crackIntensity * 0.9f + poolNoise * 0.7f - 0.55f, 0.0f, 1.0f);

        if (crackIntensity >= 0.36f && canyonRatio > 0.12f && mergedCenter > 0.42f)
        {
            const int poolDepth = static_cast<int>((mergedCenter - 0.42f) * 30.0f + 5.0f + centerBias * 5.0f);
            baseFloorY = std::max(adaptiveFloorY, baseFloorY - poolDepth);
        }

        if (crackIntensity >= 0.62f)
        {
            baseFloorY = std::max(adaptiveFloorY, baseFloorY - 2);
        }
        else if (crackIntensity >= 0.42f)
        {
            baseFloorY = std::max(adaptiveFloorY, baseFloorY - 1);
        }

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

// ── Hash koordinat deterministik untuk per-block random dither di Hell ─────
static inline uint32_t hashHellCoords(int x, int z, uint32_t seed)
{
    uint32_t h = seed ^ (static_cast<uint32_t>(x) * 0x27d4eb2du) ^ (static_cast<uint32_t>(z) * 0x165667b1u);
    h = (h ^ (h >> 15)) * 0x85ebca6bu;
    h = (h ^ (h >> 13)) * 0xc2b2ae35u;
    return h ^ (h >> 16);
}

static inline float hellBlockDither(int worldX, int worldZ, uint32_t seedOffset)
{
    uint32_t h = hashHellCoords(worldX, worldZ, static_cast<uint32_t>(Setting::seed) + seedOffset);
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

static inline float getHellDitherHash(int worldX, int worldZ)
{
    const float white = hellBlockDither(worldX, worldZ, 6666);
    const float noise = (FBMNoise::generate(
        worldX * 0.045f, worldZ * 0.045f,
        3, 0.55f, 1.0f, Setting::seed + 7777) + 1.0f) * 0.5f;
    return std::clamp(white * 0.35f + noise * 0.65f, 0.0f, 0.9999f);
}

BlockType HellTerrain::sampleSurfaceBlock(int worldX, int worldZ, float canyonRatio, float crackIntensity)
{
    const float materialRatio = getMaterialTransitionRatio(
        static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
    const float hash = getHellDitherHash(worldX, worldZ);
    const float canyonBlend = std::clamp(
        materialRatio / 0.50f, 0.0f, 1.0f);
    const float smoothCanyonBlend = canyonBlend * canyonBlend * (3.0f - 2.0f * canyonBlend);

    // Break up long uniform river strips while keeping the material changes gradual.
    const float materialVariation = FBMNoise::generate(
        (worldX + 420.0f) * 0.045f,
        (worldZ - 180.0f) * 0.045f,
        3, 0.55f, 0.5f, Setting::seed + 1212);
    const float horizontalPatch = (FBMNoise::generate(
        (worldX - 260.0f) * 0.045f,
        (worldZ + 310.0f) * 0.045f,
        3, 0.55f, 0.5f, Setting::seed + 1313) + 1.0f) * 0.5f;
    const float edgeMaterialLift = smoothCanyonBlend * 0.32f;
    const float localCrack = std::clamp(
        crackIntensity + edgeMaterialLift + materialVariation * 0.12f * smoothCanyonBlend +
            (horizontalPatch - 0.5f) * 0.18f * smoothCanyonBlend,
        0.0f, 1.0f);

    // The closest threshold gap is 0.16; 0.078 gives a 0.156-wide band
    // without overlapping the next material boundary.
    const float blendDelta = 0.078f;

    // Thin volcanic fissures break the river crust into visible rock cracks.
    // They only appear where a real lava vein already exists, avoiding random
    // lava touching the outer biome.
    const float rockFissure = RidgeNoise::generate(
        worldX * 0.115f + 180.0f,
        worldZ * 0.115f - 90.0f,
        2, 0.5f, 0.055f, Setting::seed + 1661);
    const float fissureBoost = (materialRatio > 0.24f && rockFissure > 0.70f)
        ? 0.15f
        : 0.0f;

    const CellularSample raftCell = CellularNoise::generate(
        (worldX + 1450.0f) * 0.013f,
        (worldZ - 720.0f) * 0.013f,
        Setting::seed + 2424);
    const float raftCore = std::clamp(
        (raftCell.f2 - raftCell.f1) / 0.55f, 0.0f, 1.0f);
    const float raftNoise = (FBMNoise::generate(
        (worldX + 930.0f) * 0.030f,
        (worldZ - 450.0f) * 0.030f,
        4, 0.55f, 0.5f, Setting::seed + 2525) + 1.0f) * 0.5f;
    float raftMask = std::clamp(
        (raftCore * 0.65f + raftNoise * 0.55f - 0.52f) / 0.38f,
        0.0f, 1.0f);
    raftMask = raftMask * raftMask * (3.0f - 2.0f * raftMask);

    const float raftCrackLine = RidgeNoise::generate(
        worldX * 0.155f - 610.0f,
        worldZ * 0.155f + 275.0f,
        3, 0.5f, 0.05f, Setting::seed + 2626);
    const float seep = std::clamp(
        (raftCrackLine - 0.68f) / 0.22f, 0.0f, 1.0f);
    raftMask *= 1.0f - seep * 0.85f;

    // Rakit hanya mengukir tepian, tidak menutup inti sungai.
    const float raftDepth = 0.20f + 0.18f * raftCore;
    const float raftCarve = raftMask * raftDepth *
                            std::clamp(materialRatio / 0.30f, 0.0f, 1.0f);
    const float riverCore = smooth01(
        (canyonRatio - 0.30f) / 0.35f);
    const float blendedCrack = std::clamp(
        std::max(localCrack + fissureBoost - raftCarve, riverCore * 0.90f),
        0.0f, 1.0f);

    // Narrow solid-rock fissures in the river center. These stay dark and
    // volcanic instead of becoming another open lava strip.
    const float volcanicRockCrack = RidgeNoise::generate(
        worldX * 0.17f + 520.0f,
        worldZ * 0.17f - 260.0f,
        2, 0.5f, 0.045f, Setting::seed + 1772);
    if (canyonRatio > 0.35f && volcanicRockCrack > 0.76f &&
        blendedCrack < Setting::hellLavaCrackThreshold - blendDelta)
    {
        return (hash < 0.72f) ? BlockType::Obsidian : BlockType::Ash;
    }

    // 1. Transisi Cinder <-> Lava di pusat rekahan magma
    const float tLava = Setting::hellLavaCrackThreshold;
    if (blendedCrack >= tLava + blendDelta)
        return BlockType::Lava;
    if (blendedCrack >= tLava - blendDelta)
    {
        float t = (blendedCrack - (tLava - blendDelta)) / (2.0f * blendDelta);
        float p = t * t * (3.0f - 2.0f * t); // smoothstep
        return (hash < p) ? BlockType::Lava : BlockType::Cinder;
    }

    // 2. Transisi Obsidian <-> Cinder di tepian rekahan lava
    const float tCinder = Setting::hellCinderThreshold;
    if (blendedCrack >= tCinder + blendDelta)
        return BlockType::Cinder;
    if (blendedCrack >= tCinder - blendDelta)
    {
        float t = (blendedCrack - (tCinder - blendDelta)) / (2.0f * blendDelta);
        float p = t * t * (3.0f - 2.0f * t);
        return (hash < p) ? BlockType::Cinder : BlockType::Obsidian;
    }

    // 3. Transisi Ash <-> Obsidian di area kerak lava yang mendingin
    const float tObsidian = Setting::hellObsidianThreshold;
    if (blendedCrack >= tObsidian + blendDelta)
        return BlockType::Obsidian;
    if (blendedCrack >= tObsidian - blendDelta)
    {
        float t = (blendedCrack - (tObsidian - blendDelta)) / (2.0f * blendDelta);
        float p = t * t * (3.0f - 2.0f * t);
        return (hash < p) ? BlockType::Obsidian : BlockType::Ash;
    }

    // 4. Transisi Basalt <-> Ash di batas dataran abu vulkanik
    const float tAsh = Setting::hellAshThreshold;
    if (blendedCrack >= tAsh + blendDelta)
        return BlockType::Ash;
    if (blendedCrack >= tAsh - blendDelta)
    {
        float t = (blendedCrack - (tAsh - blendDelta)) / (2.0f * blendDelta);
        float p = t * t * (3.0f - 2.0f * t);
        return (hash < p) ? BlockType::Ash : BlockType::Basalt;
    }

    // 5. Kerak luar basalt
    return BlockType::Basalt;
}

BlockType HellTerrain::sampleBlock(int worldX, int worldZ, int surfaceHeight)
{
    float canyonRatio = getCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    float crackIntensity = getCrackIntensity(static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
    return sampleSurfaceBlock(worldX, worldZ, canyonRatio, crackIntensity);
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
    (void)floorH;
    int baseFloorY = sampleFloorHeight(worldX, worldZ);
    const float materialRatio = getMaterialTransitionRatio(
        static_cast<float>(worldX), static_cast<float>(worldZ), canyonRatio);
    const float province = getHellProvince(
        static_cast<float>(worldX), static_cast<float>(worldZ));

    // Transisi dither acak Stone <-> Basalt di perbatasan canyon brim
    const float cNorm = std::clamp(materialRatio / 0.28f, 0.0f, 1.0f);
    const float cProb = cNorm * cNorm * (3.0f - 2.0f * cNorm) * province;
    const float baseHash = hellBlockDither(worldX, worldZ, 8888);
    const bool isHellBasalt = (baseHash < cProb);

    const bool inHellCanyon = canyonRatio > 0.12f;
    for (int y = 0; y <= baseFloorY; ++y)
    {
        if (isHellBasalt || inHellCanyon)
        {
            const int depthFromSurface = baseFloorY - y;
            const BlockType surfaceMaterial = sampleSurfaceBlock(
                worldX, worldZ, materialRatio, crackIntensity);

            // Always keep a basalt -> ash/obsidian -> lava stack under a
            // fissure, so normal stone never borders exposed lava directly.
            if (depthFromSurface >= 3)
                chunk.blocks[x][y][z] = BlockType::Basalt;
            else if (surfaceMaterial == BlockType::Lava)
                chunk.blocks[x][y][z] = (depthFromSurface == 0)
                    ? BlockType::Lava
                    : (depthFromSurface == 1 ? BlockType::Obsidian : BlockType::Ash);
            else if (depthFromSurface == 0)
                chunk.blocks[x][y][z] = surfaceMaterial;
            else if (depthFromSurface == 1 &&
                     (surfaceMaterial == BlockType::Obsidian || surfaceMaterial == BlockType::Cinder))
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
    chunk.blocks[x][y][z] = sampleSurfaceBlock(worldX, worldZ, canyonRatio, crackIntensity);
}
