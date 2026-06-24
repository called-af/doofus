#include "TerrainSampler.h"

#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"

#include <cmath>

#include "../../core/Setting.h"

TerrainSample TerrainSampler::sample(int worldX, int worldZ)
{
    TerrainSample terrain;

    // ─────────────────────────────────────────────────────────────
    //  CONTINENTALNESS
    //  0 = ocean / coast
    //  1 = deep inland
    // ─────────────────────────────────────────────────────────────
    terrain.continentalness =
        (FBMNoise::generate(worldX, worldZ, 4, 0.5f,
                            Setting::continentalScale, Setting::seed) +
         1.0f) *
        0.5f;

    // ─────────────────────────────────────────────────────────────
    //  PEAKS
    //  0 = valley / flat
    //  1 = sharp mountain ridge
    // ─────────────────────────────────────────────────────────────
    terrain.peaks = RidgeNoise::generate(
        worldX, worldZ, 5, 0.5f, Setting::peaksScale, Setting::seed + 100);

    // ─────────────────────────────────────────────────────────────
    //  EROSION
    //  0 = rough / eroded
    //  1 = smooth / intact
    // ─────────────────────────────────────────────────────────────
    terrain.erosion =
        (FBMNoise::generate(worldX, worldZ, 4, 0.5f,
                            Setting::erosionScale, Setting::seed + 200) +
         1.0f) *
        0.5f;

    // ─────────────────────────────────────────────────────────────
    //  RIVER
    //  0 = tepat di tengah sungai
    //  1 = jauh dari sungai
    // ─────────────────────────────────────────────────────────────
    terrain.river = std::abs(FBMNoise::generate(
        worldX, worldZ, 3, 0.5f, Setting::riverScale, Setting::seed + 300));

    // ─────────────────────────────────────────────────────────────
    //  PLATEAU MASK
    //  Ridge noise berskala besar → tinggi di puncak plateau, mendekati 0
    //  di tepi dan valley. Ini yang menentukan lokasi & bentuk plateau.
    //  Frequency rendah = plateau lebar & massif.
    // ─────────────────────────────────────────────────────────────
    terrain.plateau = RidgeNoise::generate(
        worldX, worldZ, 4, 0.6f,
        Setting::plateauScale, Setting::seed + 600);

    // ─────────────────────────────────────────────────────────────
    //  PILLAR RIDGE
    //  Ridge noise lebih kasar & frekuensi lebih tinggi dari plateau.
    //  Dipakai untuk memilih posisi pilar batu di bawah tepi cliff.
    // ─────────────────────────────────────────────────────────────
    terrain.pillar = RidgeNoise::generate(
        worldX, worldZ, 3, 0.5f,
        Setting::pillarScale, Setting::seed + 700);

    // ─────────────────────────────────────────────────────────────
    //  CLIFF EROSION MASK
    //  FBM kasar untuk memecah tepi cliff agar tidak rata sempurna.
    //  Nilai 0.0–1.0: dipakai untuk menggeser batas terrace per-kolom.
    // ─────────────────────────────────────────────────────────────
    terrain.cliffMask =
        (FBMNoise::generate(worldX, worldZ, 3, 0.7f,
                            Setting::cliffErosionScale, Setting::seed + 800) +
         1.0f) *
        0.5f;

    return terrain;
}