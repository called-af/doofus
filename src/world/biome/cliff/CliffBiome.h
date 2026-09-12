#pragma once

#include "../Biome.h"

class CliffBiome : public Biome
{
public:
    const char* getName() override { return "Mountain Cliff"; }
    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;
};
