#include "CellularNoise.h"
#include "Noise.h"

#include <cmath>
#include <algorithm>

CellularSample CellularNoise::generate(float x, float z, int seed) {
    int cellX = (int)std::floor(x);
    int cellZ = (int)std::floor(z);

    float minDist1 = 999999.0f;
    float minDist2 = 999999.0f;
    int closestCellX = 0;
    int closestCellZ = 0;

    for (int offsetX = -1; offsetX <= 1; offsetX++) {
        for (int offsetZ = -1; offsetZ <= 1; offsetZ++) {
            int pointX = cellX + offsetX;
            int pointZ = cellZ + offsetZ;

            float randomX = Noise::random(pointX, pointZ, seed);
            float randomZ = Noise::random(pointX, pointZ, seed + 999);

            float featureX = pointX + randomX;
            float featureZ = pointZ + randomZ;

            float dx = featureX - x;
            float dz = featureZ - z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist < minDist1) {
                minDist2 = minDist1;
                minDist1 = dist;
                closestCellX = pointX;
                closestCellZ = pointZ;
            } else if (dist < minDist2) {
                minDist2 = dist;
            }
        }
    }

    CellularSample result;
    result.f1 = std::min(minDist1 / 1.5f, 1.0f);
    result.f2 = std::min(minDist2 / 1.5f, 1.0f);
    result.cellId = Noise::random(closestCellX, closestCellZ, seed + 5000);
    return result;
}

CellularSample CellularNoise::generate(float x, float z, float scale, int seed) {
    return generate(x * scale, z * scale, seed);
}