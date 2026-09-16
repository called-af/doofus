#pragma once

#include "../Biome.h"

class PlainBiome : public Biome
{
public:
    const char* getName() override { return "Plains"; }

    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;

    float idealTemp()     const override { return 0.5f;  }  // tepat di tengah range
    float idealHumidity() const override { return 0.5f;  }  // tepat di tengah range
    float idealPeaks()    const override { return 0.0f;  }  // dataran flat
};