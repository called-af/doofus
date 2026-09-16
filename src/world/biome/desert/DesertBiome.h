#pragma once
#include "../Biome.h"

class DesertBiome : public Biome {
public:
    const char* getName() override { return "Desert"; }

    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;

    float idealTemp()     const override { return 0.9f;  }  // sangat panas
    float idealHumidity() const override { return 0.1f;  }  // sangat kering
    float idealPeaks()    const override { return 0.0f;  }  // dataran
};