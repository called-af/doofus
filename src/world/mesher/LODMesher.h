#pragma once

#include "../ChunkWorker.h"
#include "../block/BlockType.h"
#include <vector>

// Span representing a contiguous solid vertical segment in a column
struct LODSpan {
    int topY;
    int bottomY;
    BlockType surfaceType;
    bool needsBottomCap;
    bool needsSkirt;
};

// Column representing all vertical spans at a given XZ sample point
struct LODColumn {
    std::vector<LODSpan> spans;
};

class LODMesher {
public:
    static void build(const LODMeshRequest &req, std::vector<float> &outVertices);

private:
    // Phase 2: LOD 1 & 2 (Voxel blocky)
    static void buildVoxelLOD(const LODMeshRequest &req, std::vector<float> &outVertices);

    // Phase 3: LOD 3, 4, 5 (Analytical multi-span)
    static void buildAnalyticalLOD(const LODMeshRequest &req, std::vector<float> &outVertices);
};
