#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "climate/ClimateSampler.h"
#include "noise/FBMNoise.h"
#include "terrain/TerrainSampler.h"

#include "../core/Setting.h"

#include <cmath>
#include <iostream>

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
}

void TerrainGenerator::generateBaseTerrain(Chunk &chunk) {
  for (int x = 0; x < Chunk::SIZE; x++) {
    for (int z = 0; z < Chunk::SIZE; z++) {
      int worldX = chunk.chunkX * Chunk::SIZE + x;

      int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);

      int height = Setting::baseTerrainHeight;

      /*
          CONTINENTALNESS
      */

      height += terrain.continentalness * Setting::continentalHeight;

      /*
          PEAKS
      */

      float peakFactor = std::pow(terrain.peaks, 2.0f);
      height += (int)(peakFactor * Setting::peakHeight);

      /*
          EROSION
      */

      height -= (int)(terrain.erosion * Setting::erosionStrength);

      /*
          RIVERS
      */

      if (terrain.river < Setting::riverThreshold) {
        height -= Setting::riverDepth;
      }

      /*
          CLAMP
      */

      if (height < 1) {
        height = 1;
      }

      if (height >= Chunk::HEIGHT) {
        height = Chunk::HEIGHT - 1;
      }

      if (height >= Chunk::HEIGHT - 1) {
        // Log ini akan muncul di konsol jika gunung Anda terkena batas atas
        std::cout << "Warning: Terrain clamped at X:" << worldX
                  << " Z:" << worldZ << std::endl;
      }

      chunk.heightMap[x][z] = height;

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

      TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);

      ClimateSample climate = ClimateSampler::sample(worldX, worldZ);

      Biome *biome = BiomeManager::getBiome(terrain, climate);

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