#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "climate/ClimateSampler.h"
#include "noise/FBMNoise.h"
#include "terrain/TerrainSampler.h"

#include "../core/Setting.h"

#include <algorithm>
#include <cmath>

//  COLUMN CACHE — noise is sampled ONCE per column, used across all passes

TerrainGenerator::ColumnGrid
TerrainGenerator::buildColumnCache(const Chunk &chunk) {
  ColumnGrid grid{};

  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      ColumnCache &c = grid[x][z];
      c.terrain = TerrainSampler::sample(worldX, worldZ);

      const float raw = (c.terrain.plateau - Setting::plateauThreshold) /
                        (1.0f - Setting::plateauThreshold);
      c.pRaw = std::clamp(raw, 0.0f, 1.0f);
      c.pDepth = c.pRaw * c.pRaw * (3.0f - 2.0f * c.pRaw); // smoothstep

      c.floorH = computeBaseHeight(c.terrain);
      c.isIsland = (c.terrain.plateau >= Setting::plateauThreshold);
    }
  }

  return grid;
}

//  ENTRY POINT

void TerrainGenerator::generate(Chunk &chunk) {
  // Explicit initialization — prevent UB from an unset heightMap
  for (int x = 0; x < Chunk::SIZE; ++x)
    for (int z = 0; z < Chunk::SIZE; ++z) {
      chunk.heightMap[x][z] = 0;
      for (int y = 0; y < Chunk::HEIGHT; ++y)
        chunk.blocks[x][y][z] = BlockType::Air;
    }

  // Calculate all terrain data once — cache is used by all three passes
  const ColumnGrid cache = buildColumnCache(chunk);

  generateBaseTerrain(chunk, cache);
  generateSurface(chunk, cache);
  generateCaves(chunk, cache);
}

//  HEIGHT PIPELINE

int TerrainGenerator::computeBaseHeight(const TerrainSample &t) {
  int floorH = 14 + static_cast<int>(t.continentalness * 6.0f);

  if (t.river < Setting::riverThreshold) {
    const float rf = 1.0f - (t.river / Setting::riverThreshold);
    floorH -= static_cast<int>(rf * rf * 6.0f);
  }

  return std::clamp(floorH, 3, Chunk::HEIGHT - 1);
}

int TerrainGenerator::applyPlateauLift(int baseH, const TerrainSample &t) {
  if (t.plateau < Setting::plateauThreshold)
    return baseH;
  return Setting::plateauBaseHeight + Setting::plateauHeight;
}

int TerrainGenerator::applyTerrace(int h, const TerrainSample &t) {
  const float p = t.plateau;
  if (p < 0.25f || p > 0.82f)
    return h;

  const float window = (p - 0.25f) / (0.82f - 0.25f);
  float strength = 1.0f - std::abs(window * 2.0f - 1.0f);
  strength = std::pow(strength, 0.7f);

  const float erosionShift = (t.cliffMask - 0.5f) * Setting::cliffErosionStr;
  const int th = Setting::terraceHeight;
  const int hShifted = static_cast<int>(h + erosionShift);
  const int snapped = (hShifted / th) * th;

  return (strength * 0.85f > 0.5f) ? snapped : h;
}

int TerrainGenerator::applyMountainTop(int h, const TerrainSample &t) {
  if (t.plateau < Setting::mountainCoreThreshold)
    return h;

  const float plateauDepth = (t.plateau - Setting::mountainCoreThreshold) /
                             (1.0f - Setting::mountainCoreThreshold);
  const float mountainProfile = std::pow(plateauDepth, 2.0f);
  const float peakFactor = std::pow(t.peaks, 3.0f);

  return h +
         static_cast<int>(mountainProfile * peakFactor * Setting::peakHeight);
}

int TerrainGenerator::applyErosion(int h, const TerrainSample &t) {
  float erosionMult = 1.0f - (t.plateau * 0.8f);
  erosionMult = std::max(0.2f, erosionMult);
  return h -
         static_cast<int>(t.erosion * Setting::erosionStrength * erosionMult);
}

//  PILLAR SUPPORT

bool TerrainGenerator::shouldSpawnPillar(const TerrainSample &t) {
  return (t.plateau >= Setting::plateauThreshold + 0.05f) &&
         (t.pillar >= 0.48f);
}

void TerrainGenerator::fillPillar(Chunk &chunk, int lx, int lz, int topH,
                                  int bottomH) {
  bottomH = std::max(0, bottomH);
  topH = std::min(Chunk::HEIGHT - 1, topH);
  if (bottomH > topH)
    return;

  for (int y = bottomH; y <= topH; ++y)
    chunk.blocks[lx][y][lz] = BlockType::Stone;
}

//  ISLAND GEOMETRY HELPER — single source of truth for the bodyBottom formula

int TerrainGenerator::estimateBodyBottom(int flatPlateauH, float pDepth) {
  constexpr int maxThickness = 48;
  constexpr int minThickness = 2;

  const float thicknessFactor = std::pow(pDepth, 1.8f);
  const int bodyThickness =
      minThickness +
      static_cast<int>(thicknessFactor * (maxThickness - minThickness));
  return flatPlateauH - bodyThickness;
}

//  PASS 1 — BASE TERRAIN

void TerrainGenerator::generateBaseTerrain(Chunk &chunk,
                                           const ColumnGrid &cache) {
  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const TerrainSample &terrain = col.terrain;
      const int floorH = col.floorH;
      const float pDepth = col.pDepth;

      // Base world floor
      for (int y = 0; y <= floorH; ++y)
        chunk.blocks[x][y][z] = BlockType::Stone;

      // If inside a non-island zone: small, thin hills
      if (!col.isIsland) {
        const int h =
            std::clamp(floorH + static_cast<int>(terrain.peaks * 10.0f), floorH,
                       floorH + 12);
        chunk.heightMap[x][z] = h;

        for (int y = floorH + 1; y <= h; ++y)
          chunk.blocks[x][y][z] = BlockType::Stone;

        // FLOATING ISLAND GENERATION PROCEDURE
        const float groupNoise =
            FBMNoise::generate(worldX * 0.0035f, worldZ * 0.0035f, 2, 0.5f,
                               0.5f, Setting::seed + 432);
        const float groupMask = (groupNoise + 1.0f) * 0.5f;

        if (groupMask > 0.50f) {

          const float islandNoise =
              FBMNoise::generate(worldX * 0.045f, worldZ * 0.045f, 3, 0.55f,
                                 0.5f, Setting::seed + 876);
          const float islandMask = (islandNoise + 1.0f) * 0.5f;
          const float islandThreshold = 0.50f;

          if (islandMask > islandThreshold) {
            
            // Perform sample checks 2 blocks away in all 4 cardinal directions
            constexpr int checkDist = 2; 
            float maskN = (FBMNoise::generate((worldX + checkDist) * 0.045f, worldZ * 0.045f, 3, 0.55f, 0.5f, Setting::seed + 876) + 1.0f) * 0.5f;
            float maskS = (FBMNoise::generate((worldX - checkDist) * 0.045f, worldZ * 0.045f, 3, 0.55f, 0.5f, Setting::seed + 876) + 1.0f) * 0.5f;
            float maskE = (FBMNoise::generate(worldX * 0.045f, (worldZ + checkDist) * 0.045f, 3, 0.55f, 0.5f, Setting::seed + 876) + 1.0f) * 0.5f;
            float maskW = (FBMNoise::generate(worldX * 0.045f, (worldZ - checkDist) * 0.045f, 3, 0.55f, 0.5f, Setting::seed + 876) + 1.0f) * 0.5f;

            // If the value drops below the threshold in all directions, discard/ignore this column
            if (maskN < islandThreshold && maskS < islandThreshold && maskE < islandThreshold && maskW < islandThreshold) {
              continue;
            }

            float intensity =
                (islandMask - islandThreshold) / (1.0f - islandThreshold);

            const float vNoise =
                FBMNoise::generate(worldX * 0.01f, worldZ * 0.01f, 2, 0.5f,
                                   0.5f, Setting::seed + 555);
            int heightOffset = static_cast<int>(vNoise * 20.0f);

            int coneTop = 155 + heightOffset;
            int coneBottom = 110 + heightOffset;

            coneTop = std::min(coneTop, Chunk::HEIGHT - 10);
            coneBottom = std::max(coneBottom, 40);

            for (int y = coneBottom; y <= coneTop; ++y) {
              float heightFactor = static_cast<float>(y - coneBottom) /
                                   static_cast<float>(coneTop - coneBottom);

              float requiredIntensity = std::pow(1.0f - heightFactor, 1.4f);
              float roughness = FBMNoise::generate(worldX * 0.2f, y * 0.15f, 2,
                                                   0.5f, 0.5f, Setting::seed) *
                                0.07f;

              if (intensity > (requiredIntensity + roughness)) {
                chunk.blocks[x][y][z] = BlockType::Stone;

                if (y > chunk.heightMap[x][z]) {
                  chunk.heightMap[x][z] = y;
                }
              }
            }
          }
        }
        continue;
      }

      // 1. Thin edge cutoffs
      const bool corePillarArea =
          (pDepth > Setting::islandCorePillarThreshold) &&
          shouldSpawnPillar(terrain);

      if (!corePillarArea && pDepth < Setting::islandEdgeCutoff) {
        chunk.heightMap[x][z] = floorH;
        continue;
      }

      // 2. Island height variation
      const float n1 = FBMNoise::generate((worldX + 54321) * 0.0018f,
                                          (worldZ + 12345) * 0.0018f, 3, 0.5f,
                                          0.5f, Setting::seed + 225);
      const float hVariation = (n1 + 1.0f) * 0.5f;
      const int flatPlateauH = std::clamp(
          100 + static_cast<int>(hVariation * 75.0f), 100, Chunk::HEIGHT - 50);

      // 3. Dome mountains
      int h = flatPlateauH;
      constexpr float blendStart = 0.35f;

      if (pDepth > blendStart) {
        const float tBlend = (pDepth - blendStart) / (1.0f - blendStart);
        const float tSmooth = tBlend * tBlend * (3.0f - 2.0f * tBlend);

        const float peakBlend =
            std::max(0.0f, (tBlend - Setting::mountainCoreThreshold) /
                               (1.0f - Setting::mountainCoreThreshold));
        const float peakFactor = std::sqrt(std::max(0.0f, terrain.peaks));

        h += static_cast<int>(tSmooth * 40.0f) +
             static_cast<int>(peakBlend * peakFactor * Setting::peakHeight);
      } else {
        h += static_cast<int>(terrain.erosion * 0.5f);
      }

      h = applyErosion(h, terrain);
      h = std::clamp(h, flatPlateauH - 4, Chunk::HEIGHT - 1);
      chunk.heightMap[x][z] = h;

      // 4. Under-belly (gets thinner towards the edges)
      const int bodyBottom =
          std::max(floorH + 22, estimateBodyBottom(flatPlateauH, pDepth));

      if (bodyBottom >= h) {
        chunk.heightMap[x][z] = floorH;
        continue;
      }

      for (int y = bodyBottom; y <= h; ++y)
        chunk.blocks[x][y][z] = BlockType::Stone;

      // 5. Pillars & spikes
      const int stemTop = bodyBottom;
      const int stemBottom = floorH + 1;

      const float spikeNoise = FBMNoise::generate(
          worldX * Setting::spikeNoiseScale, worldZ * Setting::spikeNoiseScale,
          4, 0.65f, 0.20f, Setting::seed + 77);

      const float pillarWidthMod = FBMNoise::generate(
          worldX * 0.08f, worldZ * 0.08f, 2, 0.5f, 0.5f, Setting::seed + 999);

      int maxStalactiteLen = 0;
      int maxStalagmiteLen = 0;

      const float dynamicSpawnThreshold =
          Setting::spikeSpawnThreshold + (1.0f - pDepth) * 0.28f;

      if (spikeNoise > dynamicSpawnThreshold) {
        const float spikeFactor =
            std::pow((spikeNoise - dynamicSpawnThreshold) /
                         (1.0f - dynamicSpawnThreshold),
                     1.2f);
        const float lengthScale = 0.4f + 0.6f * pDepth;

        maxStalactiteLen = static_cast<int>(
            spikeFactor * Setting::stalactiteMaxLen * lengthScale);
        maxStalagmiteLen = static_cast<int>(
            spikeFactor * Setting::stalagmiteMaxLen * lengthScale);
      }

      const int stalagBase = floorH + 1;

      for (int y = stemBottom; y < stemTop; ++y) {
        const float tNorm = static_cast<float>(y - stemBottom) /
                            static_cast<float>(stemTop - stemBottom);
        const float midDist = std::abs(tNorm - 0.5f);
        const float hourglassFactor = midDist * midDist * 4.0f;

        const float stemThreshold =
            (0.91f + (pillarWidthMod * 0.8f)) - (hourglassFactor * 0.20f);
        const bool isMainPillar =
            corePillarArea && (terrain.pillar >= stemThreshold);

        // Stalactite
        bool isStalactite = false;
        if (maxStalactiteLen > 0 && y >= stemTop - maxStalactiteLen &&
            y < stemTop) {
          const float tStala = static_cast<float>(stemTop - y) /
                               static_cast<float>(maxStalactiteLen);
          const float taperCurve =
              std::pow(tStala, Setting::spikeTaperExponent) *
              Setting::spikeTaperCurve;
          isStalactite =
              spikeNoise >
              (0.005f + taperCurve - pDepth * Setting::spikeDepthThicknessMult);
        }

        // Stalagmite
        bool isStalagmite = false;
        if (maxStalagmiteLen > 0 && y >= stalagBase &&
            y <= stalagBase + maxStalagmiteLen) {
          const float tStalag = static_cast<float>(y - stalagBase) /
                                static_cast<float>(maxStalagmiteLen);
          const float taperCurve =
              std::pow(tStalag, Setting::spikeTaperExponent) *
              Setting::spikeTaperCurve;
          isStalagmite =
              spikeNoise >
              (0.005f + taperCurve - pDepth * Setting::spikeDepthThicknessMult);
        }

        if (isMainPillar || isStalactite || isStalagmite)
          chunk.blocks[x][y][z] = BlockType::Stone;
      }
    }
  }
}

//  PASS 2 — SURFACE (biome-aware block replacement)

void TerrainGenerator::generateSurface(Chunk &chunk, const ColumnGrid &cache) {
  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const TerrainSample &terrain = col.terrain;
      const float pDepth = col.pDepth;
      const int floorH = col.floorH;

      ClimateSample climate = ClimateSampler::sample(worldX, worldZ);

      if (col.isIsland) {
        if (pDepth > Setting::mountainCoreThreshold) {
          climate.temperature = 0.35f;
          climate.humidity = 0.25f;
        } else {
          if (climate.temperature > Setting::desertTemperature)
            climate.humidity = 0.15f;
          else {
            climate.temperature = 0.50f;
            climate.humidity = 0.50f;
          }
        }
      }

      Biome *biome = BiomeManager::getBiome(terrain, climate);

      const int surfaceH = chunk.heightMap[x][z];
      const bool isIsland = col.isIsland;

      const bool validIsland =
          isIsland ? (surfaceH > floorH + 20) : (surfaceH > 90);

      int layerDepth = 0;

      for (int y = Chunk::HEIGHT - 1; y >= 0; --y) {
        if (chunk.blocks[x][y][z] != BlockType::Stone)
          continue;

        // Skip blocks that are far below the surface
        if (validIsland) {
          if (y < surfaceH - 5) {
            layerDepth = 0;
            continue;
          }
        } else {
          if (y > 25) {
            layerDepth = 0;
            continue;
          }
        }

        const bool airAbove = (y + 1 >= Chunk::HEIGHT) ||
                              (chunk.blocks[x][y + 1][z] == BlockType::Air);

        if (airAbove) {
          if (y < 20 && terrain.river < Setting::riverThreshold) {
            chunk.blocks[x][y][z] = BlockType::Dirt;
            layerDepth = 0;
          } else {
            chunk.blocks[x][y][z] = biome->getTopBlock();
            layerDepth = 4;
          }
        } else if (layerDepth > 0) {
          chunk.blocks[x][y][z] = biome->getMiddleBlock();
          --layerDepth;
        }
      }
    }
  }
}

//  PASS 3 — CAVES

void TerrainGenerator::generateCaves(Chunk &chunk, const ColumnGrid &cache) {
  for (int x = 0; x < Chunk::SIZE; ++x) {
    for (int z = 0; z < Chunk::SIZE; ++z) {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const int surfaceH = chunk.heightMap[x][z];

      int caveMax = std::min(90, static_cast<int>(surfaceH * 0.70f));

      if (col.isIsland) {
        constexpr int conservativeFlatPlateauH = 100;
        const int estimatedBodyBottom =
            estimateBodyBottom(conservativeFlatPlateauH, col.pDepth);

        caveMax = std::min(caveMax, estimatedBodyBottom - 2);
      }

      if (caveMax <= 5)
        continue;

      for (int y = 5; y < caveMax; ++y) {
        const float cave = FBMNoise::generate(worldX + y * 2, worldZ + y * 2, 3,
                                              0.5f, 0.045f, Setting::seed);

        if (cave > 0.72f) {
          const bool hasNeighborStone =
              (y > 0 && chunk.blocks[x][y - 1][z] == BlockType::Stone) ||
              (y < Chunk::HEIGHT - 1 &&
               chunk.blocks[x][y + 1][z] == BlockType::Stone);

          if (hasNeighborStone)
            chunk.blocks[x][y][z] = BlockType::Air;
        }
      }
    }
  }
}