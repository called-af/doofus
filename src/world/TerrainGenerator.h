#pragma once

#include "Chunk.h"
#include "terrain/TerrainSample.h"

#include <array>
#include <vector>

// Satellite sub-islet in a Heaven Archipelago Cluster
struct SubIslet
{
    float offsetX, offsetZ; // Relative position offset from cluster center
    float offsetY;          // Relative Y elevation offset
    float radius;           // Sub-islet radius
    float thickness;        // Sub-islet thickness
};

// Feature-Point Seed representation for Tier 3 (Heaven floating island clusters)
struct FeatureSeed
{
    bool exists = false;   // Whether an archipelago cluster spawns in this cell (rare)
    float x, z;            // Cluster center world coordinates
    float radius;          // Main alpha island radius (12 to 135 blocks)
    float thickness;       // Main alpha island thickness (10 to 75 blocks)
    float basePosY;        // Base slab Y elevation (high: 320 to 410)
    float angle;           // Rotation angle for asymmetric elongation
    float aspect;          // Aspect ratio (1.0 to 2.2 for stretched shapes)
    bool isAnchor;         // Landmark island with large crystal spire
    bool isGiant;          // Giant cluster flag
    bool hasPeak;          // Mountain peak on top
    int subIsletCount = 0; // Number of satellite islets in this cluster (2 to 4)
    SubIslet subIslets[4]; // Satellite islet parameters
};

// Per-column precomputed cache (calculated once per chunk per column)
struct ColumnCache
{
    TerrainSample terrain; // noise sample (continentalness, plateau, etc.)
    float pRaw;            // (plateau - threshold) / (1 - threshold), clamped [0,1]
    float pDepth;          // smoothstep(pRaw)
    int floorH;            // base height
    bool isIsland;         // plateau >= plateauThreshold

    // Tier 1 Hell Canyon data
    float canyonDepthRatio; // 1.0 at spine center, 0.0 outside canyon

    // Tier 3 Heaven data
    float heavenDistance;   // distance to nearest Heaven seed (0 = center, 1 = edge)
    FeatureSeed heavenSeed; // nearest Heaven seed
};

// Evaluated 2D column slice of a Heaven island (top plateau, bottom underbelly, validity)
struct IslandSlice
{
    float topY = 0.0f;
    float botY = 0.0f;
    float effectiveD = 1.0f;
    bool valid = false;
    bool isAnchorPeak = false;
};

class TerrainGenerator
{
public:
    static void generate(Chunk &chunk);

    //  Standalone feature seed & canyon query helpers ─
    static FeatureSeed generateHeavenSeed(int cellX, int cellZ);
    static FeatureSeed findNearestHeavenSeed(float worldX, float worldZ, float &outDist);
    static IslandSlice evaluateIslandSlice(float worldX, float worldZ, const FeatureSeed &s, int islandIndex);

    static float getHellSpineX(float worldZ);
    static float getHellCanyonDepthRatio(float worldX, float worldZ);

    //  Standalone height and block samplers (does not require loaded Chunk) 
    static int sampleHeightAt(int worldX, int worldZ);
    static bool isSolidAt(int worldX, int worldZ, int y);
    static BlockType sampleBlockAt(int worldX, int worldZ, int surfaceHeight);

    //  Standalone multi-tier samplers for LOD & physics
    static int sampleHellFloorAt(int worldX, int worldZ);
    static int sampleContinentHeightAt(int worldX, int worldZ);
    static int sampleContinentBodyBottomAt(int worldX, int worldZ);
    static int estimateBodyBottom(int flatPlateauH, float pDepth);

private:
    using ColumnGrid = std::array<std::array<ColumnCache, Chunk::SIZE>, Chunk::SIZE>;
    static ColumnGrid buildColumnCache(const Chunk &chunk);

    // Main passes
    static void generateBaseTerrain(Chunk &chunk, const ColumnGrid &cache);
    static void generateSurface(Chunk &chunk, const ColumnGrid &cache);
    static void generateCaves(Chunk &chunk, const ColumnGrid &cache);

    // Height pipeline
    static int computeBaseHeight(const TerrainSample &t);
    static int applyPlateauLift(int baseH, const TerrainSample &t);
    static int applyTerrace(int h, const TerrainSample &t);
    static int applyMountainTop(int h, const TerrainSample &t);
    static int applyErosion(int h, const TerrainSample &t);

    // Pillar support
    static bool shouldSpawnPillar(const TerrainSample &t);
    static void fillPillar(Chunk &chunk, int lx, int lz, int topH, int bottomH);
};