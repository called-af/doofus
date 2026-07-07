#include "LODChunk.h"
#include "Chunk.h"
#include <SDL3/SDL.h>

LODChunk::LODChunk(int tileX_, int tileZ_, int level_)
    : tileX(tileX_), tileZ(tileZ_), level(level_) {
    // Coverage in chunk units
    int cov = chunkCoverage();
    // World-space bounds
    float wx = (float)(tileX * cov * Chunk::SIZE);
    float wz = (float)(tileZ * cov * Chunk::SIZE);
    float sz = (float)(cov * Chunk::SIZE);
    minBounds = {wx,    0.0f,        wz};
    maxBounds = {wx+sz, (float)Chunk::HEIGHT, wz+sz};
}

void LODChunk::uploadMesh() {
    if (pendingVertices.empty()) {
        mesh.reset();
        empty = true;
        return;
    }

    unsigned int bytes = (unsigned int)(pendingVertices.size() * sizeof(float));
    if (!mesh)
        mesh = std::make_unique<Mesh>(pendingVertices.data(), bytes);
    else
        mesh->update(pendingVertices.data(), bytes);

    empty = false;

    // Record spawn time when first uploaded (not on update)
    if (spawnTime < 0.0f)
        spawnTime = (float)(SDL_GetTicks() / 1000.0);

    pendingVertices.clear();
    pendingVertices.shrink_to_fit();
}

void LODChunk::draw() {
    if (mesh && !empty)
        mesh->draw();
}

glm::vec3 LODChunk::worldOrigin() const {
    return minBounds;
}
