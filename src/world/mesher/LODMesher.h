#pragma once

#include <vector>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  LODMesher
//
//  Generates a low-resolution mesh for a single LOD tile.
//
//  Combined techniques:
//
//  From LodGen (QEM / vertex clustering):
//    - Each LOD grid cell = one vertex cluster.
//    - Cell height is computed from the average heightmap within that cluster.
//    - Block type is chosen by majority vote within the cluster.
//
//  From CDLOD (cdlod_terrain.vert — morphVertex):
//    - Border vertices between different LOD tiles snap to compatible positions
//      so that no visible seams appear.
//    - "Side skirts" (vertical walls) are emitted to close gaps between LOD levels.
//
//  Build process (3 phases):
//    1. Vertex Clustering — average height + majority block vote per cell
//    2. Greedy Quad Merging — merge similar cells → fewer polygons
//    3. Side Skirts — vertical walls closing tile boundaries & height differences
//
//  Usage:
//    LODMesher::build(
//        level,        // 1–5
//        tileX, tileZ, // tile coordinates
//        blockQuery,   // function (wx, wy, wz) -> BlockType
//        heightQuery,  // function (wx, wz)     -> int (highest solid Y)
//        outVertices   // output vertex buffer
//    );
// ─────────────────────────────────────────────────────────────────────────────

#include "../block/BlockType.h"

class LODMesher {
public:
    // blockQuery(worldX, worldY, worldZ) -> BlockType
    using BlockQuery  = std::function<BlockType(int, int, int)>;
    // heightQuery(worldX, worldZ) -> highest solid Y (-1 if empty)
    using HeightQuery = std::function<int(int, int)>;

    // Build the LOD mesh for a single tile.
    // outVertices is filled with vertex data (7 floats per vertex: x,y,z,u,v,layer,light).
    static void build(
        int level,
        int tileX,
        int tileZ,
        const BlockQuery&   blockQuery,
        const HeightQuery&  heightQuery,
        std::vector<float>& outVertices
    );
};
