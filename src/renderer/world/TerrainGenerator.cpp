#include "TerrainGenerator.h"
#include "Noise.h"

void TerrainGenerator::generate(Chunk& chunk)
{
    for (int x = 0; x < Chunk::SIZE; x++)
    for (int z = 0; z < Chunk::SIZE; z++)
    {
        int worldX = chunk.chunkX * Chunk::SIZE + x;
        int worldZ = chunk.chunkZ * Chunk::SIZE + z;

        float noise = Noise::get(worldX, worldZ);

        int height = 20 + (int)(noise * 15.0f);

        for (int y = 0; y < Chunk::HEIGHT; y++)
        {
            if (y > height)
                chunk.blocks[x][y][z] = BlockType::Air;
            else if (y == height)
                chunk.blocks[x][y][z] = BlockType::Grass;
            else if (y > height - 3)
                chunk.blocks[x][y][z] = BlockType::Dirt;
            else
                chunk.blocks[x][y][z] = BlockType::Stone;
        }
    }
}