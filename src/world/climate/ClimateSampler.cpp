#include "ClimateSampler.h"

#include "../noise/FBMNoise.h"

#include "../../core/Setting.h"

ClimateSample ClimateSampler::sample(int worldX, int worldZ) {
  ClimateSample climate;

  // Domain warp — offset koordinat sebelum sampling supaya garis batas biome
  // tidak membentuk kontur bulat sempurna dari noise mentah, melainkan bergelombang alami
  const float warpX = FBMNoise::generate(
      worldX * Setting::climateWarpScale, worldZ * Setting::climateWarpScale,
      2, 0.5f, 1.0f, Setting::seed + 9001) * Setting::climateWarpStrength;
  const float warpZ = FBMNoise::generate(
      worldX * Setting::climateWarpScale, worldZ * Setting::climateWarpScale,
      2, 0.5f, 1.0f, Setting::seed + 9002) * Setting::climateWarpStrength;

  const float wx = static_cast<float>(worldX) + warpX;
  const float wz = static_cast<float>(worldZ) + warpZ;

  climate.temperature =
      (FBMNoise::generate(wx, wz, 3, 0.5f, Setting::temperatureScale,
                          Setting::seed + 400) +
       1.0f) *
      0.5f;

  climate.humidity =
      (FBMNoise::generate(wx, wz, 3, 0.5f, Setting::humidityScale,
                          Setting::seed + 500) +
       1.0f) *
      0.5f;

  return climate;
}