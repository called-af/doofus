#include "World.h"
#include "../core/Setting.h"
#include "../renderer/Frustum.h"

#include <cmath>
#include <cstring>
#include <iostream>

World::World() { worker = std::make_unique<ChunkWorker>(this); }

World::~World() = default;

long long World::getChunkKey(int x, int z) {
  return ((long long)(unsigned int)x << 32) | (unsigned int)z;
}

// ───────────────────────────────────────
//   Chunk lookup
// ───────────────────────────────────────

Chunk *World::getChunk(int chunkX, int chunkZ) {
  long long key = getChunkKey(chunkX, chunkZ);
  auto it = chunks.find(key);
  return it != chunks.end() ? it->second.get() : nullptr;
}

std::shared_ptr<Chunk> World::getChunkShared(int chunkX, int chunkZ) {
  std::shared_lock lock(chunksMutex);
  long long key = getChunkKey(chunkX, chunkZ);
  auto it = chunks.find(key);
  return it != chunks.end() ? it->second : nullptr;
}

// ───────────────────────────────────────
//   Dirty / remesh
// ───────────────────────────────────────

void World::markChunkDirty(Chunk *chunk) {
  if (!chunk || chunk->dirty)
    return;
  chunk->dirty = true;
   remeshQueue.push_back(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

// ───────────────────────────────────────
//   Load
// ───────────────────────────────────────

void World::loadChunk(int chunkX, int chunkZ, glm::vec3 cameraPos,
                      glm::vec3 cameraFront, const Frustum &frustum,
                      bool isLoading) {
  long long key = getChunkKey(chunkX, chunkZ);

  if (chunks.contains(key))
    return;

  auto it = queuedChunks.find(key);
  if (it != queuedChunks.end() && it->second == worker->generation.load())
    return;

  queuedChunks[key] = worker->generation.load();

  // Kirim parameter isLoading
  int priority = calculatePriority(chunkX, chunkZ, cameraPos, cameraFront,
                                   frustum, isLoading);

  worker->requestChunk(chunkX, chunkZ, priority, worker->generation.load());
}

// ───────────────────────────────────────
//   Update
// ───────────────────────────────────────

void World::update(glm::vec3 cameraPos, glm::vec3 cameraFront,
                   const Frustum &frustum, bool isLoading) {
  // Jika sedang loading, kunci posisi pusat koordinat di (0,0) agar sinkron
  // dengan Scene
  int playerChunkX = isLoading ? 0 : (int)std::floor(cameraPos.x / Chunk::SIZE);
  int playerChunkZ = isLoading ? 0 : (int)std::floor(cameraPos.z / Chunk::SIZE);

  static int lastChunkX = INT_MAX;
  static int lastChunkZ = INT_MAX;

  // checking player chunk changed
  if (playerChunkX != lastChunkX || playerChunkZ != lastChunkZ) {
    worker->nextGeneration();
    worker->clearRequests();
    worker->flushFinished();
    queuedChunks.clear();
    remeshQueue.clear();

    for (auto &[key, chunk] : chunks) {
      chunk->dirty = false;
      // Hanya remesh chunk yang belum punya mesh valid
      if (!chunk->mesh || chunk->empty.load()) {
        markChunkDirty(chunk.get());
      }
    }

    lastChunkX = playerChunkX;
    lastChunkZ = playerChunkZ;
  }

  // Request terrain
  for (int x = -Setting::renderDistance; x <= Setting::renderDistance; x++) {
    for (int z = -Setting::renderDistance; z <= Setting::renderDistance; z++) {
      if (x * x + z * z > Setting::renderDistance * Setting::renderDistance)
        continue;
      // Berikan parameter isLoading
      loadChunk(playerChunkX + x, playerChunkZ + z, cameraPos, cameraFront,
                frustum, isLoading);
    }
  }

  // Accepting terrain from worker
  GeneratedChunk result;
  while (worker->popFinishedChunk(result)) {
    if (result.generation != worker->generation.load())
      continue;

    int cx = result.chunk->chunkX;
    int cz = result.chunk->chunkZ;
    long long key = getChunkKey(cx, cz);

    chunks[key] = std::move(result.chunk);
    queuedChunks.erase(key);

    for (auto [ncx, ncz] : std::initializer_list<std::pair<int, int>>{
             {cx, cz},
             {cx - 1, cz},
             {cx + 1, cz},
             {cx, cz - 1},
             {cx, cz + 1},
         }) {
      Chunk *n = getChunk(ncx, ncz);
      if (!n)
        continue;
      n->dirty = false;
      markChunkDirty(n);
    }
  }

  // Send mesh request to worker (using priority)
  // Sort dulu berdasarkan priority (ascending = terkecil duluan)
  std::sort(
      remeshQueue.begin(), remeshQueue.end(), [&](long long a, long long b) {
        auto itA = chunks.find(a);
        auto itB = chunks.find(b);
        if (itA == chunks.end() || itB == chunks.end())
          return false;
        int pA = calculatePriority(itA->second->chunkX, itA->second->chunkZ,
                                   cameraPos, cameraFront, frustum, isLoading);
        int pB = calculatePriority(itB->second->chunkX, itB->second->chunkZ,
                                   cameraPos, cameraFront, frustum, isLoading);
        return pA < pB;
      });

  const int MAX_MESH_DISPATCH = 4;
  int dispatched = 0;
  std::vector<long long> requeue;

  for (long long key : remeshQueue) {
    auto it = chunks.find(key);
    if (it == chunks.end())
      continue;
    auto chunk = it->second;
    if (!chunk->dirty)
      continue;

    int dx = std::abs(chunk->chunkX - playerChunkX);
    int dz = std::abs(chunk->chunkZ - playerChunkZ);
    if (dx > Setting::renderDistance || dz > Setting::renderDistance) {
      chunk->dirty = false;
      continue;
    }

    auto nNX = getChunkShared(chunk->chunkX - 1, chunk->chunkZ);
    auto nPX = getChunkShared(chunk->chunkX + 1, chunk->chunkZ);
    auto nNZ = getChunkShared(chunk->chunkX, chunk->chunkZ - 1);
    auto nPZ = getChunkShared(chunk->chunkX, chunk->chunkZ + 1);

    if (!nNX || !nPX || !nNZ || !nPZ) {
      requeue.push_back(key); // tetangga belum siap, coba lagi nanti
      continue;
    }

    if (dispatched >= MAX_MESH_DISPATCH) {
      requeue.push_back(key);
      continue;
    }

    MeshRequest meshReq;
    meshReq.chunk = chunk.get();
    meshReq.priority =
        calculatePriority(chunk->chunkX, chunk->chunkZ, cameraPos, cameraFront,
                          frustum, isLoading);
    meshReq.generation = worker->generation.load();
    meshReq.mainChunk = chunk;
    meshReq.nNX = nNX;
    meshReq.nPX = nPX;
    meshReq.nNZ = nNZ;
    meshReq.nPZ = nPZ;

    worker->enqueueMeshRequest(std::move(meshReq));
    dispatched++;
  }

  remeshQueue = std::move(requeue);

  // Accept result mesh from worker, upload on main thread
  MeshResult meshResult;
  while (worker->popFinishedMesh(meshResult)) {
    if (meshResult.generation != worker->generation.load())
      continue;

    Chunk *chunk = meshResult.chunk;
    chunk->pendingVertices = std::move(meshResult.vertices);
    chunk->empty.store(chunk->pendingVertices.empty());
    chunk->uploadMesh();
    chunk->dirty = false;
  }

  unloadFarChunks(playerChunkX, playerChunkZ);
}

// ───────────────────────────────────────
//   Draw
// ───────────────────────────────────────

void World::draw(float playerX, float playerZ, const Frustum &frustum) {
  int playerChunkX = (int)std::floor(playerX / Chunk::SIZE);
  int playerChunkZ = (int)std::floor(playerZ / Chunk::SIZE);

  for (auto &[key, chunk] : chunks) {
    int dx = std::abs(chunk->chunkX - playerChunkX);
    int dz = std::abs(chunk->chunkZ - playerChunkZ);

    if (dx > Setting::renderDistance || dz > Setting::renderDistance)
      continue;
    if (chunk->empty)
      continue;
    if (!chunk->mesh) {
      continue;
    }
    if (!frustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds()))
      continue;

    chunk->draw();
  }
}

// ───────────────────────────────────────
//   Block ops
// ───────────────────────────────────────

bool World::isSolid(int x, int y, int z) {
  Chunk *chunk = getChunk((int)std::floor((float)x / Chunk::SIZE),
                          (int)std::floor((float)z / Chunk::SIZE));
  if (!chunk)
    return false;

  int lx = x % Chunk::SIZE;
  if (lx < 0)
    lx += Chunk::SIZE;
  int lz = z % Chunk::SIZE;
  if (lz < 0)
    lz += Chunk::SIZE;

  if (y < 0 || y >= Chunk::HEIGHT)
    return false;
  return chunk->blocks[lx][y][lz] != BlockType::Air;
}

int World::getHeight(int x, int z) {
  for (int y = Chunk::HEIGHT - 1; y >= 0; y--)
    if (isSolid(x, y, z))
      return y;
  return 0;
}

void World::setBlock(int x, int y, int z, BlockType type) {
  int chunkX = (int)std::floor((float)x / Chunk::SIZE);
  int chunkZ = (int)std::floor((float)z / Chunk::SIZE);
  Chunk *chunk = getChunk(chunkX, chunkZ);
  if (!chunk)
    return;

  int lx = x % Chunk::SIZE;
  if (lx < 0)
    lx += Chunk::SIZE;
  int lz = z % Chunk::SIZE;
  if (lz < 0)
    lz += Chunk::SIZE;
  if (y < 0 || y >= Chunk::HEIGHT)
    return;

  chunk->blocks[lx][y][lz] = type;
  markChunkDirty(chunk);

  if (lx == 0)
    if (auto *n = getChunk(chunkX - 1, chunkZ))
      markChunkDirty(n);
  if (lx == Chunk::SIZE - 1)
    if (auto *n = getChunk(chunkX + 1, chunkZ))
      markChunkDirty(n);
  if (lz == 0)
    if (auto *n = getChunk(chunkX, chunkZ - 1))
      markChunkDirty(n);
  if (lz == Chunk::SIZE - 1)
    if (auto *n = getChunk(chunkX, chunkZ + 1))
      markChunkDirty(n);
}

// ───────────────────────────────────────
//   Unload
// ───────────────────────────────────────

void World::unloadFarChunks(int centerChunkX, int centerChunkZ) {
  const int UNLOAD_DISTANCE = Setting::renderDistance + 2;

  for (auto it = chunks.begin(); it != chunks.end();) {
    Chunk *chunk = it->second.get();
    int dx = std::abs(chunk->chunkX - centerChunkX);
    int dz = std::abs(chunk->chunkZ - centerChunkZ);

    if (dx > UNLOAD_DISTANCE || dz > UNLOAD_DISTANCE)
      it = chunks.erase(it);
    else
      ++it;
  }
}

int World::calculatePriority(int chunkX, int chunkZ, glm::vec3 cameraPos,
                             glm::vec3 cameraFront, const Frustum &frustum,
                             bool isLoading) {
  // Jika masih loading awal game, pakai jarak lingkaran murni dari pusat 0,0
  if (isLoading) {
    int dx = chunkX - 0;
    int dz = chunkZ - 0;
    return (dx * dx + dz * dz);
  }

  // --- Logika kamera saat gameplay ---
  glm::vec3 minBounds =
      glm::vec3(chunkX * Chunk::SIZE, 0, chunkZ * Chunk::SIZE);
  glm::vec3 maxBounds =
      glm::vec3(chunkX * Chunk::SIZE + Chunk::SIZE, Chunk::HEIGHT,
                chunkZ * Chunk::SIZE + Chunk::SIZE);
  glm::vec3 chunkCenter = (minBounds + maxBounds) * 0.5f;

  float dist = glm::distance(cameraPos, chunkCenter);

  // KUNCI: Jarak murni dikali 100 biar jadi fondasi angka utama.
  // Semakin dekat chunk dengan player, angkanya semakin kecil (prioritas
  // semakin tinggi).
  int priority = static_cast<int>(dist * 100.0f);

  bool inFrustum = frustum.isBoxVisible(minBounds, maxBounds);

  if (inFrustum) {
    glm::vec3 toChunk = chunkCenter - cameraPos;
    float len = glm::length(toChunk);
    float dot = 0.0f;
    if (len > 0.001f) {
      toChunk /= len;
      dot = glm::dot(cameraFront, toChunk);
    }

    // Bonus prioritas kalau pas di tengah crosshair (mengurangi nilai priority)
    float crosshairFactor = 1.0f - dot;
    priority += static_cast<int>(crosshairFactor * 500.0f);
  } else {
    // Penalti moderat untuk chunk di luar pandangan (belakang/bawah kaki saat
    // mendongak). Nilai +15000 ini cukup adil: mendahulukan chunk depan yang
    // kelihatan, tapi GAK AKAN mengalahkan chunk yang super dekat di bawah
    // kaki.
    priority += 15000;
  }

  return priority;
}