#include "LODMesher.h"
#include "../TerrainGenerator.h"
#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "../terrain/TerrainSampler.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <cmath>

namespace {

// Push 1 vertex to the vertex buffer: [x, y, z, u, v, textureLayer, light]
inline void pushVertex(std::vector<float> &vertexBuffer, float x,
                       float y, float z, float u, float v,
                       float textureLayer, float light)
{
    vertexBuffer.push_back(x);
    vertexBuffer.push_back(y);
    vertexBuffer.push_back(z);
    vertexBuffer.push_back(u);
    vertexBuffer.push_back(v);
    vertexBuffer.push_back(textureLayer);
    vertexBuffer.push_back(light);
}

// Select texture layer based on block type, axis (0=X, 1=Y, 2=Z), and face orientation
inline int getTextureLayerForFace(BlockType blockType, int faceAxis, bool isBackFace)
{
    if (blockType == BlockType::Grass)
    {
        if (faceAxis == 1)
            return isBackFace ? 2 : 0; // Y-axis: top=0 (grass), bottom=2 (dirt)
        return 1;                    // side = layer 1
    }
    if (blockType == BlockType::Dirt)
        return 2;
    if (blockType == BlockType::Stone)
        return 3;
    if (blockType == BlockType::Sand)
        return 4;
    if (blockType == BlockType::Basalt)
        return 5;
    if (blockType == BlockType::Lava)
        return 6;
    if (blockType == BlockType::Obsidian)
        return 7;
    if (blockType == BlockType::Ash)
        return 8;
    if (blockType == BlockType::Cinder)
        return 9;
    if (blockType == BlockType::Heavenstone)
        return 10;
    if (blockType == BlockType::Crystal)
        return 11;
    return 0;
}

// Emit horizontal top quad facing +Y
void emitTopQuad(std::vector<float> &vertexBuffer, float minX, float maxX,
                 float minZ, float maxZ, float y, BlockType blockType)
{
    const float layer = (float)getTextureLayerForFace(blockType, 1, false);
    constexpr float light = 1.0f;
    const float u0 = minX, v0 = minZ;
    const float u1 = maxX, v1 = maxZ;

    // CCW: (p0, p1, p2) and (p0, p2, p3)
    pushVertex(vertexBuffer, minX, y, maxZ, u0, v1, layer, light);
    pushVertex(vertexBuffer, maxX, y, maxZ, u1, v1, layer, light);
    pushVertex(vertexBuffer, maxX, y, minZ, u1, v0, layer, light);

    pushVertex(vertexBuffer, minX, y, maxZ, u0, v1, layer, light);
    pushVertex(vertexBuffer, maxX, y, minZ, u1, v0, layer, light);
    pushVertex(vertexBuffer, minX, y, minZ, u0, v0, layer, light);
}

// Emit horizontal bottom quad facing -Y
void emitBottomQuad(std::vector<float> &vertexBuffer, float minX, float maxX,
                    float minZ, float maxZ, float y, BlockType blockType)
{
    const float layer = (float)getTextureLayerForFace(blockType, 1, true);
    constexpr float light = 0.55f;
    const float u0 = minX, v0 = minZ;
    const float u1 = maxX, v1 = maxZ;

    // CCW looking from below (-Y): (p0, p3, p2) and (p0, p2, p1)
    pushVertex(vertexBuffer, minX, y, maxZ, u0, v1, layer, light);
    pushVertex(vertexBuffer, minX, y, minZ, u0, v0, layer, light);
    pushVertex(vertexBuffer, maxX, y, minZ, u1, v0, layer, light);

    pushVertex(vertexBuffer, minX, y, maxZ, u0, v1, layer, light);
    pushVertex(vertexBuffer, maxX, y, minZ, u1, v0, layer, light);
    pushVertex(vertexBuffer, maxX, y, maxZ, u1, v1, layer, light);
}

// Emit vertical quad along X plane (facing +X if !isBackFace, -X if isBackFace)
void emitSideQuadX(std::vector<float> &vertexBuffer, float x, float minY, float maxY,
                   float minZ, float maxZ, bool isBackFace, BlockType blockType)
{
    if (minY >= maxY)
        return;

    const float layer = (float)getTextureLayerForFace(blockType, 0, isBackFace);
    constexpr float light = 0.80f;
    const float u0 = minZ, v0 = minY;
    const float u1 = maxZ, v1 = maxY;

    if (!isBackFace)
    {
        // +X normal
        pushVertex(vertexBuffer, x, minY, minZ, u0, v0, layer, light);
        pushVertex(vertexBuffer, x, maxY, minZ, u0, v1, layer, light);
        pushVertex(vertexBuffer, x, maxY, maxZ, u1, v1, layer, light);

        pushVertex(vertexBuffer, x, minY, minZ, u0, v0, layer, light);
        pushVertex(vertexBuffer, x, maxY, maxZ, u1, v1, layer, light);
        pushVertex(vertexBuffer, x, minY, maxZ, u1, v0, layer, light);
    }
    else
    {
        // -X normal
        pushVertex(vertexBuffer, x, minY, maxZ, u0, v0, layer, light);
        pushVertex(vertexBuffer, x, maxY, maxZ, u0, v1, layer, light);
        pushVertex(vertexBuffer, x, maxY, minZ, u1, v1, layer, light);

        pushVertex(vertexBuffer, x, minY, maxZ, u0, v0, layer, light);
        pushVertex(vertexBuffer, x, maxY, minZ, u1, v1, layer, light);
        pushVertex(vertexBuffer, x, minY, minZ, u1, v0, layer, light);
    }
}

// Emit vertical quad along Z plane (facing +Z if !isBackFace, -Z if isBackFace)
void emitSideQuadZ(std::vector<float> &vertexBuffer, float z, float minX, float maxX,
                   float minY, float maxY, bool isBackFace, BlockType blockType)
{
    if (minY >= maxY)
        return;

    const float layer = (float)getTextureLayerForFace(blockType, 2, isBackFace);
    constexpr float light = 0.70f;
    const float u0 = minX, v0 = minY;
    const float u1 = maxX, v1 = maxY;

    if (!isBackFace)
    {
        // +Z normal
        pushVertex(vertexBuffer, maxX, minY, z, u0, v0, layer, light);
        pushVertex(vertexBuffer, maxX, maxY, z, u0, v1, layer, light);
        pushVertex(vertexBuffer, minX, maxY, z, u1, v1, layer, light);

        pushVertex(vertexBuffer, maxX, minY, z, u0, v0, layer, light);
        pushVertex(vertexBuffer, minX, maxY, z, u1, v1, layer, light);
        pushVertex(vertexBuffer, minX, minY, z, u1, v0, layer, light);
    }
    else
    {
        // -Z normal
        pushVertex(vertexBuffer, minX, minY, z, u0, v0, layer, light);
        pushVertex(vertexBuffer, minX, maxY, z, u0, v1, layer, light);
        pushVertex(vertexBuffer, maxX, maxY, z, u1, v1, layer, light);

        pushVertex(vertexBuffer, minX, minY, z, u0, v0, layer, light);
        pushVertex(vertexBuffer, maxX, maxY, z, u1, v1, layer, light);
        pushVertex(vertexBuffer, maxX, minY, z, u1, v0, layer, light);
    }
}

// Find candidate Heaven seeds overlapping a given world AABB
void findCandidateHeavenSeeds(float minX, float maxX, float minZ, float maxZ,
                              std::vector<FeatureSeed> &outSeeds)
{
    outSeeds.clear();
    constexpr float g = Setting::heavenClusterSpacing;
    constexpr float maxClusterRadius = 220.0f; // max giant island radius + satellite offsets + warp

    int minCellX = static_cast<int>(std::floor((minX - maxClusterRadius) / g));
    int maxCellX = static_cast<int>(std::floor((maxX + maxClusterRadius) / g));
    int minCellZ = static_cast<int>(std::floor((minZ - maxClusterRadius) / g));
    int maxCellZ = static_cast<int>(std::floor((maxZ + maxClusterRadius) / g));

    for (int cx = minCellX; cx <= maxCellX; ++cx)
    {
        for (int cz = minCellZ; cz <= maxCellZ; ++cz)
        {
            FeatureSeed s = TerrainGenerator::generateHeavenSeed(cx, cz);
            if (s.exists)
            {
                outSeeds.push_back(s);
            }
        }
    }
}

} // anonymous namespace

void LODMesher::build(const LODMeshRequest &req, std::vector<float> &outVertices)
{
    outVertices.clear();
    if (req.level <= 2)
    {
        buildVoxelLOD(req, outVertices);
    }
    else
    {
        buildAnalyticalLOD(req, outVertices);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phase 2: LOD 1 & 2 (Voxel Blocky Mesher) — Fully Optimized
// ─────────────────────────────────────────────────────────────────────────────

void LODMesher::buildVoxelLOD(const LODMeshRequest &req, std::vector<float> &outVertices)
{
    const int level = std::clamp(req.level, 1, 2);
    const int covChunks = 1 << (level - 1);       // 1 chunk (LOD1) or 2 chunks (LOD2)
    const int stride = level;                     // 1 block (LOD1) or 2 blocks (LOD2)
    constexpr int GRID_SIZE = 16;                 // 16x16 sample cells per tile
    const int tileWidthBlocks = covChunks * Chunk::SIZE;

    const int baseWorldX = req.tileX * tileWidthBlocks;
    const int baseWorldZ = req.tileZ * tileWidthBlocks;

    // Pre-query Heaven seeds overlapping this tile
    std::vector<FeatureSeed> tileHeavenSeeds;
    findCandidateHeavenSeeds(static_cast<float>(baseWorldX),
                             static_cast<float>(baseWorldX + tileWidthBlocks),
                             static_cast<float>(baseWorldZ),
                             static_cast<float>(baseWorldZ + tileWidthBlocks),
                             tileHeavenSeeds);

    // Fast column scanner with zero redundant noise sampling
    auto scanColumn = [&](int wx, int wz) -> LODColumn
    {
        LODColumn col;

        const int floorH = TerrainGenerator::sampleHellFloorAt(wx, wz);
        const int contH  = TerrainGenerator::sampleContinentHeightAt(wx, wz);
        const int contBot = (contH > 0) ? TerrainGenerator::sampleContinentBodyBottomAt(wx, wz) : 0;

        // Tier 1 Hell Floor span: 0..floorH
        BlockType hellType = TerrainGenerator::sampleBlockAt(wx, wz, floorH);
        col.spans.push_back({floorH, 0, hellType, false, true});

        // Tier 2 Continent + Hourglass stem
        if (contH > 0 && contH >= contBot)
        {
            const TerrainSample terrain = TerrainSampler::sample(wx, wz);
            const float raw = (terrain.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
            const float pClamped = std::clamp(raw, 0.0f, 1.0f);
            const float pDepth = pClamped * pClamped * (3.0f - 2.0f * pClamped);

            const int stemTop = contBot;
            const int stemBottom = floorH + 1;
            const int stemHeight = stemTop - stemBottom;

            int stemSpanBot = -1;
            int stemSpanTop = -1;

            if (stemHeight > 0)
            {
                constexpr float waistRatio = 0.32f;
                for (int y = stemBottom; y < stemTop; y += stride)
                {
                    const float tNorm = static_cast<float>(y - stemBottom) / static_cast<float>(stemHeight);
                    const float midDist = std::abs(tNorm - 0.5f) * 2.0f;
                    const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);
                    const float baseRequiredDepth = 1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

                    const float rockDetail = FBMNoise::generate(wx * 0.05f + y * 0.15f, wz * 0.05f + y * 0.15f, 2, 0.5f, 0.5f, Setting::seed + 777) * 0.07f;
                    const float verticalRidge = RidgeNoise::generate(static_cast<float>(wx), static_cast<float>(wz + y * 2), 2, 0.5f, 0.03f, Setting::seed + 888) * 0.04f;

                    if (pDepth >= baseRequiredDepth + rockDetail - verticalRidge)
                    {
                        if (stemSpanBot == -1)
                            stemSpanBot = y;
                        stemSpanTop = y;
                    }
                    else if (stemSpanBot != -1)
                    {
                        // Stem detached segment
                        col.spans.push_back({stemSpanTop, stemSpanBot, BlockType::Stone, true, true});
                        stemSpanBot = -1;
                        stemSpanTop = -1;
                    }
                }
            }

            int finalContinentBot = (stemSpanTop == stemTop - 1 || stemSpanTop >= stemTop - stride) && stemSpanBot != -1 ? stemSpanBot : contBot;
            BlockType contType = TerrainGenerator::sampleBlockAt(wx, wz, contH);
            col.spans.push_back({contH, finalContinentBot, contType, true, true});
        }

        // Tier 3 Heaven Floating Islands
        for (const auto &s : tileHeavenSeeds)
        {
            const int totalIslands = 1 + s.subIsletCount;
            for (int i = 0; i < totalIslands; ++i)
            {
                IslandSlice slice = TerrainGenerator::evaluateIslandSlice(static_cast<float>(wx), static_cast<float>(wz), s, i);
                if (slice.valid)
                {
                    const int minH = std::clamp(static_cast<int>(slice.botY), 280, Chunk::HEIGHT - 2);
                    const int maxH = std::clamp(static_cast<int>(slice.topY), minH, Chunk::HEIGHT - 1);
                    BlockType hType = slice.isAnchorPeak ? BlockType::Crystal : BlockType::Grass;
                    col.spans.push_back({maxH, minH, hType, true, true});
                }
            }
        }

        return col;
    };

    // Scan all 16x16 columns in this tile
    LODColumn grid[GRID_SIZE][GRID_SIZE];
    for (int lx = 0; lx < GRID_SIZE; ++lx)
    {
        for (int lz = 0; lz < GRID_SIZE; ++lz)
        {
            grid[lx][lz] = scanColumn(baseWorldX + lx * stride, baseWorldZ + lz * stride);
        }
    }

    auto getNeighbor = [&](int gx, int gz) -> LODColumn
    {
        if (gx >= 0 && gx < GRID_SIZE && gz >= 0 && gz < GRID_SIZE)
            return grid[gx][gz];
        return scanColumn(baseWorldX + gx * stride, baseWorldZ + gz * stride);
    };

    // Emit faces for all columns
    for (int lx = 0; lx < GRID_SIZE; ++lx)
    {
        const float minX = static_cast<float>(baseWorldX + lx * stride);
        const float maxX = minX + static_cast<float>(stride);

        for (int lz = 0; lz < GRID_SIZE; ++lz)
        {
            const float minZ = static_cast<float>(baseWorldZ + lz * stride);
            const float maxZ = minZ + static_cast<float>(stride);

            const LODColumn &col = grid[lx][lz];
            if (col.spans.empty())
                continue;

            const LODColumn nNX = getNeighbor(lx - 1, lz);
            const LODColumn nPX = getNeighbor(lx + 1, lz);
            const LODColumn nNZ = getNeighbor(lx, lz - 1);
            const LODColumn nPZ = getNeighbor(lx, lz + 1);

            for (const auto &span : col.spans)
            {
                const float topY = static_cast<float>(span.topY + stride);
                const float botY = static_cast<float>(span.bottomY);

                // Top quad
                emitTopQuad(outVertices, minX, maxX, minZ, maxZ, topY, span.surfaceType);

                // Bottom quad (for floating continent/heaven bodies)
                if (span.needsBottomCap)
                {
                    BlockType botType = (span.bottomY >= 270) ? BlockType::Heavenstone : BlockType::Stone;
                    emitBottomQuad(outVertices, minX, maxX, minZ, maxZ, botY, botType);
                }

                // Helper to emit exposed vertical side faces
                auto emitSideAgainstNeighbor = [&](const LODColumn &nCol, float borderCoord, bool isXAxis, bool isBackFace)
                {
                    int exposedBot = -1;
                    int exposedTop = -1;

                    auto closeExposed = [&](int eBot, int eTop)
                    {
                        if (eBot < 0 || eTop < eBot)
                            return;
                        float y0 = static_cast<float>(eBot);
                        float y1 = static_cast<float>(eTop + stride);
                        BlockType sideType = (eTop >= 270) ? BlockType::Heavenstone : ((eTop <= Setting::hellCanyonRimY) ? BlockType::Basalt : BlockType::Stone);
                        if (isXAxis)
                            emitSideQuadX(outVertices, borderCoord, y0, y1, minZ, maxZ, isBackFace, sideType);
                        else
                            emitSideQuadZ(outVertices, borderCoord, minX, maxX, y0, y1, isBackFace, sideType);
                    };

                    for (int y = span.bottomY; y <= span.topY; y += stride)
                    {
                        bool neighborCovered = false;
                        for (const auto &nSpan : nCol.spans)
                        {
                            if (y >= nSpan.bottomY && y <= nSpan.topY)
                            {
                                neighborCovered = true;
                                break;
                            }
                        }

                        if (!neighborCovered)
                        {
                            if (exposedBot == -1)
                                exposedBot = y;
                            exposedTop = y;
                        }
                        else if (exposedBot != -1)
                        {
                            closeExposed(exposedBot, exposedTop);
                            exposedBot = -1;
                            exposedTop = -1;
                        }
                    }

                    if (exposedBot != -1)
                    {
                        closeExposed(exposedBot, exposedTop);
                    }
                };

                // -X face
                emitSideAgainstNeighbor(nNX, minX, true, true);
                // +X face
                emitSideAgainstNeighbor(nPX, maxX, true, false);
                // -Z face
                emitSideAgainstNeighbor(nNZ, minZ, false, true);
                // +Z face
                emitSideAgainstNeighbor(nPZ, maxZ, false, false);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Phase 3: LOD 3, 4, 5 (Analytical Multi-Span Mesher) — Fully Optimized
// ─────────────────────────────────────────────────────────────────────────────

void LODMesher::buildAnalyticalLOD(const LODMeshRequest &req, std::vector<float> &outVertices)
{
    const int level = std::clamp(req.level, 3, 5);
    const int covChunks = 1 << (level - 1);       // LOD3: 4 chunks, LOD4: 8 chunks, LOD5: 16 chunks
    const int stride = 1 << (level - 1);          // LOD3: 4 blocks, LOD4: 8 blocks, LOD5: 16 blocks
    constexpr int GRID_SIZE = 16;                 // 16x16 sample cells per tile
    constexpr int kMaxWallDrop = 64;              // Max skirt drop height clamping
    const int tileWidthBlocks = covChunks * Chunk::SIZE;

    const int baseWorldX = req.tileX * tileWidthBlocks;
    const int baseWorldZ = req.tileZ * tileWidthBlocks;

    // Pre-query Heaven seeds overlapping this tile
    std::vector<FeatureSeed> tileHeavenSeeds;
    findCandidateHeavenSeeds(static_cast<float>(baseWorldX),
                             static_cast<float>(baseWorldX + tileWidthBlocks),
                             static_cast<float>(baseWorldZ),
                             static_cast<float>(baseWorldZ + tileWidthBlocks),
                             tileHeavenSeeds);

    // Fast analytical column sampler
    auto sampleAnalyticalColumn = [&](int wx, int wz) -> LODColumn
    {
        LODColumn col;

        // 1. Tier 1: Hell Floor
        const int hellTop = TerrainGenerator::sampleHellFloorAt(wx, wz);
        const BlockType hellType = TerrainGenerator::sampleBlockAt(wx, wz, hellTop);
        col.spans.push_back({hellTop, 0, hellType, false, true});

        // 2. Tier 2: Continent (Plateau Body Slab, skipping hourglass stem)
        const int contTop = TerrainGenerator::sampleContinentHeightAt(wx, wz);
        if (contTop > 0)
        {
            const int contBot = TerrainGenerator::sampleContinentBodyBottomAt(wx, wz);
            if (contTop >= contBot)
            {
                const BlockType contType = TerrainGenerator::sampleBlockAt(wx, wz, contTop);
                col.spans.push_back({contTop, contBot, contType, true, true});
            }
        }

        // 3. Tier 3: Heaven Floating Islands
        for (const auto &s : tileHeavenSeeds)
        {
            const int maxIslets = (level >= 4) ? 1 : (1 + s.subIsletCount);
            for (int i = 0; i < maxIslets; ++i)
            {
                IslandSlice slice = TerrainGenerator::evaluateIslandSlice(static_cast<float>(wx), static_cast<float>(wz), s, i);
                if (slice.valid)
                {
                    const int hTop = std::clamp(static_cast<int>(slice.topY), 280, Chunk::HEIGHT - 1);
                    const int hBot = std::clamp(static_cast<int>(slice.botY), 280, hTop);

                    BlockType hType = BlockType::Grass;
                    if (level == 3 && slice.isAnchorPeak)
                    {
                        hType = BlockType::Crystal;
                    }
                    col.spans.push_back({hTop, hBot, hType, true, true});
                }
            }
        }

        return col;
    };

    // Pre-sample grid of 16x16 columns
    LODColumn grid[GRID_SIZE][GRID_SIZE];
    for (int lx = 0; lx < GRID_SIZE; ++lx)
    {
        for (int lz = 0; lz < GRID_SIZE; ++lz)
        {
            grid[lx][lz] = sampleAnalyticalColumn(baseWorldX + lx * stride, baseWorldZ + lz * stride);
        }
    }

    auto getNeighbor = [&](int gx, int gz) -> LODColumn
    {
        if (gx >= 0 && gx < GRID_SIZE && gz >= 0 && gz < GRID_SIZE)
            return grid[gx][gz];
        return sampleAnalyticalColumn(baseWorldX + gx * stride, baseWorldZ + gz * stride);
    };

    // Emit top caps, bottom caps, and connecting skirts with clamping
    for (int lx = 0; lx < GRID_SIZE; ++lx)
    {
        const float minX = static_cast<float>(baseWorldX + lx * stride);
        const float maxX = minX + static_cast<float>(stride);

        for (int lz = 0; lz < GRID_SIZE; ++lz)
        {
            const float minZ = static_cast<float>(baseWorldZ + lz * stride);
            const float maxZ = minZ + static_cast<float>(stride);

            const LODColumn &col = grid[lx][lz];
            if (col.spans.empty())
                continue;

            const LODColumn nNX = getNeighbor(lx - 1, lz);
            const LODColumn nPX = getNeighbor(lx + 1, lz);
            const LODColumn nNZ = getNeighbor(lx, lz - 1);
            const LODColumn nPZ = getNeighbor(lx, lz + 1);

            for (const auto &span : col.spans)
            {
                const float topY = static_cast<float>(span.topY);
                const float botY = static_cast<float>(span.bottomY);

                // Top Cap
                emitTopQuad(outVertices, minX, maxX, minZ, maxZ, topY, span.surfaceType);

                // Bottom Cap (for floating continent and heaven tiers)
                if (span.needsBottomCap)
                {
                    BlockType botType = (span.bottomY >= 270) ? BlockType::Heavenstone : BlockType::Stone;
                    emitBottomQuad(outVertices, minX, maxX, minZ, maxZ, botY, botType);
                }

                // Helper to emit watertight skirts against neighbor
                auto emitSkirtsAgainstNeighbor = [&](const LODColumn &nCol, float borderCoord, bool isXAxis, bool isBackFace)
                {
                    const LODSpan *matchingSpan = nullptr;
                    for (const auto &nSpan : nCol.spans)
                    {
                        if (span.bottomY == 0 && nSpan.bottomY == 0)
                        {
                            matchingSpan = &nSpan;
                            break;
                        }
                        else if (span.bottomY > 0 && span.topY < 270 && nSpan.bottomY > 0 && nSpan.topY < 270)
                        {
                            matchingSpan = &nSpan;
                            break;
                        }
                        else if (span.bottomY >= 270 && nSpan.bottomY >= 270)
                        {
                            matchingSpan = &nSpan;
                            break;
                        }
                    }

                    if (matchingSpan)
                    {
                        if (span.topY > matchingSpan->topY)
                        {
                            const int dropY = std::max(matchingSpan->topY, span.topY - kMaxWallDrop);
                            BlockType sideType = (span.topY >= 270) ? BlockType::Heavenstone : ((span.topY <= Setting::hellCanyonRimY) ? BlockType::Basalt : BlockType::Stone);
                            if (isXAxis)
                                emitSideQuadX(outVertices, borderCoord, static_cast<float>(dropY), topY, minZ, maxZ, isBackFace, sideType);
                            else
                                emitSideQuadZ(outVertices, borderCoord, minX, maxX, static_cast<float>(dropY), topY, isBackFace, sideType);
                        }

                        if (span.needsBottomCap && span.bottomY < matchingSpan->bottomY)
                        {
                            const int riseY = std::min(matchingSpan->bottomY, span.bottomY + kMaxWallDrop);
                            BlockType sideType = (span.bottomY >= 270) ? BlockType::Heavenstone : BlockType::Stone;
                            if (isXAxis)
                                emitSideQuadX(outVertices, borderCoord, botY, static_cast<float>(riseY), minZ, maxZ, isBackFace, sideType);
                            else
                                emitSideQuadZ(outVertices, borderCoord, minX, maxX, botY, static_cast<float>(riseY), isBackFace, sideType);
                        }
                    }
                    else
                    {
                        const int dropY = std::max(span.bottomY, span.topY - kMaxWallDrop);
                        BlockType sideType = (span.bottomY >= 270) ? BlockType::Heavenstone : BlockType::Stone;
                        if (isXAxis)
                            emitSideQuadX(outVertices, borderCoord, static_cast<float>(dropY), topY, minZ, maxZ, isBackFace, sideType);
                        else
                            emitSideQuadZ(outVertices, borderCoord, minX, maxX, static_cast<float>(dropY), topY, isBackFace, sideType);
                    }
                };

                emitSkirtsAgainstNeighbor(nNX, minX, true, true);
                emitSkirtsAgainstNeighbor(nPX, maxX, true, false);
                emitSkirtsAgainstNeighbor(nNZ, minZ, false, true);
                emitSkirtsAgainstNeighbor(nPZ, maxZ, false, false);
            }
        }
    }
}
