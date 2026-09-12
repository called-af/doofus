#pragma once

#include "../Chunk.h"
#include "../biome/Biome.h"
#include "../block/BlockType.h"

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

// Evaluated 2D column slice of a Heaven island (top plateau, bottom underbelly, validity)
struct IslandSlice
{
    float topY = 0.0f;
    float botY = 0.0f;
    float effectiveD = 1.0f;
    bool valid = false;
    bool isAnchorPeak = false;
};

class HeavenTerrain
{
public:
    static FeatureSeed generateSeed(int cellX, int cellZ);
    static FeatureSeed findNearestSeed(float worldX, float worldZ, float &outDist);
    static IslandSlice evaluateSlice(float worldX, float worldZ, const FeatureSeed &s, int islandIndex);
    static float computeDistance(float worldX, float worldZ, const FeatureSeed &s);

    static int sampleHeightAt(float worldX, float worldZ);
    static BlockType sampleBlock(float worldX, float worldZ, int surfaceHeight);
    static Biome* getBiome(float worldX, float worldZ, int y);

    static void generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                   const FeatureSeed &seed);
    static void generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                      const FeatureSeed &seed, int &layerDepth);
};
