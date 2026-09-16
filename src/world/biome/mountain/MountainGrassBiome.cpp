#include "MountainGrassBiome.h"

BlockType MountainGrassBiome::getTopBlock()    { return BlockType::Grass; }
BlockType MountainGrassBiome::getMiddleBlock() { return BlockType::Stone; }
BlockType MountainGrassBiome::getBottomBlock() { return BlockType::Stone;  }
