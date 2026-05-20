#pragma once

#include "../block/BlockType.h"

class Chunk
{
public:
    static constexpr int SIZE = 16;
    static constexpr int HEIGHT = 64;

    BlockType blocks[SIZE][HEIGHT][SIZE];

    int chunkX;
    int chunkZ;

    Chunk(int x, int z);

    void generate();
};