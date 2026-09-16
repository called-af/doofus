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

int ContinentTerrain::estimateBodyBottom(int topHeight, float pDepth, int worldX, int worldZ)
{
    constexpr int maxThickness = 80;
    constexpr int minThickness = 2;

    // pDepth raw (0..1): tepi kecil → tipis, tengah besar → tebal
    const float thicknessFactor = std::pow(pDepth, Setting::rimThinPower);
    int bodyThickness =
        minThickness +
        static_cast<int>(thicknessFactor * (maxThickness - minThickness));

    // Variasi organik per-kolom: noise modulate ketebalan rim atas
    // Frekuensi rendah = variasi lebar (bukan pixelated), amplitudo proporsional ke zone tepi
    // Di tengah (pDepth besar) variasi kecil karena rim sudah tebal; di tepi variasi terasa lebih dramatis
    const float rimVariationAmp = (1.0f - thicknessFactor) * Setting::rimVariationAmp;
    const float rimNoise = FBMNoise::generate(
        worldX * Setting::rimVariationScale,
        worldZ * Setting::rimVariationScale,
        3, 0.5f, 1.0f, Setting::seed + 2200);
    // rimNoise range -1..1, shift ke 0..1 lalu center ke -0.5..0.5
    const float rimShift = (rimNoise * 0.5f) * rimVariationAmp;
    bodyThickness = std::max(minThickness, bodyThickness + static_cast<int>(rimShift));

    return topHeight - bodyThickness;
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
    const int flatHeight = std::clamp(100 + static_cast<int>(variation * 100.0f), 100, 280);
    int height = flatHeight;

    if (smoothDepth > 0.35f)
    {
        // ── INNER ZONE: bukit + puncak ──────────────────────────────────
        const float blend = (smoothDepth - 0.35f) / 0.65f;
        const float smoothBlend = blend * blend * (3.0f - 2.0f * blend);

        // Lifted peaks: ganti raw ridge dengan (1-(1-p)^2) supaya
        // valley antar dua puncak tidak ikut drop tajam (saddle fill)
        const float rawPeaks = std::max(0.0f, t.peaks);
        const float liftedPeaks = 1.0f - (1.0f - rawPeaks) * (1.0f - rawPeaks * Setting::saddleFillStrength);

        const float peakBlend = std::max(0.0f, (blend - Setting::mountainCoreThreshold) /
                                               (1.0f - Setting::mountainCoreThreshold));
        height += static_cast<int>(smoothBlend * 60.0f) +
                  static_cast<int>(peakBlend * std::sqrt(liftedPeaks) * Setting::peakHeight);
    }
    else
    {
        // ── EDGE ZONE: tepi, thin dan bervariasi ─────────────────────────
        height += static_cast<int>(t.erosion * 0.5f);

        const float edgeFactor = 1.0f - (smoothDepth / 0.35f);
        const float edgeSmooth = edgeFactor * edgeFactor;

        const float undulation = FBMNoise::generate(
            worldX * Setting::edgeUndulationScale, worldZ * Setting::edgeUndulationScale,
            3, 0.55f, 1.0f, Setting::seed + 1500);
        height += static_cast<int>(undulation * edgeSmooth * Setting::edgeUndulationAmp);

        const float cliffRidge = RidgeNoise::generate(
            worldX * Setting::edgeCliffScale, worldZ * Setting::edgeCliffScale,
            2, 0.5f, 1.0f, Setting::seed + 1600);
        const float cliffDrop = std::max(0.0f, 0.6f - cliffRidge);
        height -= static_cast<int>(cliffDrop * edgeSmooth * Setting::edgeCliffDrop);

        const float bump = FBMNoise::generate(
            worldX * Setting::edgeBumpScale, worldZ * Setting::edgeBumpScale,
            2, 0.5f, 1.0f, Setting::seed + 1700);
        height += static_cast<int>(bump * edgeSmooth * Setting::edgeBumpAmp);
    }

    // MOUNTAIN MASSIF — gunung besar, hanya tumbuh di daratan cukup dalam
    if (smoothDepth > 0.25f && t.mountainMassif > Setting::massifThreshold)
    {
        const float massifRaw = (t.mountainMassif - Setting::massifThreshold) /
                                (1.0f - Setting::massifThreshold);
        const float massifCurve = std::pow(massifRaw, Setting::massifSharpness);
        // Depth gate: gunung lebih tinggi di tengah daratan
        const float depthGate = std::clamp((smoothDepth - 0.25f) / 0.75f, 0.0f, 1.0f);
        height += static_cast<int>(massifCurve * depthGate * Setting::massifMaxHeight);
    }

    return std::clamp(applyErosion(height, t), flatHeight - 4, Chunk::HEIGHT - 2);
}

int ContinentTerrain::sampleBodyBottomAt(int worldX, int worldZ, const TerrainSample &t, int baseFloorY)
{
    const float raw = (t.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
    const float pRaw = std::clamp(raw, 0.0f, 1.0f);
    const float pDepth = pRaw * pRaw * (3.0f - 2.0f * pRaw);

    const int topHeight = sampleHeightAt(worldX, worldZ, t);
    return std::max(baseFloorY + 12, estimateBodyBottom(topHeight, pDepth, worldX, worldZ));
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

    constexpr float waistRatio = 0.55f;
    const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);
    const float baseRequiredDepth = 1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

    // SMOOTH BASE TAPER — stem makin tipis mendekati base floor
    constexpr float taperZone = 0.30f;
    float taperRaise = 0.0f;
    if (tNorm < taperZone)
    {
        const float ft = 1.0f - (tNorm / taperZone);
        const float ftSmooth = ft * ft * (3.0f - 2.0f * ft);
        taperRaise = ftSmooth * (1.0f - Setting::islandEdgeCutoff) * 0.70f;
    }

    const float rockDetail =
        FBMNoise::generate(worldX * 0.05f + y * 0.15f, worldZ * 0.05f + y * 0.15f, 2, 0.5f, 0.5f,
                           Setting::seed + 777) * 0.07f;
    const float verticalRidge =
        RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ + y * 2), 2, 0.5f, 0.03f,
                             Setting::seed + 888) * 0.04f;

    const float finalThreshold = baseRequiredDepth + taperRaise + rockDetail - verticalRidge;
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
    const int flatPlateauH = std::clamp(100 + static_cast<int>(hVariation * 100.0f), 100, 280);

    const float smoothDepth = pDepth * pDepth * (3.0f - 2.0f * pDepth);
    int h = flatPlateauH;
    constexpr float blendStart = 0.35f;

    if (pDepth > blendStart)
    {
        // ── INNER ZONE: bukit + puncak ──────────────────────────────────
        const float tBlend = (pDepth - blendStart) / (1.0f - blendStart);
        const float tSmooth = tBlend * tBlend * (3.0f - 2.0f * tBlend);

        // Lifted peaks: angkat valley antar dua bukit supaya tidak menjorok ke bawah
        const float rawPeaks = std::max(0.0f, terrain.peaks);
        const float liftedPeaks = 1.0f - (1.0f - rawPeaks) * (1.0f - rawPeaks * Setting::saddleFillStrength);

        const float peakBlend = std::max(0.0f, (tBlend - Setting::mountainCoreThreshold) /
                                               (1.0f - Setting::mountainCoreThreshold));
        h += static_cast<int>(tSmooth * 60.0f) +
             static_cast<int>(peakBlend * std::sqrt(liftedPeaks) * Setting::peakHeight);
    }
    else
    {
        // ── EDGE ZONE: tepi, thin dan bervariasi ─────────────────────────
        h += static_cast<int>(terrain.erosion * 0.5f);

        const float edgeFactor = 1.0f - (smoothDepth / 0.35f);
        const float edgeSmooth = edgeFactor * edgeFactor;

        const float undulation = FBMNoise::generate(
            worldX * Setting::edgeUndulationScale, worldZ * Setting::edgeUndulationScale,
            3, 0.55f, 1.0f, Setting::seed + 1500);
        h += static_cast<int>(undulation * edgeSmooth * Setting::edgeUndulationAmp);

        const float cliffRidge = RidgeNoise::generate(
            worldX * Setting::edgeCliffScale, worldZ * Setting::edgeCliffScale,
            2, 0.5f, 1.0f, Setting::seed + 1600);
        const float cliffDrop = std::max(0.0f, 0.6f - cliffRidge);
        h -= static_cast<int>(cliffDrop * edgeSmooth * Setting::edgeCliffDrop);

        const float bump = FBMNoise::generate(
            worldX * Setting::edgeBumpScale, worldZ * Setting::edgeBumpScale,
            2, 0.5f, 1.0f, Setting::seed + 1700);
        h += static_cast<int>(bump * edgeSmooth * Setting::edgeBumpAmp);
    }

    // MOUNTAIN MASSIF — gunung besar, hanya tumbuh di daratan cukup dalam
    if (smoothDepth > 0.25f && terrain.mountainMassif > Setting::massifThreshold)
    {
        const float massifRaw = (terrain.mountainMassif - Setting::massifThreshold) /
                                (1.0f - Setting::massifThreshold);
        const float massifCurve = std::pow(massifRaw, Setting::massifSharpness);
        const float depthGate = std::clamp((smoothDepth - 0.25f) / 0.75f, 0.0f, 1.0f);
        h += static_cast<int>(massifCurve * depthGate * Setting::massifMaxHeight);
    }

    h = applyErosion(h, terrain);
    h = std::clamp(h, flatPlateauH - 4, Chunk::HEIGHT - 2);
    chunk.heightMap[x][z] = h;

    // Gunakan h final sebagai topHeight supaya bodyBottom selalu tracking surface aktual,
    // bukan flatPlateauH yang tidak memperhitungkan undulation/bump/cliff di edge zone.
    // pDepth raw (bukan smoothDepth) — konsisten dengan sampleBodyBottomAt dan isSolidAt.
    const int bodyBottom = std::max(baseFloorY + 12, estimateBodyBottom(h, pDepth, worldX, worldZ));

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

                constexpr float waistRatio = 0.55f;  // lebih besar = pinggang hourglass lebih lebar
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
