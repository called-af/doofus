#pragma once

#include "../Chunk.h"
#include "../biome/Biome.h"
#include "../terrain/TerrainSample.h"
#include "../block/BlockType.h"

class ContinentTerrain
{
public:
    static int computeBaseHeight(const TerrainSample &t);
    static int sampleHeightAt(int worldX, int worldZ, const TerrainSample &t);
    static int sampleBodyBottomAt(int worldX, int worldZ, const TerrainSample &t, int baseFloorY);
    static int estimateBodyBottom(int topHeight, float pDepth, int worldX = 0, int worldZ = 0);
    static int applyErosion(int h, const TerrainSample &t);
    static bool isSolidAt(int worldX, int worldZ, int y, const TerrainSample &t, int baseFloorY);

    static void generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                   const TerrainSample &t, float pDepth, int baseFloorY);
    static void generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                      Biome* biome, int &layerDepth);
};
