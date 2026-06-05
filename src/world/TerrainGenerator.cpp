#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "noise/FBMNoise.h"

#include "../core/Setting.h"

#include <cmath>

void TerrainGenerator::generate(Chunk &chunk) {

  /*
      CLEAR WORLD
  */

  for (int x = 0; x < Chunk::SIZE; x++) {

    for (int y = 0; y < Chunk::HEIGHT; y++) {

      for (int z = 0; z < Chunk::SIZE; z++) {

        chunk.blocks[x][y][z] = BlockType::Air;
      }
    }
  }

  /*
      GENERATION STEPS
  */

  generateBaseTerrain(chunk);

  // generateMountains(chunk);

  generateSurface(chunk);

  generateCaves(chunk);

  chunk.dirty = true;
}

void TerrainGenerator::generateBaseTerrain(Chunk &chunk) {

  for (int x = 0; x < Chunk::SIZE; x++) {

    for (int z = 0; z < Chunk::SIZE; z++) {

      int worldX = chunk.chunkX * Chunk::SIZE + x;

      int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      /*
          BIOME
      */

      Biome *biome = BiomeManager::getBiome(worldX, worldZ);

      /*
          MAIN TERRAIN
      */

      float terrain =
          FBMNoise::generate(worldX, worldZ, 5, 0.5f, 0.008f, Setting::seed);

      int height =
          biome->getBaseHeight(worldX, worldZ) + (int)(terrain * 12.0f);

      /*
          BIG MOUNTAINS
      */

      float mountain =
          FBMNoise::generate(worldX, worldZ, 5, 0.5f, 0.008f, Setting::seed);

      mountain = std::pow(std::abs(mountain), 3.5f);

      height += (int)(mountain * 50.0f);

      /*
          CLAMP
      */

      if (height < 1)
        height = 1;

      if (height >= Chunk::HEIGHT)
        height = Chunk::HEIGHT - 1;

      chunk.heightMap[x][z] = height;

      /*
          STONE BASE
      */

      for (int y = 0; y <= height; y++) {

        chunk.blocks[x][y][z] = BlockType::Stone;
      }
    }
  }
}

void TerrainGenerator::generateSurface(Chunk &chunk) {

  for (int x = 0; x < Chunk::SIZE; x++) {

    for (int z = 0; z < Chunk::SIZE; z++) {

      int worldX = chunk.chunkX * Chunk::SIZE + x;

      int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      Biome *biome = BiomeManager::getBiome(worldX, worldZ);

      int layerDepth = 0;

      for (int y = Chunk::HEIGHT - 1; y >= 0; y--) {

        /*
            ONLY STONE
        */

        if (chunk.blocks[x][y][z] != BlockType::Stone) {
          continue;
        }

        /*
            AIR ABOVE
        */

        bool airAbove = (y + 1 >= Chunk::HEIGHT) ||
                        (chunk.blocks[x][y + 1][z] == BlockType::Air);

        /*
            TOP SURFACE
        */

        if (airAbove) {

          chunk.blocks[x][y][z] = biome->getTopBlock();

          layerDepth = 4;
        }

        /*
            UNDERGROUND
        */

        else if (layerDepth > 0) {

          chunk.blocks[x][y][z] = biome->getMiddleBlock();

          layerDepth--;
        }
      }
    }
  }
}

void TerrainGenerator::generateCaves(Chunk &chunk) {

  for (int x = 0; x < Chunk::SIZE; x++) {

    for (int z = 0; z < Chunk::SIZE; z++) {

      int worldX = chunk.chunkX * Chunk::SIZE + x;

      int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      for (int y = 5; y < 90; y++) {

        float cave = FBMNoise::generate(worldX + y * 2, worldZ + y * 2, 3, 0.5f,
                                        0.045f, Setting::seed);

        /*
            SMALLER CAVES
        */

        if (cave > 0.72f) {

          chunk.blocks[x][y][z] = BlockType::Air;
        }
      }
    }
  }
}