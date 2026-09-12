#include "HeavenTerrain.h"

#include "../biome/heaven/HeavenBiome.h"
#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <cmath>

static HeavenPlainsBiome s_heavenPlainsBiome;
static HeavenCrystalBiome s_heavenCrystalBiome;

FeatureSeed HeavenTerrain::generateSeed(int cellX, int cellZ)
{
    FeatureSeed s{};
    uint32_t h1 = static_cast<uint32_t>(cellX * 73856093 ^ cellZ * 19349663 ^ Setting::seed * 83492791);
    uint32_t h2 = h1 * 1664525u + 1013904223u;
    uint32_t h3 = h2 * 1664525u + 1013904223u;
    uint32_t h4 = h3 * 1664525u + 1013904223u;
    uint32_t h5 = h4 * 1664525u + 1013904223u;
    uint32_t h6 = h5 * 1664525u + 1013904223u;
    uint32_t h7 = h6 * 1664525u + 1013904223u;
    uint32_t h8 = h7 * 1664525u + 1013904223u;

    if ((h7 % 100) > static_cast<int>(Setting::heavenSpawnChance * 100.0f))
    {
        s.exists = false;
        return s;
    }

    s.exists = true;
    const float g = Setting::heavenClusterSpacing;
    float rx = (static_cast<float>(h1 % 10000) / 10000.0f) * (g * 0.60f) + (g * 0.20f);
    float rz = (static_cast<float>(h2 % 10000) / 10000.0f) * (g * 0.60f) + (g * 0.20f);

    s.x = cellX * g + rx;
    s.z = cellZ * g + rz;
    s.angle = (static_cast<float>(h7 % 6283) / 1000.0f);
    s.aspect = 1.0f + (static_cast<float>(h8 % 120) / 100.0f);

    int sizeRoll = h3 % 100;
    if (sizeRoll < 15)
    {
        s.isGiant = true;
        s.isAnchor = true;
        s.radius = Setting::heavenGiantRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenGiantRadiusMax - Setting::heavenGiantRadiusMin);
        s.thickness = 55.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 25.0f;
    }
    else if (sizeRoll < 55)
    {
        s.isGiant = false;
        s.isAnchor = (h6 % 3 == 0);
        s.radius = Setting::heavenMediumRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenMediumRadiusMax - Setting::heavenMediumRadiusMin);
        s.thickness = 28.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 20.0f;
    }
    else
    {
        s.isGiant = false;
        s.isAnchor = false;
        s.radius = Setting::heavenSmallRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenSmallRadiusMax - Setting::heavenSmallRadiusMin);
        s.thickness = 12.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 12.0f;
    }

    s.basePosY = Setting::heavenMinBaseY + (static_cast<float>(h6 % 10000) / 10000.0f) * (Setting::heavenMaxBaseY - Setting::heavenMinBaseY);
    s.hasPeak = true;

    s.subIsletCount = 2 + (h4 % 3);
    uint32_t subH = h8;
    for (int i = 0; i < s.subIsletCount; ++i)
    {
        subH = subH * 1664525u + 1013904223u;
        float polarAngle = (static_cast<float>(subH % 6283) / 1000.0f);
        subH = subH * 1664525u + 1013904223u;
        float distMult = 0.85f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.75f;

        SubIslet &sub = s.subIslets[i];
        float dist = s.radius * distMult;
        sub.offsetX = dist * std::cos(polarAngle);
        sub.offsetZ = dist * std::sin(polarAngle);

        subH = subH * 1664525u + 1013904223u;
        sub.offsetY = Setting::heavenSubIsletYMin + (static_cast<float>(subH % 1000) / 1000.0f) * (Setting::heavenSubIsletYMax - Setting::heavenSubIsletYMin);

        subH = subH * 1664525u + 1013904223u;
        sub.radius = s.radius * (0.22f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.28f);

        subH = subH * 1664525u + 1013904223u;
        sub.thickness = s.thickness * (0.35f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.35f);
    }

    return s;
}

IslandSlice HeavenTerrain::evaluateSlice(float worldX, float worldZ, const FeatureSeed &s, int islandIndex)
{
    IslandSlice slice{};
    slice.valid = false;
    if (!s.exists)
        return slice;

    float cx, cz, R, T, baseY;
    bool isMain = (islandIndex == 0);

    if (isMain)
    {
        cx = s.x;
        cz = s.z;
        R = s.radius;
        T = s.thickness;
        baseY = s.basePosY;
    }
    else
    {
        const SubIslet &sub = s.subIslets[islandIndex - 1];
        cx = s.x + sub.offsetX;
        cz = s.z + sub.offsetZ;
        R = sub.radius;
        T = sub.thickness;
        baseY = s.basePosY + sub.offsetY;
    }

    float dxRaw = worldX - cx;
    float dzRaw = worldZ - cz;
    float angleNorm = std::atan2(dzRaw, dxRaw);

    float nLobe = FBMNoise::generate(std::cos(angleNorm * 2.5f) * 1.8f, std::sin(angleNorm * 2.5f) * 1.8f, 3, 0.55f, 0.5f, Setting::seed + 333) * 0.26f;

    float warpX = FBMNoise::generate(worldX * Setting::heavenWarpScale, worldZ * Setting::heavenWarpScale, 4, 0.55f, 0.5f, Setting::seed + 444) * (R * Setting::heavenWarpStrength);
    float warpZ = FBMNoise::generate(worldX * Setting::heavenWarpScale + 400.0f, worldZ * Setting::heavenWarpScale + 400.0f, 4, 0.55f, 0.5f, Setting::seed + 555) * (R * Setting::heavenWarpStrength);

    float dx = (worldX + warpX) - cx;
    float dz = (worldZ + warpZ) - cz;

    float cosA = std::cos(s.angle);
    float sinA = std::sin(s.angle);
    float rx = (dx * cosA - dz * sinA);
    float rz = (dx * sinA + dz * cosA) * (isMain ? s.aspect : 1.0f);

    float dist = std::hypot(rx, rz);
    float d = dist / R;

    float nShape = FBMNoise::generate(worldX * 0.028f, worldZ * 0.028f, 4, 0.55f, 0.5f, Setting::seed + 789) * 0.22f;
    float nDetail = RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ), 3, 0.5f, 0.04f, Setting::seed + 912) * 0.12f;

    slice.effectiveD = d + nLobe + nShape - nDetail;

    if (slice.effectiveD >= 1.0f)
        return slice;

    slice.valid = true;

    float edgeFactor = 1.0f;
    if (slice.effectiveD > Setting::heavenPlateauRatio)
    {
        float t = (slice.effectiveD - Setting::heavenPlateauRatio) / (1.0f - Setting::heavenPlateauRatio);
        edgeFactor = 1.0f - t * t * (3.0f - 2.0f * t);
    }
    slice.topY = baseY + T * 0.30f * edgeFactor;

    if (isMain && (s.isGiant || R >= 50.0f || s.hasPeak))
    {
        float coreFactor = std::pow(std::clamp(1.0f - slice.effectiveD / 0.70f, 0.0f, 1.0f), 1.5f);
        float mRidge = RidgeNoise::generate(worldX * Setting::islandMountainScale, worldZ * Setting::islandMountainScale, 4, 0.55f, 0.5f, Setting::seed + 1100);
        float mFbm   = FBMNoise::generate(worldX * 0.015f, worldZ * 0.015f, 3, 0.5f, 0.5f, Setting::seed + 1200);

        float baseMountainH = Setting::heavenAnchorPeakHeight + (s.isGiant ? 35.0f : 12.0f);
        float mountainH = coreFactor * baseMountainH * (0.50f + 0.50f * mRidge + 0.25f * mFbm);
        slice.topY += mountainH;

        slice.isAnchorPeak = (s.isAnchor && slice.effectiveD < 0.18f && coreFactor > 0.4f);
    }
    else
    {
        slice.isAnchorPeak = false;
    }

    float coneProfile = std::pow(std::clamp(1.0f - slice.effectiveD, 0.0f, 1.0f), Setting::heavenBellyExponent);
    float bellyNoise = RidgeNoise::generate(worldX * 0.038f, worldZ * 0.038f, 3, 0.5f, 0.05f, Setting::seed + 678) * 0.20f;
    
    float maxBellyDepth = T * 1.10f * (1.0f + bellyNoise);
    float bellyDepth = maxBellyDepth * coneProfile;
    
    slice.botY = slice.topY - std::max(3.0f, bellyDepth);
    return slice;
}

float HeavenTerrain::computeDistance(float worldX, float worldZ, const FeatureSeed &s)
{
    if (!s.exists)
        return 1e9f;

    float minDist = 1e9f;
    int totalIslands = 1 + s.subIsletCount;

    for (int i = 0; i < totalIslands; ++i)
    {
        IslandSlice slice = evaluateSlice(worldX, worldZ, s, i);
        minDist = std::min(minDist, slice.effectiveD);
    }

    return minDist;
}

FeatureSeed HeavenTerrain::findNearestSeed(float worldX, float worldZ, float &outDist)
{
    const float g = Setting::heavenClusterSpacing;
    int cellX = static_cast<int>(std::floor(worldX / g));
    int cellZ = static_cast<int>(std::floor(worldZ / g));

    FeatureSeed bestSeed{};
    float minDist = 1e9f;

    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dz = -1; dz <= 1; ++dz)
        {
            FeatureSeed s = generateSeed(cellX + dx, cellZ + dz);
            if (!s.exists)
                continue;

            float dist = computeDistance(worldX, worldZ, s);
            if (dist < minDist)
            {
                minDist = dist;
                bestSeed = s;
            }
        }
    }
    outDist = minDist;
    return bestSeed;
}

int HeavenTerrain::sampleHeightAt(float worldX, float worldZ)
{
    float hDist = 0.0f;
    FeatureSeed hSeed = findNearestSeed(worldX, worldZ, hDist);
    if (!hSeed.exists)
        return 0;

    float maxHeavenTop = -1.0f;
    int totalIslands = 1 + hSeed.subIsletCount;
    for (int i = 0; i < totalIslands; ++i)
    {
        IslandSlice slice = evaluateSlice(worldX, worldZ, hSeed, i);
        if (slice.valid && slice.topY > maxHeavenTop)
            maxHeavenTop = slice.topY;
    }
    if (maxHeavenTop > 0.0f)
        return std::clamp(static_cast<int>(maxHeavenTop), 280, Chunk::HEIGHT - 1);
    return 0;
}

BlockType HeavenTerrain::sampleBlock(float worldX, float worldZ, int surfaceHeight)
{
    float hDist = 0.0f;
    FeatureSeed s = findNearestSeed(worldX, worldZ, hDist);
    if (s.exists)
    {
        IslandSlice mainSlice = evaluateSlice(worldX, worldZ, s, 0);
        if (mainSlice.valid && mainSlice.isAnchorPeak)
            return BlockType::Crystal;
    }
    return BlockType::Grass;
}

Biome* HeavenTerrain::getBiome(float worldX, float worldZ, int y)
{
    float hDist = 0.0f;
    FeatureSeed s = findNearestSeed(worldX, worldZ, hDist);
    if (s.exists)
    {
        IslandSlice mainSlice = evaluateSlice(worldX, worldZ, s, 0);
        if (mainSlice.valid && mainSlice.isAnchorPeak && y > 350)
            return &s_heavenCrystalBiome;
    }
    return &s_heavenPlainsBiome;
}

void HeavenTerrain::generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                       const FeatureSeed &seed)
{
    if (!seed.exists)
        return;

    const int totalIslands = 1 + seed.subIsletCount;
    for (int i = 0; i < totalIslands; ++i)
    {
        IslandSlice slice = evaluateSlice(static_cast<float>(worldX), static_cast<float>(worldZ), seed, i);
        if (slice.valid)
        {
            const int iBottom = std::clamp(static_cast<int>(slice.botY), 280, Chunk::HEIGHT - 2);
            const int iTop = std::clamp(static_cast<int>(slice.topY), iBottom, Chunk::HEIGHT - 1);

            for (int y = iBottom; y <= iTop; ++y)
            {
                chunk.blocks[x][y][z] = BlockType::Heavenstone;
            }

            if (iTop > chunk.heightMap[x][z])
            {
                chunk.heightMap[x][z] = iTop;
            }
        }
    }
}

void HeavenTerrain::generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                         const FeatureSeed &seed, int &layerDepth)
{
    bool isCrystal = false;
    if (seed.exists)
    {
        IslandSlice slice0 = evaluateSlice(static_cast<float>(worldX), static_cast<float>(worldZ), seed, 0);
        if (slice0.valid && slice0.isAnchorPeak && y > 350)
            isCrystal = true;
    }

    if (isCrystal)
        chunk.blocks[x][y][z] = BlockType::Crystal;
    else
        chunk.blocks[x][y][z] = BlockType::Grass;
    layerDepth = 3;
}
