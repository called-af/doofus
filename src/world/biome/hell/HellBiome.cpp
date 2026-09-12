#include "HellBiome.h"

// Hell Lava Vein Biome
BlockType HellLavaVeinBiome::getTopBlock() {
    return BlockType::Lava;
}
BlockType HellLavaVeinBiome::getMiddleBlock() {
    return BlockType::Obsidian;
}
BlockType HellLavaVeinBiome::getBottomBlock() {
    return BlockType::Basalt;
}

// Hell Basalt Crags Biome
BlockType HellBasaltCragsBiome::getTopBlock() {
    return BlockType::Basalt;
}
BlockType HellBasaltCragsBiome::getMiddleBlock() {
    return BlockType::Basalt;
}
BlockType HellBasaltCragsBiome::getBottomBlock() {
    return BlockType::Basalt;
}

// Hell Volcanic Ashlands Biome
BlockType HellAshlandsBiome::getTopBlock() {
    return BlockType::Ash;
}
BlockType HellAshlandsBiome::getMiddleBlock() {
    return BlockType::Basalt;
}
BlockType HellAshlandsBiome::getBottomBlock() {
    return BlockType::Basalt;
}
