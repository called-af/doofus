#pragma once

#include "../block/BlockType.h"
#include <string>
#include <cmath>
#include <algorithm>

class Biome
{
public:
    virtual ~Biome() = default;

    virtual const char* getName() = 0;

    virtual BlockType getTopBlock() = 0;
    virtual BlockType getMiddleBlock() = 0;
    virtual BlockType getBottomBlock() = 0;

    // Kondisi iklim ideal biome ini di ruang (temperature, humidity, peaks)
    // Dipakai BiomeManager untuk hitung jarak antar biome — bukan threshold biner
    virtual float idealTemp()     const = 0;
    virtual float idealHumidity() const = 0;
    virtual float idealPeaks()    const = 0;  // 0 = dataran, 1 = pegunungan

    // Euclidean distance di ruang climate+peaks (ternormalisasi)
    // Makin kecil = kondisi saat ini makin cocok untuk biome ini
    float distanceTo(float temperature, float humidity, float peaks) const
    {
        const float dt = temperature - idealTemp();
        const float dh = humidity    - idealHumidity();
        // peaks diberi bobot lebih kecil supaya mountain hanya dominan
        // di zona yang benar-benar tinggi, bukan menggerus plains di mana-mana
        const float dp = (peaks - idealPeaks()) * 0.8f;
        return std::sqrt(dt*dt + dh*dh + dp*dp);
    }
};