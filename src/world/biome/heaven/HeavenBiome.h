#pragma once

#include "../Biome.h"

class HeavenPlainsBiome : public Biome
{
public:
    const char* getName() override { return "Heaven Celestial Plains"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};

class HeavenCrystalBiome : public Biome
{
public:
    const char* getName() override { return "Heaven Crystal Sanctuary"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};
