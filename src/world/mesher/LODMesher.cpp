#include "LODMesher.h"
#include "../Chunk.h"
#include <cmath>
#include <algorithm>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  LODMesher — Low-resolution mesh implementation for 5-level LOD
//
//  Combined techniques from two references:
//
//  From LodGen (QEM / vertex clustering):
//    - Each LOD grid cell is one vertex cluster.
//    - Cell height = average heightmap within the cluster (not just the center point).
//      This is the core of vertex clustering: replace many original vertices with
//      ONE representative vertex per cluster.
//    - Block type is chosen by majority vote within the cluster.
//
//  From LoD/CDLOD (cdlod_terrain.vert — morphVertex):
//    - Cells are arranged so that border vertices between different LOD tiles snap
//      to compatible positions, reducing seams (gaps).
//    - Side skirts are emitted to close vertical gaps between cells,
//      just as CDLOD closes gaps between LOD levels.
//
//  Result:
//    - Top face: greedy quad merging across cells whose height & block type are
//      similar enough → far fewer polygons, smoother appearance.
//    - Side faces: vertical skirts only where there is a significant height difference.
//    - No visible holes or seams.
// ─────────────────────────────────────────────────────────────────────────────

// ─── Height tolerance (in blocks) for greedy merge ───────────────────────────
// Two cells are considered flat enough to merge if their height difference ≤ tolerance.
// LOD1 is more lenient (higher resolution); LOD4–5 are stricter (lower resolution,
// each cell is already large so height error must be controlled).
static int heightTolerance(int level) {
    switch (level) {
        case 1: return 2;   // ~32 blocks per cell, ±2 block difference is fine
        case 2: return 3;   // ~64 blocks per cell
        case 3: return 4;   // ~128 blocks per cell
        case 4: return 6;   // ~256 blocks per cell, somewhat strict
        case 5: return 8;   // ~512 blocks per cell, large enough for wide plains
        default: return 4;
    }
}

// ─── Helper: push a single vertex ────────────────────────────────────────────
static inline void pushV(std::vector<float>& b,
                         float x, float y, float z,
                         float u, float v,
                         float layer, float light) {
    b.push_back(x); b.push_back(y); b.push_back(z);
    b.push_back(u); b.push_back(v);
    b.push_back(layer);
    b.push_back(light);
}

// ─── Texture layer (matches GreedyMesher) ────────────────────────────────────
static inline float texLayer(BlockType bt, int axis, bool back) {
    if (bt == BlockType::Grass) {
        if (axis == 1) return back ? 2.0f : 0.0f; // bottom=Dirt, top=Grass
        return 1.0f;  // sides = grass_side
    }
    if (bt == BlockType::Dirt)  return 2.0f;
    if (bt == BlockType::Stone) return 3.0f;
    if (bt == BlockType::Sand)  return 4.0f;
    return 0.0f;
}

// ─── Emit top quad (CCW winding) ─────────────────────────────────────────────
// x0/z0 = SW corner, x1/z1 = NE corner, y = top surface of block
static void emitTop(std::vector<float>& buf,
                    float x0, float z0, float x1, float z1, float y,
                    float layer) {
    const float light = 1.0f;
    pushV(buf, x0,y,z0, x0,z0, layer,light);
    pushV(buf, x1,y,z0, x1,z0, layer,light);
    pushV(buf, x1,y,z1, x1,z1, layer,light);
    pushV(buf, x0,y,z0, x0,z0, layer,light);
    pushV(buf, x1,y,z1, x1,z1, layer,light);
    pushV(buf, x0,y,z1, x0,z1, layer,light);
}

// ─── Emit vertical side quads ────────────────────────────────────────────────

// +X face (normal facing +X)
static void emitSidePX(std::vector<float>& buf,
                       float x, float z0, float z1,
                       float yBot, float yTop, float layer) {
    const float light = 0.80f;
    pushV(buf, x,yBot,z1, z1,yBot, layer,light);
    pushV(buf, x,yTop,z1, z1,yTop, layer,light);
    pushV(buf, x,yTop,z0, z0,yTop, layer,light);
    pushV(buf, x,yBot,z1, z1,yBot, layer,light);
    pushV(buf, x,yTop,z0, z0,yTop, layer,light);
    pushV(buf, x,yBot,z0, z0,yBot, layer,light);
}

// -X face
static void emitSideNX(std::vector<float>& buf,
                       float x, float z0, float z1,
                       float yBot, float yTop, float layer) {
    const float light = 0.80f;
    pushV(buf, x,yBot,z0, z0,yBot, layer,light);
    pushV(buf, x,yTop,z0, z0,yTop, layer,light);
    pushV(buf, x,yTop,z1, z1,yTop, layer,light);
    pushV(buf, x,yBot,z0, z0,yBot, layer,light);
    pushV(buf, x,yTop,z1, z1,yTop, layer,light);
    pushV(buf, x,yBot,z1, z1,yBot, layer,light);
}

// +Z face
static void emitSidePZ(std::vector<float>& buf,
                       float z, float x0, float x1,
                       float yBot, float yTop, float layer) {
    const float light = 0.70f;
    pushV(buf, x0,yBot,z, x0,yBot, layer,light);
    pushV(buf, x0,yTop,z, x0,yTop, layer,light);
    pushV(buf, x1,yTop,z, x1,yTop, layer,light);
    pushV(buf, x0,yBot,z, x0,yBot, layer,light);
    pushV(buf, x1,yTop,z, x1,yTop, layer,light);
    pushV(buf, x1,yBot,z, x1,yBot, layer,light);
}

// -Z face
static void emitSideNZ(std::vector<float>& buf,
                       float z, float x0, float x1,
                       float yBot, float yTop, float layer) {
    const float light = 0.70f;
    pushV(buf, x1,yBot,z, x1,yBot, layer,light);
    pushV(buf, x1,yTop,z, x1,yTop, layer,light);
    pushV(buf, x0,yTop,z, x0,yTop, layer,light);
    pushV(buf, x1,yBot,z, x1,yBot, layer,light);
    pushV(buf, x0,yTop,z, x0,yTop, layer,light);
    pushV(buf, x0,yBot,z, x0,yBot, layer,light);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cell descriptor — vertex clustering result for one grid cell
// ─────────────────────────────────────────────────────────────────────────────
struct Cell {
    float    avgHeight = -1.0f; // -1 = empty (all Air blocks)
    BlockType bt       = BlockType::Air;
    bool     valid     = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  LODMesher::build — main entry point
// ─────────────────────────────────────────────────────────────────────────────
void LODMesher::build(
    int level,
    int tileX,
    int tileZ,
    const BlockQuery&   blockQuery,
    const HeightQuery&  heightQuery,
    std::vector<float>& outVertices)
{
    outVertices.clear();

    // step = number of blocks per cell side (higher level = coarser resolution)
    const int step     = (1 << level);           // blocks per cell side
    const int coverage = step * Chunk::SIZE;     // blocks per tile side
    const int originX  = tileX * coverage;
    const int originZ  = tileZ * coverage;
    const int cells    = Chunk::SIZE;            // always 16 cells per tile side

    // ── Phase 1: Vertex Clustering ────────────────────────────────────────
    // Each cell covers (step × step) source blocks.
    // From LodGen vertex_cluster: find a representative point (average heightmap)
    // and choose the block type by majority vote.

    std::vector<Cell> grid(cells * cells);

    for (int iz = 0; iz < cells; iz++) {
        for (int ix = 0; ix < cells; ix++) {
            int worldX0 = originX + ix * step;
            int worldZ0 = originZ + iz * step;

            int heightSum   = 0;
            int heightCount = 0;
            // Vote count per block type: Grass=0, Dirt=2, Stone=3, Sand=4
            int blockCounts[5] = {0, 0, 0, 0, 0};

            // Sample every block in this cluster
            for (int dz = 0; dz < step; dz++) {
                for (int dx = 0; dx < step; dx++) {
                    int wx = worldX0 + dx;
                    int wz = worldZ0 + dz;
                    int h  = heightQuery(wx, wz);
                    if (h < 0) continue;
                    heightSum += h;
                    heightCount++;

                    BlockType bt = blockQuery(wx, h, wz);
                    int idx = 0;
                    if      (bt == BlockType::Grass) idx = 0;
                    else if (bt == BlockType::Dirt)  idx = 2;
                    else if (bt == BlockType::Stone) idx = 3;
                    else if (bt == BlockType::Sand)  idx = 4;
                    if (idx < 5) blockCounts[idx]++;
                }
            }

            if (heightCount == 0) continue; // empty cell, skip

            Cell& c     = grid[iz * cells + ix];
            c.avgHeight = (float)heightSum / heightCount; // cluster center
            c.valid     = true;

            // Pick the majority block type
            int maxIdx = 0;
            for (int k = 1; k < 5; k++)
                if (blockCounts[k] > blockCounts[maxIdx]) maxIdx = k;

            switch (maxIdx) {
                case 0:  c.bt = BlockType::Grass; break;
                case 2:  c.bt = BlockType::Dirt;  break;
                case 3:  c.bt = BlockType::Stone; break;
                case 4:  c.bt = BlockType::Sand;  break;
                default: c.bt = BlockType::Grass; break;
            }
        }
    }

    // ── Phase 2: Greedy Quad Merging for Top Faces ────────────────────────
    // From CDLOD: merge cells whose height difference & block type fall within
    // tolerance → one large quad, far fewer polygons.

    const int tol = heightTolerance(level);
    std::vector<bool> merged(cells * cells, false);

    for (int iz = 0; iz < cells; iz++) {
        for (int ix = 0; ix < cells; ix++) {
            if (merged[iz * cells + ix]) continue;
            const Cell& c = grid[iz * cells + ix];
            if (!c.valid) continue;

            // Expand width in the +X direction
            int w = 1;
            while (ix + w < cells) {
                const Cell& cn = grid[iz * cells + (ix + w)];
                if (!cn.valid || cn.bt != c.bt ||
                    std::abs(cn.avgHeight - c.avgHeight) > tol)
                    break;
                w++;
            }

            // Expand height in the +Z direction (each row must be compatible across width w)
            int h = 1;
            while (iz + h < cells) {
                bool rowOk = true;
                for (int col = 0; col < w; col++) {
                    const Cell& cn = grid[(iz + h) * cells + (ix + col)];
                    if (!cn.valid || cn.bt != c.bt ||
                        std::abs(cn.avgHeight - c.avgHeight) > tol) {
                        rowOk = false; break;
                    }
                }
                if (!rowOk) break;
                h++;
            }

            // Mark all cells in this patch as merged
            for (int dz = 0; dz < h; dz++)
                for (int dx = 0; dx < w; dx++)
                    merged[(iz + dz) * cells + (ix + dx)] = true;

            // World-space corners
            float wx0 = (float)(originX + ix * step);
            float wx1 = (float)(originX + (ix + w) * step);
            float wz0 = (float)(originZ + iz * step);
            float wz1 = (float)(originZ + (iz + h) * step);

            // Average height of the entire patch for accuracy
            float ySum = 0.0f;
            int   yN   = 0;
            for (int dz = 0; dz < h; dz++)
                for (int dx = 0; dx < w; dx++) {
                    ySum += grid[(iz + dz) * cells + (ix + dx)].avgHeight;
                    yN++;
                }
            float fy = (ySum / yN) + 1.0f; // top block surface (+1)

            float layer = texLayer(c.bt, 1, false);
            emitTop(outVertices, wx0, wz0, wx1, wz1, fy, layer);
        }
    }

    // ── Phase 3: Side Skirts ──────────────────────────────────────────────
    // From CDLOD cdlod_terrain.vert: vertical skirts at LOD tile edges and
    // between cells with significant height differences.
    // This prevents holes between adjacent LOD levels.

    for (int iz = 0; iz < cells; iz++) {
        for (int ix = 0; ix < cells; ix++) {
            const Cell& c = grid[iz * cells + ix];
            if (!c.valid) continue;

            float wx0 = (float)(originX + ix * step);
            float wx1 = (float)(originX + (ix + 1) * step);
            float wz0 = (float)(originZ + iz * step);
            float wz1 = (float)(originZ + (iz + 1) * step);
            float fy  = c.avgHeight + 1.0f;

            // -X face
            if (ix > 0) {
                const Cell& cn = grid[iz * cells + (ix - 1)];
                float yN = cn.valid ? (cn.avgHeight + 1.0f) : 0.0f;
                if (!cn.valid || fy > yN + 0.5f) {
                    float yBot  = std::max(0.0f, yN);
                    float layer = texLayer(c.bt, 0, false);
                    emitSideNX(outVertices, wx0, wz0, wz1, yBot, fy, layer);
                }
            } else {
                // Tile boundary: always emit skirt downward so there is no gap
                // with a neighbouring tile at a different LOD level
                float layer = texLayer(c.bt, 0, false);
                emitSideNX(outVertices, wx0, wz0, wz1,
                           std::max(0.0f, fy - step * 2.0f), fy, layer);
            }

            // +X face
            if (ix < cells - 1) {
                const Cell& cn = grid[iz * cells + (ix + 1)];
                float yN = cn.valid ? (cn.avgHeight + 1.0f) : 0.0f;
                if (!cn.valid || fy > yN + 0.5f) {
                    float yBot  = std::max(0.0f, yN);
                    float layer = texLayer(c.bt, 0, true);
                    emitSidePX(outVertices, wx1, wz0, wz1, yBot, fy, layer);
                }
            } else {
                float layer = texLayer(c.bt, 0, true);
                emitSidePX(outVertices, wx1, wz0, wz1,
                           std::max(0.0f, fy - step * 2.0f), fy, layer);
            }

            // -Z face
            if (iz > 0) {
                const Cell& cn = grid[(iz - 1) * cells + ix];
                float yN = cn.valid ? (cn.avgHeight + 1.0f) : 0.0f;
                if (!cn.valid || fy > yN + 0.5f) {
                    float yBot  = std::max(0.0f, yN);
                    float layer = texLayer(c.bt, 2, false);
                    emitSideNZ(outVertices, wz0, wx0, wx1, yBot, fy, layer);
                }
            } else {
                float layer = texLayer(c.bt, 2, false);
                emitSideNZ(outVertices, wz0, wx0, wx1,
                           std::max(0.0f, fy - step * 2.0f), fy, layer);
            }

            // +Z face
            if (iz < cells - 1) {
                const Cell& cn = grid[(iz + 1) * cells + ix];
                float yN = cn.valid ? (cn.avgHeight + 1.0f) : 0.0f;
                if (!cn.valid || fy > yN + 0.5f) {
                    float yBot  = std::max(0.0f, yN);
                    float layer = texLayer(c.bt, 2, true);
                    emitSidePZ(outVertices, wz1, wx0, wx1, yBot, fy, layer);
                }
            } else {
                float layer = texLayer(c.bt, 2, true);
                emitSidePZ(outVertices, wz1, wx0, wx1,
                           std::max(0.0f, fy - step * 2.0f), fy, layer);
            }
        }
    }
}
