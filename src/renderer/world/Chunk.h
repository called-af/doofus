#pragma once

#include <vector>
#include <memory>
#include "block/BlockType.h"
#include "../Mesh.h"

class Chunk
{
public:
    static constexpr int SIZE = 16;
    static constexpr int HEIGHT = 64;

    int chunkX;
    int chunkZ;

    BlockType blocks[SIZE][HEIGHT][SIZE];

    std::vector<float> vertices;
    std::unique_ptr<Mesh> mesh;

    Chunk(int x, int z);

    void generate();
    void buildMesh();
    void draw();

    bool isAir(int x, int y, int z);

    void addFace(
        const float* face,
        int count,
        float worldX,
        float worldY,
        float worldZ
    );
};