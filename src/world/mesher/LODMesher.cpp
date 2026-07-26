#include "LODMesher.h"
#include "../Chunk.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr int kCells = Chunk::SIZE;

    struct SurfaceCell
    {
        float height = -1.0f;
        BlockType type = BlockType::Air;
        bool valid = false;
    };

    void push(std::vector<float> &out, float x, float y, float z,
              float u, float v, float layer, float light)
    {
        out.insert(out.end(), {x, y, z, u, v, layer, light});
    }

    float layerFor(BlockType type, bool top)
    {
        if (type == BlockType::Grass) return top ? 0.0f : 1.0f;
        if (type == BlockType::Dirt)  return 2.0f;
        if (type == BlockType::Stone) return 3.0f;
        if (type == BlockType::Sand)  return 4.0f;
        return 0.0f;
    }

    void top(std::vector<float> &out, float x0, float z0, float x1, float z1,
             float y00, float y10, float y11, float y01, float layer)
    {
        // World-space UVs are intentional: the fragment shader repeats them and
        // therefore a merged/coarse face never stretches one texel over a tile.
        // Counter-clockwise when viewed from above (+Y).  Back-face culling is
        // enabled for terrain, so reversed winding makes every LOD top disappear.
        push(out, x0, y00, z0, z0, x0, layer, 1.0f);
        push(out, x0, y01, z1, z1, x0, layer, 1.0f);
        push(out, x1, y11, z1, z1, x1, layer, 1.0f);
        push(out, x0, y00, z0, z0, x0, layer, 1.0f);
        push(out, x1, y11, z1, z1, x1, layer, 1.0f);
        push(out, x1, y10, z0, z0, x1, layer, 1.0f);
    }

    void wallX(std::vector<float> &out, float x, float z0, float z1,
               float bottom, float topY, float layer, bool positive)
    {
        if (topY <= bottom) return;
        if (positive)
        {
            push(out, x, bottom, z0, z0, bottom, layer, .80f);
            push(out, x, topY, z0, z0, topY, layer, .80f);
            push(out, x, topY, z1, z1, topY, layer, .80f);
            push(out, x, bottom, z0, z0, bottom, layer, .80f);
            push(out, x, topY, z1, z1, topY, layer, .80f);
            push(out, x, bottom, z1, z1, bottom, layer, .80f);
        }
        else
        {
            push(out, x, bottom, z1, z1, bottom, layer, .80f);
            push(out, x, topY, z1, z1, topY, layer, .80f);
            push(out, x, topY, z0, z0, topY, layer, .80f);
            push(out, x, bottom, z1, z1, bottom, layer, .80f);
            push(out, x, topY, z0, z0, topY, layer, .80f);
            push(out, x, bottom, z0, z0, bottom, layer, .80f);
        }
    }

    void wallZ(std::vector<float> &out, float z, float x0, float x1,
               float bottom, float topY, float layer, bool positive)
    {
        if (topY <= bottom) return;
        if (positive)
        {
            push(out, x1, bottom, z, x1, bottom, layer, .70f);
            push(out, x1, topY, z, x1, topY, layer, .70f);
            push(out, x0, topY, z, x0, topY, layer, .70f);
            push(out, x1, bottom, z, x1, bottom, layer, .70f);
            push(out, x0, topY, z, x0, topY, layer, .70f);
            push(out, x0, bottom, z, x0, bottom, layer, .70f);
        }
        else
        {
            push(out, x0, bottom, z, x0, bottom, layer, .70f);
            push(out, x0, topY, z, x0, topY, layer, .70f);
            push(out, x1, topY, z, x1, topY, layer, .70f);
            push(out, x0, bottom, z, x0, bottom, layer, .70f);
            push(out, x1, topY, z, x1, topY, layer, .70f);
            push(out, x1, bottom, z, x1, bottom, layer, .70f);
        }
    }

    SurfaceCell sampleCell(int level, int x0, int z0, int step,
                           const LODMesher::BlockQuery &block, const LODMesher::HeightQuery &height)
    {
        const int sampleAxis = level == 1 ? 2 : 3;
        const int totalSamples = sampleAxis * sampleAxis;
        int heights = 0, count = 0;
        std::array<int, 5> votes{};
        for (int sz = 0; sz < sampleAxis; ++sz)
            for (int sx = 0; sx < sampleAxis; ++sx)
            {
                const int wx = x0 + ((2 * sx + 1) * step) / (2 * sampleAxis);
                const int wz = z0 + ((2 * sz + 1) * step) / (2 * sampleAxis);
                const int h = height(wx, wz);
                if (h < 0) continue;
                heights += h;
                ++count;
                const BlockType type = block(wx, h, wz);
                if (type == BlockType::Grass) ++votes[0];
                else if (type == BlockType::Dirt) ++votes[2];
                else if (type == BlockType::Stone) ++votes[3];
                else if (type == BlockType::Sand) ++votes[4];
            }

        if (count * 2 < totalSamples)
            return {};

        const int winner = int(std::max_element(votes.begin(), votes.end()) - votes.begin());
        const BlockType types[] = {BlockType::Grass, BlockType::Air, BlockType::Dirt,
                                   BlockType::Stone, BlockType::Sand};
        return {float(heights) / float(count), types[winner], true};
    }

    // Scan downward from just below `topY` looking for the first voxel that
    // is actually air. That point becomes the real wall bottom. This is what
    // lets a floating island's hollow stem produce a genuine gap instead of
    // a wall stretched all the way to the ground below it.
    //
    // sampleStep keeps this cheap: at low LOD levels the cell already covers
    // many real blocks, so we don't need to test every single Y.
    float findRealWallBottom(int wx, int wz, float topY, float floorLimit,
                             const LODMesher::SolidQuery &solid, int sampleStep)
    {
        if (!solid) return floorLimit; // no solidity info available -> old behavior
        float y = topY - 1.0f;
        float lastSolidY = topY;
        while (y > floorLimit)
        {
            if (!solid(wx, (int)y, wz))
                return y + 1.0f; // hit a gap -> stop the wall here
            lastSolidY = y;
            y -= (float)sampleStep;
        }
        return floorLimit;
    }

    void buildVoxelSurface(int level, int originX, int originZ, int step,
                           const LODMesher::BlockQuery &block, const LODMesher::HeightQuery &height,
                           const LODMesher::SolidQuery &solid,
                           std::vector<float> &out)
    {
        std::array<SurfaceCell, kCells * kCells> grid;
        for (int z = 0; z < kCells; ++z)
            for (int x = 0; x < kCells; ++x)
                grid[z * kCells + x] = sampleCell(level, originX + x * step, originZ + z * step,
                                                  step, block, height);

        // How coarsely to scan for the real wall bottom. Cheap: a handful of
        // samples per wall rather than checking every voxel.
        const int scanStep = std::max(1, step / 4);

        for (int z = 0; z < kCells; ++z)
            for (int x = 0; x < kCells; ++x)
            {
                const SurfaceCell &c = grid[z * kCells + x];
                if (!c.valid) continue;

                const float x0 = float(originX + x * step), x1 = x0 + step;
                const float z0 = float(originZ + z * step), z1 = z0 + step, y = c.height + 1.0f;
                top(out, x0, z0, x1, z1, y, y, y, y, layerFor(c.type, true));
                const float side = layerFor(c.type, false);

                // Neighbor height only decides HOW FAR a wall could plausibly
                // reach (out-of-tile neighbors still drop to 0 to stay seamless
                // with adjacent tiles). The ACTUAL bottom is then clipped by a
                // real solidity scan, so a hollow stem or overhang stops the
                // wall at the true gap instead of stretching to the neighbor.
                const auto neighborLimit = [&](int nx, int nz) -> float
                {
                    if (nx < 0 || nx >= kCells || nz < 0 || nz >= kCells)
                        return 0.0f;
                    const SurfaceCell &n = grid[nz * kCells + nx];
                    if (!n.valid) return y; // treat as no drop -> wall skip guard handles it
                    return n.height + 1.0f;
                };

                const int wxCenter = int(x0 + step * 0.5f);
                const int wzCenter = int(z0 + step * 0.5f);

                const float limNX = neighborLimit(x - 1, z);
                const float limPX = neighborLimit(x + 1, z);
                const float limNZ = neighborLimit(x, z - 1);
                const float limPZ = neighborLimit(x, z + 1);

                const float bottomNX = findRealWallBottom(wxCenter, wzCenter, y, limNX, solid, scanStep);
                const float bottomPX = findRealWallBottom(wxCenter, wzCenter, y, limPX, solid, scanStep);
                const float bottomNZ = findRealWallBottom(wxCenter, wzCenter, y, limNZ, solid, scanStep);
                const float bottomPZ = findRealWallBottom(wxCenter, wzCenter, y, limPZ, solid, scanStep);

                wallX(out, x0, z0, z1, bottomNX, y, side, false);
                wallX(out, x1, z0, z1, bottomPX, y, side, true);
                wallZ(out, z0, x0, x1, bottomNZ, y, side, false);
                wallZ(out, z1, x0, x1, bottomPZ, y, side, true);
            }
    }

void buildHeightmap(int level, int originX, int originZ, int step,
                        const LODMesher::BlockQuery &block, const LODMesher::HeightQuery &height,
                        std::vector<float> &out)
    {
        // LOD3-5 use a simplified fast-path height formula that doesn't match
        // isSolidAt's reconstruction (which mirrors the LOD1/2 formula), so
        // solidity scanning here would misfire and erase walls everywhere.
        // Fall back to the neighbor-clamp approach for these far levels.
        const int cells = level == 3 ? kCells : 8;
        const int cellStep = (step * kCells) / cells;
        const int points = cells + 1;
        std::vector<float> h(points * points);
        std::vector<BlockType> type(points * points);

        for (int z = 0; z < points; ++z)
            for (int x = 0; x < points; ++x)
            {
                const int i = z * points + x, wx = originX + x * cellStep, wz = originZ + z * cellStep;
                h[i] = float(std::max(0, height(wx, wz)) + 1);
                type[i] = block(wx, int(h[i] - 1), wz);
            }

        const float cliff = std::max(float(Setting::terraceHeight), float(cellStep) * .35f);
        constexpr float kMaxWallDrop = 6.0f;

        for (int z = 0; z < cells; ++z)
            for (int x = 0; x < cells; ++x)
            {
                const int i = z * points + x;
                const float x0 = originX + x * cellStep, z0 = originZ + z * cellStep,
                            x1 = x0 + cellStep, z1 = z0 + cellStep;
                top(out, x0, z0, x1, z1, h[i], h[i + 1], h[i + points + 1], h[i + points], layerFor(type[i], true));

                const float side = layerFor(type[i], false);
                if (x != 0 && std::abs(h[i] - h[i - 1]) > cliff)
                    wallX(out, x0, z0, z1, std::max(h[i - 1], h[i] - kMaxWallDrop), h[i], side, false);
                if (x != cells - 1 && std::abs(h[i + 1] - h[i + 2]) > cliff)
                    wallX(out, x1, z0, z1, std::max(h[i + 2], h[i + 1] - kMaxWallDrop), h[i + 1], side, true);
                if (z != 0 && std::abs(h[i] - h[i - points]) > cliff)
                    wallZ(out, z0, x0, x1, std::max(h[i - points], h[i] - kMaxWallDrop), h[i], side, false);
                if (z != cells - 1 && std::abs(h[i + points] - h[i + 2 * points]) > cliff)
                    wallZ(out, z1, x0, x1, std::max(h[i + 2 * points], h[i + points] - kMaxWallDrop), h[i + points], side, true);
            }
    }
} // namespace

void LODMesher::build(int level, int tileX, int tileZ, const BlockQuery &blockQuery,
                      const HeightQuery &heightQuery, const SolidQuery &solidQuery,
                      std::vector<float> &outVertices)
{
    outVertices.clear();
    const int step = 1 << level;
    const int coverage = step * Chunk::SIZE;
    const int originX = tileX * coverage, originZ = tileZ * coverage;
    if (level <= 2)
        buildVoxelSurface(level, originX, originZ, step, blockQuery, heightQuery, solidQuery, outVertices);
    else
        buildHeightmap(level, originX, originZ, step, blockQuery, heightQuery, outVertices); 
}