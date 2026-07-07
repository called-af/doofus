#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "../renderer/opengl/Mesh.h"

// ─────────────────────────────────────────────────────────────────────────────
//  LODChunk
//
//  A single "tile" in the LOD ring system. Each tile combines multiple regular
//  chunks into one low-resolution mesh.
//
//  Concept from CDLOD (LoD/src/cpp/engine/cdlod/):
//  - Level determines downsampling scale (1 = 2x, 2 = 4x, 3 = 8x)
//  - Each tile covers (2^level × 2^level) regular chunks
//
//  Vertex format is exactly the same as regular chunks:
//    [x, y, z,  u, v,  layer,  light]   (7 floats per vertex)
// ─────────────────────────────────────────────────────────────────────────────

class LODChunk {
public:
    // "Tile coordinates" in the LOD grid — not regular chunk coordinates.
    // One tile covers (2^level) × (2^level) regular chunks.
    int tileX = 0;
    int tileZ = 0;
    int level  = 1;    // 1, 2, or 3

    // Number of regular chunks covered on one side (2^level).
    int chunkCoverage() const { return 1 << level; }

    // AABB in world space (for frustum culling).
    glm::vec3 minBounds{0};
    glm::vec3 maxBounds{0};

    // GPU mesh, null until uploadMesh() is called.
    std::unique_ptr<Mesh> mesh;

    // CPU-side buffer for uploading to GPU.
    std::vector<float> pendingVertices;

    // true means no visible voxels - skip draw call.
    bool empty = true;

    // Time in seconds (from SDL_GetTicks64 / 1000.0) when the mesh was first
    // uploaded to GPU - used by shader for spawn animation.
    float spawnTime = -1.0f;

    LODChunk() = default;
    LODChunk(int tileX, int tileZ, int level);

    void uploadMesh();
    void draw();

    // World-space origin (north-west corner) of this tile.
    glm::vec3 worldOrigin() const;
};
