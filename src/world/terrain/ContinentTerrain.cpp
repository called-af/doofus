#include "ContinentTerrain.h"

#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <cmath>

int ContinentTerrain::computeBaseHeight(const TerrainSample &t)
{
    int floorH = 14 + static_cast<int>(t.continentalness * 6.0f);
    if (t.river < Setting::riverThreshold)
    {
        const float rf = 1.0f - (t.river / Setting::riverThreshold);
        floorH -= static_cast<int>(rf * rf * 6.0f);
    }
    return std::clamp(floorH, 3, Chunk::HEIGHT - 1);
}

int ContinentTerrain::applyErosion(int h, const TerrainSample &t)
{
    float erosionMult = 1.0f - (t.plateau * 0.8f);
    erosionMult = std::max(0.2f, erosionMult);
    return h - static_cast<int>(t.erosion * Setting::erosionStrength * erosionMult);
}

int ContinentTerrain::estimateBodyBottom(int flatPlateauH, float pDepth)
{
    constexpr int maxThickness = 48;
    constexpr int minThickness = 2;

    const float thicknessFactor = std::pow(pDepth, 1.8f);
    const int bodyThickness =
        minThickness +
        static_cast<int>(thicknessFactor * (maxThickness - minThickness));
    return flatPlateauH - bodyThickness;
}

int ContinentTerrain::sampleHeightAt(int worldX, int worldZ, const TerrainSample &t)
{
    const float raw = (t.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
    const float pDepth = std::clamp(raw, 0.0f, 1.0f);
    const float smoothDepth = pDepth * pDepth * (3.0f - 2.0f * pDepth);

    if (smoothDepth < Setting::islandEdgeCutoff)
        return 0;

    const float variation = (FBMNoise::generate((worldX + 54321) * 0.0018f,
                                                (worldZ + 12345) * 0.0018f,
                                                3, 0.5f, 0.5f, Setting::seed + 225) + 1.0f) * 0.5f;
    const int flatHeight = std::clamp(100 + static_cast<int>(variation * 75.0f), 100, 220);
    int height = flatHeight;

    if (smoothDepth > 0.35f)
    {
        const float blend = (smoothDepth - 0.35f) / 0.65f;
        const float smoothBlend = blend * blend * (3.0f - 2.0f * blend);
        const float peakBlend = std::max(0.0f, (blend - Setting::mountainCoreThreshold) / (1.0f - Setting::mountainCoreThreshold));
        height += static_cast<int>(smoothBlend * 40.0f) + static_cast<int>(peakBlend * std::sqrt(std::max(0.0f, t.peaks)) * Setting::peakHeight);
    }
    else
    {
        height += static_cast<int>(t.erosion * 0.5f);
    }

    return std::clamp(applyErosion(height, t), flatHeight - 4, 250);
}

int ContinentTerrain::sampleBodyBottomAt(int worldX, int worldZ, const TerrainSample &t, int baseFloorY)
{
    const float raw = (t.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
    const float pDepth = std::clamp(raw, 0.0f, 1.0f);
    const float smoothDepth = pDepth * pDepth * (3.0f - 2.0f * pDepth);

    const float variation = (FBMNoise::generate((worldX + 54321) * 0.0018f,
                                                (worldZ + 12345) * 0.0018f,
                                                3, 0.5f, 0.5f, Setting::seed + 225) + 1.0f) * 0.5f;
    const int flatPlateauH = std::clamp(100 + static_cast<int>(variation * 75.0f), 100, 220);
    return std::max(baseFloorY + 12, estimateBodyBottom(flatPlateauH, smoothDepth));
}

bool ContinentTerrain::isSolidAt(int worldX, int worldZ, int y, const TerrainSample &t, int baseFloorY)
{
    const float raw = (t.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
    const float pClamped = std::clamp(raw, 0.0f, 1.0f);
    const float pDepth = pClamped * pClamped * (3.0f - 2.0f * pClamped);

    if (pDepth < Setting::islandEdgeCutoff)
        return false;

    const int bodyBottom = sampleBodyBottomAt(worldX, worldZ, t, baseFloorY);
    if (y >= bodyBottom)
        return true;

    // Hourglass stem support
    const int stemTop = bodyBottom;
    const int stemBottom = baseFloorY + 1;
    const int stemHeight = stemTop - stemBottom;
    if (stemHeight <= 0)
        return false;

    const float tNorm = static_cast<float>(y - stemBottom) / static_cast<float>(stemHeight);
    const float midDist = std::abs(tNorm - 0.5f) * 2.0f;

    constexpr float waistRatio = 0.32f;
    const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);
    const float baseRequiredDepth = 1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

    const float rockDetail =
        FBMNoise::generate(worldX * 0.05f + y * 0.15f, worldZ * 0.05f + y * 0.15f, 2, 0.5f, 0.5f,
                           Setting::seed + 777) * 0.07f;
    const float verticalRidge =
        RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ + y * 2), 2, 0.5f, 0.03f,
                             Setting::seed + 888) * 0.04f;

    const float finalThreshold = baseRequiredDepth + rockDetail - verticalRidge;
    return pDepth >= finalThreshold;
}

void ContinentTerrain::generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                          const TerrainSample &terrain, float pDepth, int baseFloorY)
{
    if (pDepth < Setting::islandEdgeCutoff)
        return;

    const float n1 = FBMNoise::generate((worldX + 54321) * 0.0018f,
                                        (worldZ + 12345) * 0.0018f, 3, 0.5f,
                                        0.5f, Setting::seed + 225);
    const float hVariation = (n1 + 1.0f) * 0.5f;
    const int flatPlateauH = std::clamp(100 + static_cast<int>(hVariation * 75.0f), 100, 220);

    int h = flatPlateauH;
    constexpr float blendStart = 0.35f;

    if (pDepth > blendStart)
    {
        const float tBlend = (pDepth - blendStart) / (1.0f - blendStart);
        const float tSmooth = tBlend * tBlend * (3.0f - 2.0f * tBlend);
        const float peakBlend = std::max(0.0f, (tBlend - Setting::mountainCoreThreshold) / (1.0f - Setting::mountainCoreThreshold));
        const float peakFactor = std::sqrt(std::max(0.0f, terrain.peaks));

        h += static_cast<int>(tSmooth * 40.0f) +
             static_cast<int>(peakBlend * peakFactor * Setting::peakHeight);
    }
    else
    {
        h += static_cast<int>(terrain.erosion * 0.5f);
    }

    h = applyErosion(h, terrain);
    h = std::clamp(h, flatPlateauH - 4, 250);
    chunk.heightMap[x][z] = h;

    const int bodyBottom = std::max(baseFloorY + 12, estimateBodyBottom(flatPlateauH, pDepth));

    if (bodyBottom < h)
    {
        for (int y = bodyBottom; y <= h; ++y)
            chunk.blocks[x][y][z] = BlockType::Stone;

        const int stemTop = bodyBottom;
        const int stemBottom = baseFloorY + 1;
        const int stemHeight = stemTop - stemBottom;

        if (stemHeight > 0)
        {
            for (int y = stemBottom; y < stemTop; ++y)
            {
                const float tNorm = static_cast<float>(y - stemBottom) / static_cast<float>(stemHeight);
                const float midDist = std::abs(tNorm - 0.5f) * 2.0f;

                constexpr float waistRatio = 0.32f;
                const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);
                const float baseRequiredDepth = 1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

                const float rockDetail =
                    FBMNoise::generate(worldX * 0.05f + y * 0.15f, worldZ * 0.05f + y * 0.15f, 2, 0.5f, 0.5f,
                                       Setting::seed + 777) * 0.07f;
                const float verticalRidge =
                    RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ + y * 2), 2, 0.5f, 0.03f,
                                         Setting::seed + 888) * 0.04f;

                const float finalThreshold = baseRequiredDepth + rockDetail - verticalRidge;

                if (pDepth >= finalThreshold)
                {
                    chunk.blocks[x][y][z] = BlockType::Stone;
                }
            }
        }
    }
}

void ContinentTerrain::generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                             Biome* biome, int &layerDepth)
{
    chunk.blocks[x][y][z] = biome->getTopBlock();
    layerDepth = 3;
}
