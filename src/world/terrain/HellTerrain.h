#pragma once

#include "../Chunk.h"
#include "../biome/Biome.h"
#include "../block/BlockType.h"

class HellTerrain
{
public:
    static float getSpineX(float worldZ);
    static float getCanyonDepthRatio(float worldX, float worldZ);
    static float getHellProvince(float worldX, float worldZ);
    static float getCrackIntensity(float worldX, float worldZ, float canyonRatio);
    
    static int sampleFloorHeight(int worldX, int worldZ);
    static BlockType sampleBlock(int worldX, int worldZ, int surfaceHeight);
    static BlockType sampleSurfaceBlock(int worldX, int worldZ, float canyonRatio, float crackIntensity);
    static Biome* getBiome(int worldX, int worldZ);

    static void generateColumnBase(Chunk &chunk, int x, int z, int worldX, int worldZ,
                                   int floorH, float canyonRatio, float crackIntensity,
                                   int &outBaseFloorY);
    static void generateColumnSurface(Chunk &chunk, int x, int z, int y, int worldX, int worldZ,
                                      float canyonRatio, float crackIntensity);
};
