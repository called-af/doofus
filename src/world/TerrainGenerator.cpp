#include "TerrainGenerator.h"

#include "biome/BiomeManager.h"
#include "climate/ClimateSampler.h"
#include "noise/FBMNoise.h"
#include "terrain/TerrainSampler.h"
#include "noise/RidgeNoise.h"

#include "../core/Setting.h"

#include <algorithm>
#include <cmath>

//  FEATURE-POINT SEED GENERATION & ARCHIPELAGO CLUSTERS (Tier 3 — Heaven Realm)

FeatureSeed TerrainGenerator::generateHeavenSeed(int cellX, int cellZ)
{
  FeatureSeed s{};
  uint32_t h1 = static_cast<uint32_t>(cellX * 73856093 ^ cellZ * 19349663 ^ Setting::seed * 83492791);
  uint32_t h2 = h1 * 1664525u + 1013904223u;
  uint32_t h3 = h2 * 1664525u + 1013904223u;
  uint32_t h4 = h3 * 1664525u + 1013904223u;
  uint32_t h5 = h4 * 1664525u + 1013904223u;
  uint32_t h6 = h5 * 1664525u + 1013904223u;
  uint32_t h7 = h6 * 1664525u + 1013904223u;
  uint32_t h8 = h7 * 1664525u + 1013904223u;

  // Rare spawn check per grid cell
  if ((h7 % 100) > static_cast<int>(Setting::heavenSpawnChance * 100.0f))
  {
    s.exists = false;
    return s;
  }

  s.exists = true;

  const float g = Setting::heavenClusterSpacing;
  float rx = (static_cast<float>(h1 % 10000) / 10000.0f) * (g * 0.60f) + (g * 0.20f);
  float rz = (static_cast<float>(h2 % 10000) / 10000.0f) * (g * 0.60f) + (g * 0.20f);

  s.x = cellX * g + rx;
  s.z = cellZ * g + rz;

  s.angle = (static_cast<float>(h7 % 6283) / 1000.0f);        // Rotation angle [0, 2PI]
  s.aspect = 1.0f + (static_cast<float>(h8 % 120) / 100.0f);  // Stretched aspect ratio 1.0 - 2.2

  // Size distribution: 15% Giant Anchor Cluster, 40% Medium Cluster, 45% Small Cluster
  int sizeRoll = h3 % 100;
  if (sizeRoll < 15)
  {
    // GIANT ANCHOR CLUSTER
    s.isGiant = true;
    s.isAnchor = true;
    s.radius = Setting::heavenGiantRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenGiantRadiusMax - Setting::heavenGiantRadiusMin);
    s.thickness = 55.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 25.0f;
  }
  else if (sizeRoll < 55)
  {
    // MEDIUM CLUSTER
    s.isGiant = false;
    s.isAnchor = (h6 % 3 == 0); // 33% of medium clusters are landmark anchor islands
    s.radius = Setting::heavenMediumRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenMediumRadiusMax - Setting::heavenMediumRadiusMin);
    s.thickness = 28.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 20.0f;
  }
  else
  {
    // SMALL ISLET CLUSTER
    s.isGiant = false;
    s.isAnchor = false;
    s.radius = Setting::heavenSmallRadiusMin + (static_cast<float>(h4 % 10000) / 10000.0f) * (Setting::heavenSmallRadiusMax - Setting::heavenSmallRadiusMin);
    s.thickness = 12.0f + (static_cast<float>(h5 % 10000) / 10000.0f) * 12.0f;
  }

  s.basePosY = Setting::heavenMinBaseY + (static_cast<float>(h6 % 10000) / 10000.0f) * (Setting::heavenMaxBaseY - Setting::heavenMinBaseY);
  s.hasPeak = true;

  // Generate 2 to 4 satellite sub-islets orbiting the main anchor island at staggered elevations!
  s.subIsletCount = 2 + (h4 % 3);
  uint32_t subH = h8;
  for (int i = 0; i < s.subIsletCount; ++i)
  {
    subH = subH * 1664525u + 1013904223u;
    float polarAngle = (static_cast<float>(subH % 6283) / 1000.0f);
    subH = subH * 1664525u + 1013904223u;
    float distMult = 0.85f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.75f;

    SubIslet &sub = s.subIslets[i];
    float dist = s.radius * distMult;
    sub.offsetX = dist * std::cos(polarAngle);
    sub.offsetZ = dist * std::sin(polarAngle);

    // Staggered Y elevation offset relative to main anchor island (creating multi-layered sky islands)
    subH = subH * 1664525u + 1013904223u;
    sub.offsetY = Setting::heavenSubIsletYMin + (static_cast<float>(subH % 1000) / 1000.0f) * (Setting::heavenSubIsletYMax - Setting::heavenSubIsletYMin);

    subH = subH * 1664525u + 1013904223u;
    sub.radius = s.radius * (0.22f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.28f);

    subH = subH * 1664525u + 1013904223u;
    sub.thickness = s.thickness * (0.35f + (static_cast<float>(subH % 1000) / 1000.0f) * 0.35f);
  }

  return s;
}

//  EVALUATE ISLAND SLICE (Evaluates column slice of any island in cluster)
IslandSlice TerrainGenerator::evaluateIslandSlice(float worldX, float worldZ, const FeatureSeed &s, int islandIndex)
{
  IslandSlice slice{};
  slice.valid = false;
  if (!s.exists)
    return slice;

  float cx, cz, R, T, baseY;
  bool isMain = (islandIndex == 0);

  if (isMain)
  {
    cx = s.x;
    cz = s.z;
    R = s.radius;
    T = s.thickness;
    baseY = s.basePosY;
  }
  else
  {
    const SubIslet &sub = s.subIslets[islandIndex - 1];
    cx = s.x + sub.offsetX;
    cz = s.z + sub.offsetZ;
    R = sub.radius;
    T = sub.thickness;
    baseY = s.basePosY + sub.offsetY;
  }

  // ─────────────────────────────────────────────────────────────────────────
  //  1. DOMAIN WARPING & NON-CIRCULAR ANGULAR NOISE DISTORTION
  //  Prevents artificial circular/elliptical shapes by perturbing the radius
  //  with polar angular noise + multi-octave FBM domain warp.
  // ─────────────────────────────────────────────────────────────────────────
  float dxRaw = worldX - cx;
  float dzRaw = worldZ - cz;
  float angleNorm = std::atan2(dzRaw, dxRaw); // polar angle [-PI, PI]

  // Radial lobe perturbation (breaks circular symmetry into irregular organic coastlines)
  float nLobe = FBMNoise::generate(std::cos(angleNorm * 2.5f) * 1.8f, std::sin(angleNorm * 2.5f) * 1.8f, 3, 0.55f, 0.5f, Setting::seed + 333) * 0.26f;

  // Single domain warp per column (evaluates once per column, NOT per-Y)
  float warpX = FBMNoise::generate(worldX * Setting::heavenWarpScale, worldZ * Setting::heavenWarpScale, 4, 0.55f, 0.5f, Setting::seed + 444) * (R * Setting::heavenWarpStrength);
  float warpZ = FBMNoise::generate(worldX * Setting::heavenWarpScale + 400.0f, worldZ * Setting::heavenWarpScale + 400.0f, 4, 0.55f, 0.5f, Setting::seed + 555) * (R * Setting::heavenWarpStrength);

  float dx = (worldX + warpX) - cx;
  float dz = (worldZ + warpZ) - cz;

  // Rotation angle & aspect ratio stretching
  float cosA = std::cos(s.angle);
  float sinA = std::sin(s.angle);
  float rx = (dx * cosA - dz * sinA);
  float rz = (dx * sinA + dz * cosA) * (isMain ? s.aspect : 1.0f);

  float dist = std::hypot(rx, rz);
  float d = dist / R;

  // Multi-scale organic coastline noise detailing
  float nShape = FBMNoise::generate(worldX * 0.028f, worldZ * 0.028f, 4, 0.55f, 0.5f, Setting::seed + 789) * 0.22f;
  float nDetail = RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ), 3, 0.5f, 0.04f, Setting::seed + 912) * 0.12f;

  slice.effectiveD = d + nLobe + nShape - nDetail;

  if (slice.effectiveD >= 1.0f)
    return slice;

  slice.valid = true;

  // ─────────────────────────────────────────────────────────────────────────
  //  2. TOP SURFACE & MOUNTAIN PEAKS ("kalau gede banget dikasi gunung")
  // ─────────────────────────────────────────────────────────────────────────
  float edgeFactor = 1.0f;
  if (slice.effectiveD > Setting::heavenPlateauRatio)
  {
    float t = (slice.effectiveD - Setting::heavenPlateauRatio) / (1.0f - Setting::heavenPlateauRatio);
    edgeFactor = 1.0f - t * t * (3.0f - 2.0f * t); // Smoothstep rim falloff
  }
  slice.topY = baseY + T * 0.30f * edgeFactor;

  // Mountain Range / Peak System on Large & Giant Islands
  if (isMain && (s.isGiant || R >= 50.0f || s.hasPeak))
  {
    float coreFactor = std::pow(std::clamp(1.0f - slice.effectiveD / 0.70f, 0.0f, 1.0f), 1.5f);
    float mRidge = RidgeNoise::generate(worldX * Setting::islandMountainScale, worldZ * Setting::islandMountainScale, 4, 0.55f, 0.5f, Setting::seed + 1100);
    float mFbm   = FBMNoise::generate(worldX * 0.015f, worldZ * 0.015f, 3, 0.5f, 0.5f, Setting::seed + 1200);

    float baseMountainH = Setting::heavenAnchorPeakHeight + (s.isGiant ? 35.0f : 12.0f);
    float mountainH = coreFactor * baseMountainH * (0.50f + 0.50f * mRidge + 0.25f * mFbm);
    slice.topY += mountainH;

    slice.isAnchorPeak = (s.isAnchor && slice.effectiveD < 0.18f && coreFactor > 0.4f);
  }
  else
  {
    slice.isAnchorPeak = false;
  }

  // ─────────────────────────────────────────────────────────────────────────
  //  3. INVERTED CONE UNDERBELLY TAPERING ("kek cone terbalik tuh")
  //  Deep stalactite center tip sloping up steeply & smoothly to island rim.
  // ─────────────────────────────────────────────────────────────────────────
  float coneProfile = std::pow(std::clamp(1.0f - slice.effectiveD, 0.0f, 1.0f), Setting::heavenBellyExponent);
  float bellyNoise = RidgeNoise::generate(worldX * 0.038f, worldZ * 0.038f, 3, 0.5f, 0.05f, Setting::seed + 678) * 0.20f;
  
  float maxBellyDepth = T * 1.10f * (1.0f + bellyNoise); // Deep inverted cone center
  float bellyDepth = maxBellyDepth * coneProfile;
  
  slice.botY = slice.topY - std::max(3.0f, bellyDepth);

  return slice;
}

static inline float computeHeavenDistance(float worldX, float worldZ, const FeatureSeed &s)
{
  if (!s.exists)
    return 1e9f;

  float minDist = 1e9f;
  int totalIslands = 1 + s.subIsletCount;

  for (int i = 0; i < totalIslands; ++i)
  {
    IslandSlice slice = TerrainGenerator::evaluateIslandSlice(worldX, worldZ, s, i);
    minDist = std::min(minDist, slice.effectiveD);
  }

  return minDist;
}

FeatureSeed TerrainGenerator::findNearestHeavenSeed(float worldX, float worldZ, float &outDist)
{
  const float g = Setting::heavenClusterSpacing;
  int cellX = static_cast<int>(std::floor(worldX / g));
  int cellZ = static_cast<int>(std::floor(worldZ / g));

  FeatureSeed bestSeed{};
  float minDist = 1e9f;

  for (int dx = -1; dx <= 1; ++dx)
  {
    for (int dz = -1; dz <= 1; ++dz)
    {
      FeatureSeed s = generateHeavenSeed(cellX + dx, cellZ + dz);
      if (!s.exists)
        continue;

      float dist = computeHeavenDistance(worldX, worldZ, s);
      if (dist < minDist)
      {
        minDist = dist;
        bestSeed = s;
      }
    }
  }
  outDist = minDist;
  return bestSeed;
}

//  SPINE-BASED CANYON GENERATION (Tier 1 — Hell Realm)

float TerrainGenerator::getHellSpineX(float worldZ)
{
  float w1 = FBMNoise::generate(worldZ * Setting::hellSpineWarpScale1, 0.0f, 3, 0.5f, 0.5f, Setting::seed + 101) * 320.0f;
  float w2 = FBMNoise::generate(worldZ * Setting::hellSpineWarpScale2, 100.0f, 2, 0.5f, 0.5f, Setting::seed + 202) * 55.0f;
  return w1 + w2;
}

float TerrainGenerator::getHellCanyonDepthRatio(float worldX, float worldZ)
{
  float spineX = getHellSpineX(worldZ);
  float dist = std::abs(worldX - spineX);
  if (dist >= Setting::hellCanyonWidth)
    return 0.0f;
  float norm = dist / Setting::hellCanyonWidth;
  return 1.0f - (norm * norm * (3.0f - 2.0f * norm)); // smoothstep falloff
}

//  COLUMN CACHE PIPELINE

TerrainGenerator::ColumnGrid
TerrainGenerator::buildColumnCache(const Chunk &chunk)
{
  ColumnGrid grid{};

  for (int x = 0; x < Chunk::SIZE; ++x)
  {
    for (int z = 0; z < Chunk::SIZE; ++z)
    {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      ColumnCache &c = grid[x][z];
      c.terrain = TerrainSampler::sample(worldX, worldZ);

      const float raw = (c.terrain.plateau - Setting::plateauThreshold) /
                        (1.0f - Setting::plateauThreshold);
      c.pRaw = std::clamp(raw, 0.0f, 1.0f);
      c.pDepth = c.pRaw * c.pRaw * (3.0f - 2.0f * c.pRaw); // smoothstep

      c.floorH = computeBaseHeight(c.terrain);
      c.isIsland = (c.terrain.plateau >= Setting::plateauThreshold);

      c.canyonDepthRatio = getHellCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
      c.heavenSeed = findNearestHeavenSeed(static_cast<float>(worldX), static_cast<float>(worldZ), c.heavenDistance);
    }
  }

  return grid;
}

// ENTRY POINT

void TerrainGenerator::generate(Chunk &chunk)
{
  for (int x = 0; x < Chunk::SIZE; ++x)
    for (int z = 0; z < Chunk::SIZE; ++z)
    {
      chunk.heightMap[x][z] = 0;
      for (int y = 0; y < Chunk::HEIGHT; ++y)
        chunk.blocks[x][y][z] = BlockType::Air;
    }

  const ColumnGrid cache = buildColumnCache(chunk);

  generateBaseTerrain(chunk, cache);
  generateSurface(chunk, cache);
  generateCaves(chunk, cache);
}

//  STANDALONE SAMPLERS (LOD & Physics consistency across all 3 tiers)

int TerrainGenerator::sampleHellFloorAt(int worldX, int worldZ)
{
  const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
  const int floorH = computeBaseHeight(terrain);
  const float canyonRatio = getHellCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
  const int floorY = Setting::hellCanyonFloorY;

  int baseFloorY = floorH;
  if (canyonRatio > 0.0f)
  {
    baseFloorY = std::max(floorY, static_cast<int>(floorH - canyonRatio * (floorH - floorY)));
  }

  if (canyonRatio > 0.35f)
  {
    const float spikeNoise = RidgeNoise::generate(
        worldX * Setting::hellSpikeNoiseScale, worldZ * Setting::hellSpikeNoiseScale, 2, 0.5f, 0.05f, Setting::seed + 666);
    if (spikeNoise > 0.55f)
    {
      int spikeHeight = baseFloorY + static_cast<int>((spikeNoise - 0.55f) * 45.0f);
      spikeHeight = std::min(spikeHeight, Setting::hellCanyonRimY - 4);
      if (spikeHeight > baseFloorY)
        baseFloorY = spikeHeight;
    }
  }

  return baseFloorY;
}

int TerrainGenerator::sampleContinentHeightAt(int worldX, int worldZ)
{
  const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
  const float raw = (terrain.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
  const float pDepth = std::clamp(raw, 0.0f, 1.0f);
  const float smoothDepth = pDepth * pDepth * (3.0f - 2.0f * pDepth);

  if (smoothDepth < Setting::islandEdgeCutoff)
    return 0;

  const float variation = (FBMNoise::generate((worldX + 54321) * 0.0018f,
                                              (worldZ + 12345) * 0.0018f,
                                              3, 0.5f, 0.5f, Setting::seed + 225) + 1.0f) * 0.5f;
  const int flatHeight = std::clamp(100 + static_cast<int>(variation * 75.0f), 100, 220);
  int height = flatHeight;

  if (smoothDepth > 0.35f)
  {
    const float blend = (smoothDepth - 0.35f) / 0.65f;
    const float smoothBlend = blend * blend * (3.0f - 2.0f * blend);
    const float peakBlend = std::max(0.0f, (blend - Setting::mountainCoreThreshold) / (1.0f - Setting::mountainCoreThreshold));
    height += static_cast<int>(smoothBlend * 40.0f) + static_cast<int>(peakBlend * std::sqrt(std::max(0.0f, terrain.peaks)) * Setting::peakHeight);
  }
  else
  {
    height += static_cast<int>(terrain.erosion * 0.5f);
  }

  return std::clamp(applyErosion(height, terrain), flatHeight - 4, 250);
}

int TerrainGenerator::sampleContinentBodyBottomAt(int worldX, int worldZ)
{
  const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
  const float raw = (terrain.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
  const float pDepth = std::clamp(raw, 0.0f, 1.0f);
  const float smoothDepth = pDepth * pDepth * (3.0f - 2.0f * pDepth);

  const float variation = (FBMNoise::generate((worldX + 54321) * 0.0018f,
                                              (worldZ + 12345) * 0.0018f,
                                              3, 0.5f, 0.5f, Setting::seed + 225) + 1.0f) * 0.5f;
  const int flatPlateauH = std::clamp(100 + static_cast<int>(variation * 75.0f), 100, 220);
  int baseFloorY = sampleHellFloorAt(worldX, worldZ);
  return std::max(baseFloorY + 12, estimateBodyBottom(flatPlateauH, smoothDepth));
}

int TerrainGenerator::sampleHeightAt(int worldX, int worldZ)
{
  // Tier 3: Heaven floating islands
  float hDist = 0.0f;
  FeatureSeed hSeed = findNearestHeavenSeed(static_cast<float>(worldX), static_cast<float>(worldZ), hDist);
  if (hSeed.exists)
  {
    float maxHeavenTop = -1.0f;
    int totalIslands = 1 + hSeed.subIsletCount;
    for (int i = 0; i < totalIslands; ++i)
    {
      IslandSlice slice = evaluateIslandSlice(static_cast<float>(worldX), static_cast<float>(worldZ), hSeed, i);
      if (slice.valid && slice.topY > maxHeavenTop)
        maxHeavenTop = slice.topY;
    }
    if (maxHeavenTop > 0.0f)
      return std::clamp(static_cast<int>(maxHeavenTop), 280, Chunk::HEIGHT - 1);
  }

  // Tier 2: Normal terrain / Continent
  int continentH = sampleContinentHeightAt(worldX, worldZ);
  if (continentH > 0)
    return continentH;

  // Tier 1: Hell Floor & Canyon
  return sampleHellFloorAt(worldX, worldZ);
}

BlockType TerrainGenerator::sampleBlockAt(int worldX, int worldZ, int surfaceHeight)
{
  if (surfaceHeight >= 270)
  {
    float hDist = 0.0f;
    FeatureSeed s = findNearestHeavenSeed(static_cast<float>(worldX), static_cast<float>(worldZ), hDist);
    if (s.exists)
    {
      IslandSlice mainSlice = evaluateIslandSlice(static_cast<float>(worldX), static_cast<float>(worldZ), s, 0);
      if (mainSlice.valid && mainSlice.isAnchorPeak)
        return BlockType::Crystal;
    }
    return BlockType::Grass;
  }

  if (surfaceHeight <= Setting::hellCanyonRimY)
  {
    float canyonRatio = getHellCanyonDepthRatio(static_cast<float>(worldX), static_cast<float>(worldZ));
    float lavaRidge = RidgeNoise::generate(worldX * 0.04f, worldZ * 0.04f, 2, 0.5f, 0.04f, Setting::seed + 999);
    if (canyonRatio > 0.6f && lavaRidge > Setting::hellLavaThreshold)
      return BlockType::Lava;
    if (canyonRatio > 0.4f && lavaRidge > Setting::hellObsidianThreshold)
      return BlockType::Obsidian;
    if (canyonRatio > 0.3f)
      return BlockType::Basalt;
  }

  const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
  ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
  return BiomeManager::getBiome(terrain, climate)->getTopBlock();
}

bool TerrainGenerator::isSolidAt(int worldX, int worldZ, int y)
{
    // Tier 3 — Heaven floating islands
    float hDist = 0.0f;
    FeatureSeed hSeed = findNearestHeavenSeed(static_cast<float>(worldX), static_cast<float>(worldZ), hDist);
    if (hSeed.exists)
    {
        int totalIslands = 1 + hSeed.subIsletCount;
        for (int i = 0; i < totalIslands; ++i)
        {
            IslandSlice slice = evaluateIslandSlice(static_cast<float>(worldX), static_cast<float>(worldZ), hSeed, i);
            if (slice.valid && y >= slice.botY && y <= slice.topY)
                return true;
        }
    }

    const TerrainSample terrain = TerrainSampler::sample(worldX, worldZ);
    const int floorH = computeBaseHeight(terrain);

    const float raw = (terrain.plateau - Setting::plateauThreshold) / (1.0f - Setting::plateauThreshold);
    const float pClamped = std::clamp(raw, 0.0f, 1.0f);
    const float pDepth = pClamped * pClamped * (3.0f - 2.0f * pClamped);

    int baseFloorY = sampleHellFloorAt(worldX, worldZ);

    // Hell floor / normal ground column, all the way down
    if (y <= baseFloorY)
        return true;

    // Not inside plateau/island territory at all -> nothing above baseFloorY
    if (pDepth < Setting::islandEdgeCutoff)
        return false;

    // Tier 2 — plateau island body + hourglass stem
    const int bodyBottom = sampleContinentBodyBottomAt(worldX, worldZ);

    // Solid plateau body (this function doesn't need the exact top — the
    // caller already knows the surface height and won't query above it)
    if (y >= bodyBottom)
        return true;

    // Hourglass stem: hollow in places, following the same threshold as
    // generateBaseTerrain's stem-carving pass.
    const int stemTop = bodyBottom;
    const int stemBottom = baseFloorY + 1;
    const int stemHeight = stemTop - stemBottom;
    if (stemHeight <= 0)
        return false;

    const float tNorm = static_cast<float>(y - stemBottom) / static_cast<float>(stemHeight);
    const float midDist = std::abs(tNorm - 0.5f) * 2.0f;

    constexpr float waistRatio = 0.32f;
    const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);
    const float baseRequiredDepth = 1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

    const float rockDetail =
        FBMNoise::generate(worldX * 0.05f + y * 0.15f, worldZ * 0.05f + y * 0.15f, 2, 0.5f, 0.5f,
                           Setting::seed + 777) * 0.07f;
    const float verticalRidge =
        RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ + y * 2), 2, 0.5f, 0.03f,
                             Setting::seed + 888) * 0.04f;

    const float finalThreshold = baseRequiredDepth + rockDetail - verticalRidge;
    return pDepth >= finalThreshold;
}

// HEIGHT PIPELINE HELPERS

int TerrainGenerator::computeBaseHeight(const TerrainSample &t)
{
  int floorH = 14 + static_cast<int>(t.continentalness * 6.0f);
  if (t.river < Setting::riverThreshold)
  {
    const float rf = 1.0f - (t.river / Setting::riverThreshold);
    floorH -= static_cast<int>(rf * rf * 6.0f);
  }
  return std::clamp(floorH, 3, Chunk::HEIGHT - 1);
}

int TerrainGenerator::applyPlateauLift(int baseH, const TerrainSample &t)
{
  if (t.plateau < Setting::plateauThreshold)
    return baseH;
  return Setting::plateauBaseHeight + Setting::plateauHeight;
}

int TerrainGenerator::applyTerrace(int h, const TerrainSample &t)
{
  const float p = t.plateau;
  if (p < 0.25f || p > 0.82f)
    return h;

  const float window = (p - 0.25f) / (0.82f - 0.25f);
  float strength = 1.0f - std::abs(window * 2.0f - 1.0f);
  strength = std::pow(strength, 0.7f);

  const float erosionShift = (t.cliffMask - 0.5f) * Setting::cliffErosionStr;
  const int th = Setting::terraceHeight;
  const int hShifted = static_cast<int>(h + erosionShift);
  const int snapped = (hShifted / th) * th;

  return (strength * 0.85f > 0.5f) ? snapped : h;
}

int TerrainGenerator::applyMountainTop(int h, const TerrainSample &t)
{
  if (t.plateau < Setting::mountainCoreThreshold)
    return h;

  const float plateauDepth = (t.plateau - Setting::mountainCoreThreshold) /
                             (1.0f - Setting::mountainCoreThreshold);
  const float mountainProfile = std::pow(plateauDepth, 2.0f);
  const float peakFactor = std::pow(t.peaks, 3.0f);

  return h +
         static_cast<int>(mountainProfile * peakFactor * Setting::peakHeight);
}

int TerrainGenerator::applyErosion(int h, const TerrainSample &t)
{
  float erosionMult = 1.0f - (t.plateau * 0.8f);
  erosionMult = std::max(0.2f, erosionMult);
  return h -
         static_cast<int>(t.erosion * Setting::erosionStrength * erosionMult);
}

bool TerrainGenerator::shouldSpawnPillar(const TerrainSample &t)
{
  return (t.plateau >= Setting::plateauThreshold + 0.05f) &&
         (t.pillar >= 0.48f);
}

void TerrainGenerator::fillPillar(Chunk &chunk, int lx, int lz, int topH, int bottomH)
{
  bottomH = std::max(0, bottomH);
  topH = std::min(Chunk::HEIGHT - 1, topH);
  if (bottomH > topH)
    return;

  for (int y = bottomH; y <= topH; ++y)
    chunk.blocks[lx][y][lz] = BlockType::Stone;
}

int TerrainGenerator::estimateBodyBottom(int flatPlateauH, float pDepth)
{
  constexpr int maxThickness = 48;
  constexpr int minThickness = 2;

  const float thicknessFactor = std::pow(pDepth, 1.8f);
  const int bodyThickness =
      minThickness +
      static_cast<int>(thicknessFactor * (maxThickness - minThickness));
  return flatPlateauH - bodyThickness;
}

//  PASS 1 — BASE TERRAIN (3-Tier Realm: Hell Underworld / Normal / Heaven Clusters)

void TerrainGenerator::generateBaseTerrain(Chunk &chunk,
                                           const ColumnGrid &cache)
{
  for (int x = 0; x < Chunk::SIZE; ++x)
  {
    for (int z = 0; z < Chunk::SIZE; ++z)
    {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const TerrainSample &terrain = col.terrain;
      const int floorH = col.floorH;
      const float pDepth = col.pDepth;
      const float canyonRatio = col.canyonDepthRatio;

      // TIER 1 — HELL UNDERWORLD & CANYON (Y: 0 to Setting::hellCanyonRimY)
      const int floorY = Setting::hellCanyonFloorY;

      int baseFloorY = floorH;
      if (canyonRatio > 0.0f)
      {
        baseFloorY = std::max(floorY, static_cast<int>(floorH - canyonRatio * (floorH - floorY)));
      }

      for (int y = 0; y <= baseFloorY; ++y)
      {
        if (canyonRatio > 0.3f && y <= baseFloorY)
          chunk.blocks[x][y][z] = BlockType::Basalt;
        else
          chunk.blocks[x][y][z] = BlockType::Stone;
      }

      // Hell Spires & Jagged Crags rising from the Hell Floor
      if (canyonRatio > 0.35f)
      {
        const float spikeNoise = RidgeNoise::generate(
            worldX * Setting::hellSpikeNoiseScale, worldZ * Setting::hellSpikeNoiseScale, 2, 0.5f, 0.05f, Setting::seed + 666);
        if (spikeNoise > 0.55f)
        {
          int spikeHeight = baseFloorY + static_cast<int>((spikeNoise - 0.55f) * 45.0f);
          spikeHeight = std::min(spikeHeight, Setting::hellCanyonRimY - 4);
          for (int y = baseFloorY + 1; y <= spikeHeight; ++y)
          {
            chunk.blocks[x][y][z] = (spikeNoise > 0.70f) ? BlockType::Obsidian : BlockType::Basalt;
          }
          if (spikeHeight > baseFloorY)
            baseFloorY = spikeHeight;
        }
      }

      chunk.heightMap[x][z] = baseFloorY;

      // TIER 2 — NORMAL REALM & CONTINENT
      if (pDepth >= Setting::islandEdgeCutoff)
      {
        const float n1 = FBMNoise::generate((worldX + 54321) * 0.0018f,
                                            (worldZ + 12345) * 0.0018f, 3, 0.5f,
                                            0.5f, Setting::seed + 225);
        const float hVariation = (n1 + 1.0f) * 0.5f;
        const int flatPlateauH = std::clamp(
            100 + static_cast<int>(hVariation * 75.0f), 100, 220);

        int h = flatPlateauH;
        constexpr float blendStart = 0.35f;

        if (pDepth > blendStart)
        {
          const float tBlend = (pDepth - blendStart) / (1.0f - blendStart);
          const float tSmooth = tBlend * tBlend * (3.0f - 2.0f * tBlend);

          const float peakBlend =
              std::max(0.0f, (tBlend - Setting::mountainCoreThreshold) /
                                 (1.0f - Setting::mountainCoreThreshold));
          const float peakFactor = std::sqrt(std::max(0.0f, terrain.peaks));

          h += static_cast<int>(tSmooth * 40.0f) +
               static_cast<int>(peakBlend * peakFactor * Setting::peakHeight);
        }
        else
        {
          h += static_cast<int>(terrain.erosion * 0.5f);
        }

        h = applyErosion(h, terrain);
        h = std::clamp(h, flatPlateauH - 4, 250);
        chunk.heightMap[x][z] = h;

        const int bodyBottom =
            std::max(baseFloorY + 12, estimateBodyBottom(flatPlateauH, pDepth));

        if (bodyBottom < h)
        {
          for (int y = bodyBottom; y <= h; ++y)
            chunk.blocks[x][y][z] = BlockType::Stone;

          // Hourglass pillars support continent down to Hell floor
          const int stemTop = bodyBottom;
          const int stemBottom = baseFloorY + 1;
          const int stemHeight = stemTop - stemBottom;

          if (stemHeight > 0)
          {
            for (int y = stemBottom; y < stemTop; ++y)
            {
              const float tNorm = static_cast<float>(y - stemBottom) /
                                  static_cast<float>(stemHeight);
              const float midDist = std::abs(tNorm - 0.5f) * 2.0f;

              constexpr float waistRatio = 0.32f;
              const float shapeFactor = waistRatio + (1.0f - waistRatio) * (midDist * midDist);

              const float baseRequiredDepth =
                  1.0f - shapeFactor * (1.0f - Setting::islandEdgeCutoff);

              const float rockDetail =
                  FBMNoise::generate(worldX * 0.05f + y * 0.15f, worldZ * 0.05f + y * 0.15f, 2, 0.5f, 0.5f,
                                     Setting::seed + 777) *
                  0.07f;
              const float verticalRidge =
                  RidgeNoise::generate(static_cast<float>(worldX), static_cast<float>(worldZ + y * 2), 2, 0.5f, 0.03f,
                                       Setting::seed + 888) *
                  0.04f;

              const float finalThreshold = baseRequiredDepth + rockDetail - verticalRidge;

              if (pDepth >= finalThreshold)
              {
                chunk.blocks[x][y][z] = BlockType::Stone;
              }
            }
          }
        }
      }

      // TIER 3 — HEAVEN FLOATING ARCHIPELAGO CLUSTERS (Super High Y 320 to 480)
      if (col.heavenSeed.exists)
      {
        const FeatureSeed &s = col.heavenSeed;
        const int totalIslands = 1 + s.subIsletCount;
        for (int i = 0; i < totalIslands; ++i)
        {
          IslandSlice slice = evaluateIslandSlice(static_cast<float>(worldX), static_cast<float>(worldZ), s, i);
          if (slice.valid)
          {
            const int iBottom = std::clamp(static_cast<int>(slice.botY), 280, Chunk::HEIGHT - 2);
            const int iTop = std::clamp(static_cast<int>(slice.topY), iBottom, Chunk::HEIGHT - 1);

            for (int y = iBottom; y <= iTop; ++y)
            {
              chunk.blocks[x][y][z] = BlockType::Heavenstone;
            }

            if (iTop > chunk.heightMap[x][z])
            {
              chunk.heightMap[x][z] = iTop;
            }
          }
        }
      }
    }
  }
}

//  PASS 2 — SURFACE (Biome & Realm Aware Block Replacement)

void TerrainGenerator::generateSurface(Chunk &chunk, const ColumnGrid &cache)
{
  for (int x = 0; x < Chunk::SIZE; ++x)
  {
    for (int z = 0; z < Chunk::SIZE; ++z)
    {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const TerrainSample &terrain = col.terrain;

      ClimateSample climate = ClimateSampler::sample(worldX, worldZ);
      Biome *biome = BiomeManager::getBiome(terrain, climate);

      const float canyonRatio = col.canyonDepthRatio;
      int layerDepth = 0;

      for (int y = Chunk::HEIGHT - 1; y >= 0; --y)
      {
        BlockType bt = chunk.blocks[x][y][z];
        if (bt == BlockType::Air)
          continue;

        const bool airAbove = (y + 1 >= Chunk::HEIGHT) ||
                              (chunk.blocks[x][y + 1][z] == BlockType::Air);

        // Tier 3 — Heavenstone surface
        if (bt == BlockType::Heavenstone)
        {
          if (airAbove)
          {
            bool isCrystal = false;
            if (col.heavenSeed.exists)
            {
              IslandSlice slice0 = evaluateIslandSlice(static_cast<float>(worldX), static_cast<float>(worldZ), col.heavenSeed, 0);
              if (slice0.valid && slice0.isAnchorPeak && y > 350)
                isCrystal = true;
            }

            if (isCrystal)
              chunk.blocks[x][y][z] = BlockType::Crystal;
            else
              chunk.blocks[x][y][z] = BlockType::Grass;
            layerDepth = 3;
          }
          else if (layerDepth > 0)
          {
            chunk.blocks[x][y][z] = BlockType::Dirt;
            --layerDepth;
          }
        }
        // Tier 1 — Hell Canyon / Underworld surface
        else if (bt == BlockType::Basalt || bt == BlockType::Obsidian || (y <= Setting::hellCanyonFloorY + 2 && canyonRatio > 0.25f))
        {
          if (airAbove)
          {
            const float lavaRidge = RidgeNoise::generate(worldX * 0.04f, worldZ * 0.04f, 2, 0.5f, 0.04f, Setting::seed + 999);
            if (canyonRatio > 0.55f && lavaRidge > Setting::hellLavaThreshold)
              chunk.blocks[x][y][z] = BlockType::Lava;
            else if (canyonRatio > 0.35f && lavaRidge > Setting::hellObsidianThreshold)
              chunk.blocks[x][y][z] = BlockType::Obsidian;
            else if (canyonRatio > 0.25f)
              chunk.blocks[x][y][z] = (y % 2 == 0) ? BlockType::Ash : BlockType::Cinder;
            else
              chunk.blocks[x][y][z] = BlockType::Basalt;
          }
        }
        // Tier 2 — Normal surface
        else if (bt == BlockType::Stone)
        {
          if (airAbove)
          {
            chunk.blocks[x][y][z] = biome->getTopBlock();
            layerDepth = 3;
          }
          else if (layerDepth > 0)
          {
            chunk.blocks[x][y][z] = biome->getMiddleBlock();
            --layerDepth;
          }
        }
      }
    }
  }
}

//  PASS 3 — CAVES

void TerrainGenerator::generateCaves(Chunk &chunk, const ColumnGrid &cache)
{
  for (int x = 0; x < Chunk::SIZE; ++x)
  {
    for (int z = 0; z < Chunk::SIZE; ++z)
    {
      const int worldX = chunk.chunkX * Chunk::SIZE + x;
      const int worldZ = chunk.chunkZ * Chunk::SIZE + z;

      const ColumnCache &col = cache[x][z];
      const int surfaceH = chunk.heightMap[x][z];

      int caveMax = std::min(90, static_cast<int>(surfaceH * 0.70f));

      if (col.isIsland)
      {
        constexpr int conservativeFlatPlateauH = 100;
        const int estimatedBodyBottom =
            estimateBodyBottom(conservativeFlatPlateauH, col.pDepth);

        caveMax = std::min(caveMax, estimatedBodyBottom - 2);
      }

      if (caveMax <= 5)
        continue;

      for (int y = 5; y < caveMax; ++y)
      {
        const float cave = FBMNoise::generate(worldX + y * 2, worldZ + y * 2, 3,
                                              0.5f, 0.045f, Setting::seed);

        if (cave > 0.72f)
        {
          const bool hasNeighborStone =
              (y > 0 && chunk.blocks[x][y - 1][z] == BlockType::Stone) ||
              (y < Chunk::HEIGHT - 1 &&
               chunk.blocks[x][y + 1][z] == BlockType::Stone);

          if (hasNeighborStone)
            chunk.blocks[x][y][z] = BlockType::Air;
        }
      }
    }
  }
}