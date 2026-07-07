#pragma once

#include <vector>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
//  LODMesher
//
//  Generates low-resolution meshes for a single LOD tile.
//
//  Inspired by CDLOD (LoD/engine/cdlod/):
//  - Vertex grid is sampled every `step` blocks (step = 2^level)
//  - Only the top face is emitted for efficiency since far chunk sides
//    are not visible from a normal player viewpoint
//  - Vertical side faces are still emitted so slopes are visible at long range
//
//  Build:
//    LODMesher::build(
//        level,           // 1, 2, or 3
//        tileX, tileZ,    // tile coordinate
//        blockQuery,      // function (wx, wy, wz) -> BlockType
//        heightQuery,     // function (wx, wz)     -> int (highest solid Y)
//        outVertices
//    );
// ─────────────────────────────────────────────────────────────────────────────

#include "../block/BlockType.h"

class LODMesher {
public:
    // blockQuery(worldX, worldY, worldZ) -> BlockType
    using BlockQuery  = std::function<BlockType(int, int, int)>;
    // heightQuery(worldX, worldZ) -> highest solid Y (-1 if empty)
    using HeightQuery = std::function<int(int, int)>;

    static void build(
        int level,
        int tileX,
        int tileZ,
        const BlockQuery&  blockQuery,
        const HeightQuery& heightQuery,
        std::vector<float>& outVertices
    );
};
