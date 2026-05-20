#include "World.h"
#include "TerrainGenerator.h"
#include <cmath>

void World::generate()
{
    for (int x = -2; x <= 2; x++)
    for (int z = -2; z <= 2; z++)
    {
        auto chunk = std::make_unique<Chunk>(x, z);

        TerrainGenerator::generate(*chunk);

        chunk->buildMesh();

        chunks.push_back(std::move(chunk));
    }
}

void World::draw()
{
    for (auto& chunk : chunks)
        chunk->draw();
}

bool World::isSolid(int x, int y, int z)
{
    int chunkX = (int)std::floor((float)x / Chunk::SIZE);
    int chunkZ = (int)std::floor((float)z / Chunk::SIZE);

    for (auto& chunk : chunks)
    {
        if (chunk->chunkX == chunkX &&
            chunk->chunkZ == chunkZ)
        {
            int localX = x - chunkX * Chunk::SIZE;
            int localZ = z - chunkZ * Chunk::SIZE;

            if (localX < 0) localX += Chunk::SIZE;
            if (localZ < 0) localZ += Chunk::SIZE;

            if (localX < 0 || localX >= Chunk::SIZE ||
                localZ < 0 || localZ >= Chunk::SIZE ||
                y < 0 || y >= Chunk::HEIGHT)
                return false;

            return chunk->blocks[localX][y][localZ] != BlockType::Air;
        }
    }

    return false;
}