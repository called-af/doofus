#include "LODMesher.h"
#include "../Chunk.h"
#include "../../core/Setting.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kCells = Chunk::SIZE;

struct SurfaceCell {
    float height = -1.0f;
    BlockType type = BlockType::Air;
    bool valid = false;
};

void push(std::vector<float>& out, float x, float y, float z,
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

void top(std::vector<float>& out, float x0, float z0, float x1, float z1,
         float y00, float y10, float y11, float y01, float layer)
{
    // World-space UVs are intentional: the fragment shader repeats them and
    // therefore a merged/coarse face never stretches one texel over a tile.
    // Counter-clockwise when viewed from above (+Y).  Back-face culling is
    // enabled for terrain, so reversed winding makes every LOD top disappear.
    push(out,x0,y00,z0,z0,x0,layer,1.0f); push(out,x0,y01,z1,z1,x0,layer,1.0f);
    push(out,x1,y11,z1,z1,x1,layer,1.0f); push(out,x0,y00,z0,z0,x0,layer,1.0f);
    push(out,x1,y11,z1,z1,x1,layer,1.0f); push(out,x1,y10,z0,z0,x1,layer,1.0f);
}

void wallX(std::vector<float>& out, float x, float z0, float z1,
           float bottom, float topY, float layer, bool positive)
{
    if (topY <= bottom) return;
    if (positive) {
        push(out,x,bottom,z0,z0,bottom,layer,.80f); push(out,x,topY,z0,z0,topY,layer,.80f);
        push(out,x,topY,z1,z1,topY,layer,.80f);     push(out,x,bottom,z0,z0,bottom,layer,.80f);
        push(out,x,topY,z1,z1,topY,layer,.80f);     push(out,x,bottom,z1,z1,bottom,layer,.80f);
    } else {
        push(out,x,bottom,z1,z1,bottom,layer,.80f); push(out,x,topY,z1,z1,topY,layer,.80f);
        push(out,x,topY,z0,z0,topY,layer,.80f);     push(out,x,bottom,z1,z1,bottom,layer,.80f);
        push(out,x,topY,z0,z0,topY,layer,.80f);     push(out,x,bottom,z0,z0,bottom,layer,.80f);
    }
}

void wallZ(std::vector<float>& out, float z, float x0, float x1,
           float bottom, float topY, float layer, bool positive)
{
    if (topY <= bottom) return;
    if (positive) {
        push(out,x1,bottom,z,x1,bottom,layer,.70f); push(out,x1,topY,z,x1,topY,layer,.70f);
        push(out,x0,topY,z,x0,topY,layer,.70f);     push(out,x1,bottom,z,x1,bottom,layer,.70f);
        push(out,x0,topY,z,x0,topY,layer,.70f);     push(out,x0,bottom,z,x0,bottom,layer,.70f);
    } else {
        push(out,x0,bottom,z,x0,bottom,layer,.70f); push(out,x0,topY,z,x0,topY,layer,.70f);
        push(out,x1,topY,z,x1,topY,layer,.70f);     push(out,x0,bottom,z,x0,bottom,layer,.70f);
        push(out,x1,topY,z,x1,topY,layer,.70f);     push(out,x1,bottom,z,x1,bottom,layer,.70f);
    }
}

SurfaceCell sampleCell(int level, int x0, int z0, int step,
                       const LODMesher::BlockQuery& block, const LODMesher::HeightQuery& height)
{
    // Four samples on LOD1 and an 8x8 area vote on LOD2.  This low-pass vote
    // is specifically for floating islands; a single sparse noise sample was
    // the source of the random missing-texture/geometry patches.
    const int sampleAxis = level == 1 ? 2 : 8;
    int heights = 0, count = 0;
    std::array<int, 5> votes{};
    for (int sz = 0; sz < sampleAxis; ++sz) for (int sx = 0; sx < sampleAxis; ++sx) {
        const int wx = x0 + ((2 * sx + 1) * step) / (2 * sampleAxis);
        const int wz = z0 + ((2 * sz + 1) * step) / (2 * sampleAxis);
        const int h = height(wx, wz);
        if (h < 0) continue;
        heights += h; ++count;
        const BlockType type = block(wx, h, wz);
        if (type == BlockType::Grass) ++votes[0];
        else if (type == BlockType::Dirt) ++votes[2];
        else if (type == BlockType::Stone) ++votes[3];
        else if (type == BlockType::Sand) ++votes[4];
    }
    if (!count) return {};
    const int winner = int(std::max_element(votes.begin(), votes.end()) - votes.begin());
    const BlockType types[] = {BlockType::Grass, BlockType::Air, BlockType::Dirt,
                               BlockType::Stone, BlockType::Sand};
    return {float(heights) / float(count), types[winner], true};
}

void buildVoxelSurface(int level, int originX, int originZ, int step,
                       const LODMesher::BlockQuery& block, const LODMesher::HeightQuery& height,
                       std::vector<float>& out)
{
    std::array<SurfaceCell, kCells * kCells> grid;
    for (int z = 0; z < kCells; ++z) for (int x = 0; x < kCells; ++x)
        grid[z * kCells + x] = sampleCell(level, originX + x * step, originZ + z * step,
                                           step, block, height);
    for (int z = 0; z < kCells; ++z) for (int x = 0; x < kCells; ++x) {
        const SurfaceCell& c = grid[z * kCells + x]; if (!c.valid) continue;
        const float x0 = float(originX + x * step), x1 = x0 + step;
        const float z0 = float(originZ + z * step), z1 = z0 + step, y = c.height + 1.0f;
        top(out, x0,z0,x1,z1,y,y,y,y,layerFor(c.type,true));
        const float side = layerFor(c.type,false);
        const auto neighbor = [&](int nx, int nz) { return nx >= 0 && nx < kCells && nz >= 0 && nz < kCells ? grid[nz*kCells+nx].height+1.0f : 0.0f; };
        wallX(out,x0,z0,z1,neighbor(x-1,z),y,side,false); wallX(out,x1,z0,z1,neighbor(x+1,z),y,side,true);
        wallZ(out,z0,x0,x1,neighbor(x,z-1),y,side,false); wallZ(out,z1,x0,x1,neighbor(x,z+1),y,side,true);
    }
}

void buildHeightmap(int level, int originX, int originZ, int step,
                    const LODMesher::BlockQuery& block, const LODMesher::HeightQuery& height,
                    std::vector<float>& out)
{
    constexpr int points = kCells + 1;
    std::array<float, points * points> h{};
    std::array<BlockType, points * points> type{};
    for (int z=0; z<points; ++z) for (int x=0; x<points; ++x) {
        const int i=z*points+x, wx=originX+x*step, wz=originZ+z*step;
        h[i]=float(std::max(0,height(wx,wz))+1); type[i]=block(wx,int(h[i]-1),wz);
    }
    const float cliff = std::max(float(Setting::terraceHeight), float(step) * .35f);
    for (int z=0; z<kCells; ++z) for (int x=0; x<kCells; ++x) {
        const int i=z*points+x; const float x0=originX+x*step, z0=originZ+z*step, x1=x0+step, z1=z0+step;
        top(out,x0,z0,x1,z1,h[i],h[i+1],h[i+points+1],h[i+points],layerFor(type[i],true));
        // LOD3 intentionally has no skirts: it is the transition heightmap
        // ring, where the cheaper gently interpolated silhouette is preferred.
        if (level == 3) continue;

        // LOD4-5 retain skirts across real cliffs and on their outer edges.
        const float side=layerFor(type[i],false);
        if (x==0 || std::abs(h[i]-h[i-1])>cliff) wallX(out,x0,z0,z1,x==0 ? std::max(0.0f,h[i]-step*2.0f) : h[i-1],h[i],side,false);
        if (x==kCells-1 || std::abs(h[i+1]-h[i+2])>cliff) wallX(out,x1,z0,z1,x==kCells-1 ? std::max(0.0f,h[i+1]-step*2.0f) : h[i+2],h[i+1],side,true);
        if (z==0 || std::abs(h[i]-h[i-points])>cliff) wallZ(out,z0,x0,x1,z==0 ? std::max(0.0f,h[i]-step*2.0f) : h[i-points],h[i],side,false);
        if (z==kCells-1 || std::abs(h[i+points]-h[i+2*points])>cliff) wallZ(out,z1,x0,x1,z==kCells-1 ? std::max(0.0f,h[i+points]-step*2.0f) : h[i+2*points],h[i+points],side,true);
    }
}
} // namespace

void LODMesher::build(int level, int tileX, int tileZ, const BlockQuery& blockQuery,
                      const HeightQuery& heightQuery, std::vector<float>& outVertices)
{
    outVertices.clear();
    const int step = 1 << level;
    const int coverage = step * Chunk::SIZE;
    const int originX = tileX * coverage, originZ = tileZ * coverage;
    if (level <= 2) buildVoxelSurface(level,originX,originZ,step,blockQuery,heightQuery,outVertices);
    else buildHeightmap(level,originX,originZ,step,blockQuery,heightQuery,outVertices);
}
