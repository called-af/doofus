#pragma once

#include "../Biome.h"

class HellLavaVeinBiome : public Biome
{
public:
    const char* getName() override { return "Hell Lava Veins"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
    float idealTemp()     const override { return 1.0f; }
    float idealHumidity() const override { return 0.0f; }
    float idealPeaks()    const override { return 0.0f; }
};

class HellBasaltCragsBiome : public Biome
{
public:
    const char* getName() override { return "Hell Basalt Crags"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
    float idealTemp()     const override { return 1.0f; }
    float idealHumidity() const override { return 0.0f; }
    float idealPeaks()    const override { return 0.5f; }
};

class HellAshlandsBiome : public Biome
{
public:
    const char* getName() override { return "Hell Volcanic Ashlands"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
    float idealTemp()     const override { return 1.0f; }
    float idealHumidity() const override { return 0.0f; }
    float idealPeaks()    const override { return 0.0f; }
};
