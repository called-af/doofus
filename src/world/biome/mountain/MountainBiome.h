#pragma once

#include "../Biome.h"

// Sub-biome gunung dalam: full batu dan abu — hanya muncul di puncak peaks tinggi
class MountainBiome : public Biome {
public:
    const char* getName() override { return "Mountain"; }

    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;

    // Puncak tertinggi — idealPeaks jauh dari Plains (0.0) supaya tidak bisa bersebelahan langsung
    float idealTemp()     const override { return 0.25f; }
    float idealHumidity() const override { return 0.35f; }
    float idealPeaks()    const override { return 1.0f;  }
};