#include "Chunk.h"
#include "World.h"
#include "mesher/GreedyMesher.h"
#include <cstring>
#include <glm/glm.hpp>
#include <iostream>

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
    vertices = std::move(pendingVertices);
    empty.store(vertices.empty());

    if (vertices.empty()) {
        mesh.reset();
        return;
    }

    unsigned int size = vertices.size() * sizeof(float);
    if (!mesh)
        mesh = std::make_unique<Mesh>(vertices.data(), size);
    else
        mesh->update(vertices.data(), size);

    vertices.clear();
    vertices.shrink_to_fit();
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