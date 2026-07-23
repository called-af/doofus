#pragma once

#include "BlockType.h"

struct BlockUV {
  float u1;
  float v1;

  float u2;
  float v2;
};

class BlockData {
public:
  static BlockUV get(BlockType type) {
    float tile = 1.0f / 4.0f;

    switch (type) {
    case BlockType::Grass:
      return {0.0f, 0.0f, tile, tile};

    case BlockType::Dirt:
      return {tile, 0.0f, tile * 2.0f, tile};

    case BlockType::Stone:
      return {tile * 2.0f, 0.0f, tile * 3.0f, tile};

    case BlockType::Sand:
      return {tile * 3.0f, 0.0f, tile * 4.0f, tile};

    case BlockType::Basalt:
      return {0.0f, tile, tile, tile * 2.0f};

    case BlockType::Obsidian:
      return {tile, tile, tile * 2.0f, tile * 2.0f};

    case BlockType::Ash:
      return {tile * 2.0f, tile, tile * 3.0f, tile * 2.0f};

    case BlockType::Cinder:
      return {tile * 3.0f, tile, tile * 4.0f, tile * 2.0f};

    case BlockType::Lava:
      return {0.0f, tile * 2.0f, tile, tile * 3.0f};

    case BlockType::Heavenstone:
      return {tile, tile * 2.0f, tile * 2.0f, tile * 3.0f};

    case BlockType::Crystal:
      return {tile * 2.0f, tile * 2.0f, tile * 3.0f, tile * 3.0f};

    default:
      return {0, 0, 0, 0};
    }
  }
};