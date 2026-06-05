#include "World.h"

#include "../core/Setting.h"
#include "../renderer/Frustum.h"

#include <cmath>
#include <iostream>

World::World() { worker = std::make_unique<ChunkWorker>(this); }

World::~World() = default;

long long World::getChunkKey(int x, int z) {
  return ((long long)x << 32) ^ (unsigned int)z;
}

void World::markChunkDirty(Chunk *chunk) {
  if (!chunk)
    return;

  if (chunk->dirty)
    return;

  chunk->dirty = true;

  remeshQueue.push(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

Chunk *World::getChunk(int chunkX, int chunkZ) {
  long long key = getChunkKey(chunkX, chunkZ);

  auto it = chunks.find(key);

  if (it == chunks.end())
    return nullptr;

  return it->second.get();
}

void World::loadChunk(int chunkX, int chunkZ, int playerChunkX,
                      int playerChunkZ) {
  long long key = getChunkKey(chunkX, chunkZ);

  if (chunks.contains(key))
    return;

  if (queuedChunks.contains(key))
    return;

  queuedChunks.insert(key);

  int dx = chunkX - playerChunkX;
  int dz = chunkZ - playerChunkZ;

  int priority = dx * dx + dz * dz;

  worker->requestChunk(chunkX, chunkZ, priority);
}

void World::update(float playerX, float playerZ) {
  int playerChunkX = (int)std::floor(playerX / Chunk::SIZE);

  int playerChunkZ = (int)std::floor(playerZ / Chunk::SIZE);

  /*
      REQUEST CHUNKS
  */

  for (int x = -Setting::renderDistance; x <= Setting::renderDistance; x++) {
    for (int z = -Setting::renderDistance; z <= Setting::renderDistance; z++) {
      loadChunk(playerChunkX + x, playerChunkZ + z, playerChunkX, playerChunkZ);
    }
  }

  /*
      RECEIVE GENERATED CHUNKS
  */

  GeneratedChunk result;

  while (worker->popFinishedChunk(result)) {
    auto chunk = std::move(result.chunk);

    long long key = getChunkKey(chunk->chunkX, chunk->chunkZ);

    int cx = chunk->chunkX;
    int cz = chunk->chunkZ;

    chunks[key] = std::move(chunk);

    queuedChunks.erase(key);

    getChunk(cx, cz)->dirty = false;

    Chunk *c = getChunk(cx, cz);

    markChunkDirty(getChunk(cx, cz));

    markChunkDirty(getChunk(cx - 1, cz));

    markChunkDirty(getChunk(cx + 1, cz));

    markChunkDirty(getChunk(cx, cz - 1));

    markChunkDirty(getChunk(cx, cz + 1));
  }

  unloadFarChunks(playerChunkX, playerChunkZ);

  const int MAX_REMESH_PER_FRAME = 2;

  for (int i = 0; i < MAX_REMESH_PER_FRAME && !remeshQueue.empty(); i++) {
    long long key = remeshQueue.front();

    remeshQueue.pop();

    Chunk *chunk = nullptr;

    auto it = chunks.find(key);

    if (it != chunks.end()) {
      chunk = it->second.get();
    }

    if (!chunk)
      continue;

    chunk->buildMesh();

    chunk->dirty = false;
  }
}

void World::draw(float playerX, float playerZ, const Frustum &frustum) {
  int playerChunkX = (int)std::floor(playerX / Chunk::SIZE);

  int playerChunkZ = (int)std::floor(playerZ / Chunk::SIZE);

  for (auto &[key, chunk] : chunks) {
    int dx = std::abs(chunk->chunkX - playerChunkX);

    int dz = std::abs(chunk->chunkZ - playerChunkZ);

    if (dx > Setting::renderDistance || dz > Setting::renderDistance) {
      continue;
    }

    if (chunk->empty)
      continue;

    if (!frustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds())) {
      continue;
    }

    chunk->draw();
  }
}

bool World::isSolid(int x, int y, int z) {
  int chunkX = std::floor((float)x / Chunk::SIZE);

  int chunkZ = std::floor((float)z / Chunk::SIZE);

  Chunk *chunk = getChunk(chunkX, chunkZ);

  if (!chunk)
    return false;

  int localX = x % Chunk::SIZE;

  int localZ = z % Chunk::SIZE;

  if (localX < 0)
    localX += Chunk::SIZE;

  if (localZ < 0)
    localZ += Chunk::SIZE;

  if (y < 0 || y >= Chunk::HEIGHT) {
    return false;
  }

  return chunk->blocks[localX][y][localZ] != BlockType::Air;
}

int World::getHeight(int x, int z) {
  for (int y = Chunk::HEIGHT - 1; y >= 0; y--) {
    if (isSolid(x, y, z)) {
      return y;
    }
  }

  return 0;
}

void World::setBlock(int x, int y, int z, BlockType type) {
  int chunkX = std::floor((float)x / Chunk::SIZE);

  int chunkZ = std::floor((float)z / Chunk::SIZE);

  Chunk *chunk = getChunk(chunkX, chunkZ);

  if (!chunk)
    return;

  int localX = x % Chunk::SIZE;

  int localZ = z % Chunk::SIZE;

  if (localX < 0)
    localX += Chunk::SIZE;

  if (localZ < 0)
    localZ += Chunk::SIZE;

  if (y < 0 || y >= Chunk::HEIGHT) {
    return;
  }

  chunk->blocks[localX][y][localZ] = type;

  markChunkDirty(chunk);

  if (localX == 0) {
    if (auto *n = getChunk(chunkX - 1, chunkZ)) {
      markChunkDirty(n);
    }
  }

  if (localX == Chunk::SIZE - 1) {
    if (auto *n = getChunk(chunkX + 1, chunkZ)) {
      markChunkDirty(n);
    }
  }

  if (localZ == 0) {
    if (auto *n = getChunk(chunkX, chunkZ - 1)) {
      markChunkDirty(n);
    }
  }

  if (localZ == Chunk::SIZE - 1) {
    if (auto *n = getChunk(chunkX, chunkZ + 1)) {
      markChunkDirty(n);
    }
  }
}

void World::unloadFarChunks(int centerChunkX, int centerChunkZ) {
  for (auto it = chunks.begin(); it != chunks.end();) {
    Chunk *chunk = it->second.get();

    int dx = std::abs(chunk->chunkX - centerChunkX);

    int dz = std::abs(chunk->chunkZ - centerChunkZ);

    if (dx > Setting::renderDistance || dz > Setting::renderDistance) {
      it = chunks.erase(it);
    } else {
      ++it;
    }
  }
}