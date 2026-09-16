#pragma once

#include "../Biome.h"

// Sub-biome gunung luar: batu di permukaan tapi masih ada grass/dirt di bawahnya.
// Berfungsi sebagai buffer wajib antara Plains dan MountainRock —
// MountainRock tidak bisa bersebelahan langsung dengan Plains karena jarak
// idealPeaks mereka (1.0 vs 0.0) terlalu jauh, MountainGrass (0.62) selalu ada di antaranya.
class MountainGrassBiome : public Biome {
public:
    const char* getName() override { return "Mountain Grass"; }

    BlockType getTopBlock() override;
    BlockType getMiddleBlock() override;
    BlockType getBottomBlock() override;

    float idealTemp()     const override { return 0.35f; }  // sejuk
    float idealHumidity() const override { return 0.45f; }  // sedikit lembab
    float idealPeaks()    const override { return 0.62f; }  // tengah antara plain(0) dan rock(1)
};
