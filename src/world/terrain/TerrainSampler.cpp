#include "TerrainSampler.h"

#include "../noise/FBMNoise.h"
#include "../noise/RidgeNoise.h"
#include "../noise/CellularNoise.h"

#include <cmath>

#include "../../core/Setting.h"

TerrainSample TerrainSampler::sample(int worldX, int worldZ)
{
    TerrainSample terrain;

    //  CONTINENTALNESS
    //  0 = ocean / coast
    //  1 = deep inland
    terrain.continentalness =
        (FBMNoise::generate(worldX, worldZ, 4, 0.5f,
                            Setting::continentalScale, Setting::seed) +
         1.0f) *
        0.5f;

    //  PEAKS
    //  0 = valley / flat
    //  1 = sharp mountain ridge
    terrain.peaks = RidgeNoise::generate(
        worldX, worldZ, 5, 0.5f, Setting::peaksScale, Setting::seed + 100);

    //  EROSION
    //  0 = rough / eroded
    //  1 = smooth / intact
    terrain.erosion =
        (FBMNoise::generate(worldX, worldZ, 4, 0.5f,
                            Setting::erosionScale, Setting::seed + 200) +
         1.0f) *
        0.5f;

    //  RIVER
    //  0 = exactly at the river center
    //  1 = far away from a river
    terrain.river = std::abs(FBMNoise::generate(
        worldX, worldZ, 3, 0.5f, Setting::riverScale, Setting::seed + 300));

    //  PLATEAU MASK
    //  Large-scale ridge noise — high at plateau tops, approaching 0
    //  at edges and valleys. Determines the location and shape of plateaus.
    //  Low frequency = wide and massive plateaus.
    terrain.plateau = RidgeNoise::generate(
        worldX, worldZ, 4, 0.6f,
        Setting::plateauScale, Setting::seed + 600);

    //  PILLAR RIDGE
    //  Rougher ridge noise at higher frequency than plateau noise.
    //  Used to select stone pillar positions under cliff edges.
    terrain.pillar = RidgeNoise::generate(
        worldX, worldZ, 3, 0.5f,
        Setting::pillarScale, Setting::seed + 700);

    //  CLIFF EROSION MASK
    //  Rough FBM noise to break cliff edges so they are not perfectly flat.
    //  Value range 0.0 to 1.0: used to shift terrace boundaries per column.
    terrain.cliffMask =
        (FBMNoise::generate(worldX, worldZ, 3, 0.7f,
                            Setting::cliffErosionScale, Setting::seed + 800) +
         1.0f) *
        0.5f;

    // ─────────────────────────────────────────
    //  DOMAIN WARP
    //  Offset koordinat sample sebelum masuk ke cellular/mountain noise,
    //  biar silhouette island nggak radial-simetris sempurna.
    // ─────────────────────────────────────────
    float warpX = FBMNoise::generate(worldX, worldZ, 3, 0.5f,
                                     Setting::islandWarpScale, Setting::seed + 900);
    float warpZ = FBMNoise::generate(worldX, worldZ, 3, 0.5f,
                                     Setting::islandWarpScale, Setting::seed + 950);

    float warpedX = worldX + warpX * Setting::islandWarpStrength;
    float warpedZ = worldZ + warpZ * Setting::islandWarpStrength;

    // ─────────────────────────────────────────
    //  ISLAND CELLULAR PLACEMENT
    //  F1 nentuin jarak ke center pulau terdekat (buat radial falloff).
    //  cellId nentuin variasi ukuran/tinggi unik per-pulau.
    // ─────────────────────────────────────────
    CellularSample islandCell = CellularNoise::generate(
        warpedX, warpedZ, Setting::islandCellScale, Setting::seed + 1000);

    terrain.islandF1 = islandCell.f1;
    terrain.islandF2 = islandCell.f2;
    terrain.islandCellId = islandCell.cellId;

    // ─────────────────────────────────────────
    //  ISLAND MOUNTAIN
    //  Ridge noise freq lebih tinggi, dipakai buat spike di atas anchor island.
    // ─────────────────────────────────────────
    terrain.islandMountain = RidgeNoise::generate(
        warpedX, warpedZ, 4, 0.55f,
        Setting::islandMountainScale, Setting::seed + 1100);

    return terrain;
}