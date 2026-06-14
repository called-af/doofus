#pragma once

#include "Chunk.h"

class TerrainGenerator {
public:
  static void generate(Chunk &chunk);
  

private:
  static void generateBaseTerrain(Chunk &chunk);

  static void generateMountains(Chunk &chunk);

  // static void generatePillars(
  //     Chunk& chunk
  // );

  static void generateSurface(Chunk &chunk);

  static void generateCaves(Chunk &chunk);
  
};