#include "HeavenBiome.h"

// Heaven Plains Biome
BlockType HeavenPlainsBiome::getTopBlock() {
    return BlockType::Grass;
}
BlockType HeavenPlainsBiome::getMiddleBlock() {
    return BlockType::Dirt;
}
BlockType HeavenPlainsBiome::getBottomBlock() {
    return BlockType::Heavenstone;
}

// Heaven Crystal Biome
BlockType HeavenCrystalBiome::getTopBlock() {
    return BlockType::Crystal;
}
BlockType HeavenCrystalBiome::getMiddleBlock() {
    return BlockType::Heavenstone;
}
BlockType HeavenCrystalBiome::getBottomBlock() {
    return BlockType::Heavenstone;
}
