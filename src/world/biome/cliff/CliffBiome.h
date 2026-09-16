#pragma once

#include "../Biome.h"

class CliffBiome : public Biome
{
public:
    const char* getName() override { return "Mountain Cliff"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;

    float idealTemp()     const override { return 0.25f; }
    float idealHumidity() const override { return 0.35f; }
    float idealPeaks()    const override { return 0.9f;  }
};
