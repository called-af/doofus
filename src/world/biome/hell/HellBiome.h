#pragma once

#include "../Biome.h"

class HellLavaVeinBiome : public Biome
{
public:
    const char* getName() override { return "Hell Lava Veins"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};

class HellBasaltCragsBiome : public Biome
{
public:
    const char* getName() override { return "Hell Basalt Crags"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};

class HellAshlandsBiome : public Biome
{
public:
    const char* getName() override { return "Hell Volcanic Ashlands"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};
