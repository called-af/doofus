#include "Chunk.h"
#include "World.h"
#include "mesher/GreedyMesher.h"
#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <SDL3/SDL.h>

Chunk::Chunk(int x, int z, World *worldPtr) {
  chunkX = x;
  chunkZ = z;
  world = worldPtr;
}

void Chunk::generateMeshData() {
    vertices.clear();
    pendingVertices.clear();

    Chunk *neighborNX = world->getChunk(chunkX - 1, chunkZ);
    Chunk *neighborPX = world->getChunk(chunkX + 1, chunkZ);
    Chunk *neighborNZ = world->getChunk(chunkX, chunkZ - 1);
    Chunk *neighborPZ = world->getChunk(chunkX, chunkZ + 1);

    GreedyMesher::build(*this, neighborNX, neighborPX, neighborNZ, neighborPZ, vertices);

    pendingVertices = std::move(vertices);
}

void Chunk::uploadMesh() {
    empty.store(pendingVertices.empty());

    if (pendingVertices.empty()) {
        mesh.reset();
        return;
    }

    const unsigned int size = pendingVertices.size() * sizeof(float);
    if (!mesh) {
        mesh = std::make_unique<Mesh>(pendingVertices.data(), size);
        // Record spawn time only when mesh is first created
        spawnTime = (float)(SDL_GetTicks() / 1000.0);
    } else {
        mesh->update(pendingVertices.data(), size);
        // Remesh (dirty update) does not reset animation — tile has already appeared
    }

    pendingVertices.clear();
}

void Chunk::draw() {
  if (mesh)
    mesh->draw();
}

glm::vec3 Chunk::getMinBounds() const {
  return glm::vec3(chunkX * SIZE, 0, chunkZ * SIZE);
}

glm::vec3 Chunk::getMaxBounds() const {
  return glm::vec3(chunkX * SIZE + SIZE, HEIGHT, chunkZ * SIZE + SIZE);
}
