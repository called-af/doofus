#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "../renderer/opengl/Mesh.h"

// ─────────────────────────────────────────────────────────────────────────────
//  LODChunk
//
//  A single "tile" in the LOD ring system. Each tile merges several regular
//  chunks into one lower-resolution mesh.
//
//  Inspired by CDLOD (QuadTree-based Level of Detail):
//    The level determines the downsampling scale:
//      Level 1 → step=2   (2×2   chunks per tile, ~32 blocks per cell)
//      Level 2 → step=4   (4×4   chunks per tile, ~64 blocks per cell)
//      Level 3 → step=8   (8×8   chunks per tile, ~128 blocks per cell)
//      Level 4 → step=16  (16×16 chunks per tile, ~256 blocks per cell)
//      Level 5 → step=32  (32×32 chunks per tile, ~512 blocks per cell)
//
//  Vertex format is identical to regular chunks:
//    [x, y, z,  u, v,  layer,  light]   (7 floats per vertex)
// ─────────────────────────────────────────────────────────────────────────────

class LODChunk {
public:
    // Tile coordinates in the LOD grid — not regular chunk coordinates.
    // One tile covers (2^level) × (2^level) regular chunks.
    int tileX = 0;
    int tileZ = 0;
    int level = 1;  // 1, 2, 3, 4, or 5

    // Number of regular chunks covered on one side (= 2^level).
    int chunkCoverage() const { return 1 << level; }

    // AABB in world space — used for frustum culling.
    glm::vec3 minBounds{0};
    glm::vec3 maxBounds{0};

    // GPU mesh, null until uploadMesh() is called.
    std::unique_ptr<Mesh> mesh;

    // CPU-side buffer for uploading to the GPU.
    std::vector<float> pendingVertices;

    // true = no visible voxels, skip draw call.
    bool empty = true;

    // Time in seconds (SDL_GetTicks / 1000.0) when the mesh was first
    // uploaded to the GPU — used by the shader for spawn animation (fade-in).
    float spawnTime = -1.0f;

    LODChunk() = default;
    LODChunk(int tileX, int tileZ, int level);

    // Upload pendingVertices to the GPU and clear the CPU buffer.
    void uploadMesh();

    // Draw this mesh (simply calls mesh->draw()).
    void draw();

    // Origin position (north-west corner) of this tile in world space.
    glm::vec3 worldOrigin() const;
};
