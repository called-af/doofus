#include "Chunk.h"
#include <cmath>

Chunk::Chunk(int x, int z)
{
    chunkX = x;
    chunkZ = z;
}

void Chunk::generate()
{
    for (int x = 0; x < SIZE; x++)
    for (int z = 0; z < SIZE; z++)
    {
        int worldX = chunkX * SIZE + x;
        int worldZ = chunkZ * SIZE + z;

        float noise = sin(worldX * 0.12f) * cos(worldZ * 0.12f);
        int height = 20 + (int)(noise * 8);

        for (int y = 0; y < HEIGHT; y++)
        {
            if (y > height)
                blocks[x][y][z] = BlockType::Air;
            else if (y == height)
                blocks[x][y][z] = BlockType::Grass;
            else
                blocks[x][y][z] = BlockType::Dirt;
        }
    }
}

bool Chunk::isAir(int x, int y, int z)
{
    if (x < 0 || x >= SIZE ||
        y < 0 || y >= HEIGHT ||
        z < 0 || z >= SIZE)
        return true;

    return blocks[x][y][z] == BlockType::Air;
}

void Chunk::addFace(
    const float* face,
    int count,
    float worldX,
    float worldY,
    float worldZ)
{
    for (int i = 0; i < count; i += 3)
    {
        vertices.push_back(face[i + 0] + worldX);
        vertices.push_back(face[i + 1] + worldY);
        vertices.push_back(face[i + 2] + worldZ);
    }
}

void Chunk::buildMesh()
{
    vertices.clear();

    const float topFace[] = {
        0,1,1, 1,1,1, 1,1,0,
        0,1,1, 1,1,0, 0,1,0
    };

    const float bottomFace[] = {
        0,0,0, 1,0,0, 1,0,1,
        0,0,0, 1,0,1, 0,0,1
    };

    const float frontFace[] = {
        0,0,1, 1,0,1, 1,1,1,
        0,0,1, 1,1,1, 0,1,1
    };

    const float backFace[] = {
        1,0,0, 0,0,0, 0,1,0,
        1,0,0, 0,1,0, 1,1,0
    };

    const float leftFace[] = {
        0,0,0, 0,0,1, 0,1,1,
        0,0,0, 0,1,1, 0,1,0
    };

    const float rightFace[] = {
        1,0,1, 1,0,0, 1,1,0,
        1,0,1, 1,1,0, 1,1,1
    };

    for (int x = 0; x < SIZE; x++)
    for (int y = 0; y < HEIGHT; y++)
    for (int z = 0; z < SIZE; z++)
    {
        if (blocks[x][y][z] == BlockType::Air)
            continue;

        float wx = chunkX * SIZE + x;
        float wy = y;
        float wz = chunkZ * SIZE + z;

        if (isAir(x, y + 1, z)) addFace(topFace, 18, wx, wy, wz);
        if (isAir(x, y - 1, z)) addFace(bottomFace, 18, wx, wy, wz);
        if (isAir(x, y, z + 1)) addFace(frontFace, 18, wx, wy, wz);
        if (isAir(x, y, z - 1)) addFace(backFace, 18, wx, wy, wz);
        if (isAir(x - 1, y, z)) addFace(leftFace, 18, wx, wy, wz);
        if (isAir(x + 1, y, z)) addFace(rightFace, 18, wx, wy, wz);
    }

    if (!vertices.empty())
    {
        mesh = std::make_unique<Mesh>(
            vertices.data(),
            vertices.size() * sizeof(float)
        );
    }
}

void Chunk::draw()
{
    if (mesh)
        mesh->draw();
}