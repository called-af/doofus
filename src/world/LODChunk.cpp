#include "LODChunk.h"
#include "Chunk.h"
#include <SDL3/SDL.h>

// ─────────────────────────────────────────────────────────────────────────────
//  LODChunk — implementation
// ─────────────────────────────────────────────────────────────────────────────

LODChunk::LODChunk(int tileX_, int tileZ_, int level_)
    : tileX(tileX_), tileZ(tileZ_), level(level_)
{
    // Compute coverage in chunk units
    int cov = chunkCoverage();

    // Compute AABB in world space
    float wx = (float)(tileX * cov * Chunk::SIZE);
    float wz = (float)(tileZ * cov * Chunk::SIZE);
    float sz = (float)(cov * Chunk::SIZE);

    minBounds = { wx,        0.0f,              wz      };
    maxBounds = { wx + sz,   (float)Chunk::HEIGHT, wz + sz };
}

void LODChunk::uploadMesh()
{
    // If there are no vertices, release the mesh and mark as empty
    if (pendingVertices.empty()) {
        mesh.reset();
        empty = true;
        return;
    }

    unsigned int bytes = (unsigned int)(pendingVertices.size() * sizeof(float));

    if (!mesh)
        // Create a new GPU mesh
        mesh = std::make_unique<Mesh>(pendingVertices.data(), bytes);
    else
        // Update the existing GPU mesh (avoids reallocation)
        mesh->update(pendingVertices.data(), bytes);

    empty = false;

    // Record spawn time only on the first upload (not on subsequent updates).
    // The shader uses this value for the fade-in animation.
    if (spawnTime < 0.0f)
        spawnTime = (float)(SDL_GetTicks() / 1000.0);

    // Retain capacity for future mesh updates without reallocating.
    pendingVertices.clear();
}

void LODChunk::draw()
{
    if (mesh && !empty)
        mesh->draw();
}

glm::vec3 LODChunk::worldOrigin() const
{
    // North-west corner position of this tile in world space
    return minBounds;
}
