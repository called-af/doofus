#include "World.h"
#include "../core/Setting.h"
#include "../renderer/Frustum.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <glad/gl.h>
#include <SDL3/SDL.h>

World::World() { worker = std::make_unique<ChunkWorker>(this); }

World::~World() = default;

long long World::getChunkKey(int x, int z) {
  return ((long long)(unsigned int)x << 32) | (unsigned int)z;
}

//  LOD key: encode tileX, tileZ, level into a single long long
//  Bit layout: [level 4bit][tileZ 30bit][tileX 30bit]
long long World::getLODKey(int tileX, int tileZ, int level) {
  return ((long long)(level & 0xF) << 60)
       | (((long long)(unsigned int)tileZ & 0x0FFFFFFF) << 30)
       | ((long long)(unsigned int)tileX & 0x0FFFFFFF);
}

//  Chunk lookup

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

//  Dirty / remesh

void World::markChunkDirty(Chunk *chunk) {
  if (!chunk || chunk->dirty)
    return;
  chunk->dirty = true;
   remeshQueue.push_back(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

//  Load

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

  int priority = calculatePriority(chunkX, chunkZ, cameraPos, cameraFront,
                                   frustum, isLoading);

  worker->requestChunk(chunkX, chunkZ, priority, worker->generation.load());
}

//  LOD ring test
//
//  A level-L tile covers (2^L × 2^L) chunks. "tileX/tileZ" are
//  tile coordinates (not chunk coordinates). A tile is valid if its overlap with
//  chunk-space falls within the ring [lodLStart..lodLEnd] from the player.
bool World::inLODRing(int tileX, int tileZ, int level,
                      int playerChunkX, int playerChunkZ) const {
  int cov = (1 << level);           // chunks per tile side
  // Chunk coordinates of the tile north-west corner
  int chunkX0 = tileX * cov;
  int chunkZ0 = tileZ * cov;
  // Tile center in chunk coordinates
  float cx = chunkX0 + cov * 0.5f;
  float cz = chunkZ0 + cov * 0.5f;

  // Chebyshev distance from tile center to player (in chunk units)
  float dx = std::abs(cx - playerChunkX);
  float dz = std::abs(cz - playerChunkZ);
  float dist = std::max(dx, dz);

  int startDist, endDist;
  if (level == 1) {
    startDist = Setting::lod1Start;
    endDist   = Setting::lod1End;
  } else if (level == 2) {
    startDist = Setting::lod2Start;
    endDist   = Setting::lod2End;
  } else { // level == 3
    startDist = Setting::lod3Start;
    endDist   = Setting::lod3End;
  }

  return dist >= startDist && dist < endDist;
}

//  Request LOD tile from worker
void World::requestLODTile(int tileX, int tileZ, int level) {
  long long key = getLODKey(tileX, tileZ, level);

  // Already exists?
  if (lodChunks.count(key)) return;
  // Already queued?
  if (queuedLODTiles.count(key)) return;

  queuedLODTiles.insert(key);

  // Create callbacks — capture by value to be safe across threads.
  // Terrain data is final after generate(), so read-only access is safe.
  LODMeshRequest req;
  req.tileX      = tileX;
  req.tileZ      = tileZ;
  req.level      = level;
  req.key        = key;
  req.generation = worker->generation.load();

  // blockQuery: read directly from chunk data (immutable after generate)
  req.blockQuery = [this](int wx, int wy, int wz) -> BlockType {
    int cx = (int)std::floor((float)wx / Chunk::SIZE);
    int cz = (int)std::floor((float)wz / Chunk::SIZE);
    Chunk *ch = getChunk(cx, cz);
    if (!ch) return BlockType::Air;
    int lx = wx - cx * Chunk::SIZE;
    int lz = wz - cz * Chunk::SIZE;
    if (lx < 0) lx += Chunk::SIZE;
    if (lz < 0) lz += Chunk::SIZE;
    if (wy < 0 || wy >= Chunk::HEIGHT) return BlockType::Air;
    return ch->blocks[lx][wy][lz];
  };

  // heightQuery: use the heightMap already cached in Chunk
  req.heightQuery = [this](int wx, int wz) -> int {
    int cx = (int)std::floor((float)wx / Chunk::SIZE);
    int cz = (int)std::floor((float)wz / Chunk::SIZE);
    Chunk *ch = getChunk(cx, cz);
    if (!ch) return -1;
    int lx = wx - cx * Chunk::SIZE;
    int lz = wz - cz * Chunk::SIZE;
    if (lx < 0) lx += Chunk::SIZE;
    if (lz < 0) lz += Chunk::SIZE;
    return ch->heightMap[lx][lz];
  };

  worker->enqueueLODMeshRequest(std::move(req));
}

//  updateLOD — manages the LOD tile registry every frame
//
//  Ring logic similar to the CDLOD QuadTree::selectNodes concept:
//  - Level 1: ring from lod1Start to lod1End (chunk dist)
//  - Level 2: ring from lod2Start to lod2End
//  - Level 3: ring from lod3Start to lod3End
void World::updateLOD(int playerChunkX, int playerChunkZ,
                      glm::vec3 cameraPos, const Frustum &frustum) {

  for (auto it = lodChunks.begin(); it != lodChunks.end(); ) {
    auto &lc = it->second;
    if (!inLODRing(lc->tileX, lc->tileZ, lc->level, playerChunkX, playerChunkZ)) {
      queuedLODTiles.erase(it->first);
      it = lodChunks.erase(it);
    } else {
      ++it;
    }
  }

  // Iterate per level, determine which tile range should exist
  for (int level = 1; level <= 3; level++) {
    int startDist, endDist;
    if (level == 1) { startDist = Setting::lod1Start; endDist = Setting::lod1End; }
    else if (level == 2) { startDist = Setting::lod2Start; endDist = Setting::lod2End; }
    else { startDist = Setting::lod3Start; endDist = Setting::lod3End; }

    int cov = (1 << level);  // chunks per tile side

    // Tile range that might fall in the ring (overestimate, inLODRing filters)
    int tileRange = (endDist / cov) + 2;
    int playerTileX = (int)std::floor((float)playerChunkX / cov);
    int playerTileZ = (int)std::floor((float)playerChunkZ / cov);

    for (int dtx = -tileRange; dtx <= tileRange; dtx++) {
      for (int dtz = -tileRange; dtz <= tileRange; dtz++) {
        int tx = playerTileX + dtx;
        int tz = playerTileZ + dtz;

        if (!inLODRing(tx, tz, level, playerChunkX, playerChunkZ))
          continue;

        // Do not request tiles whose chunk area has not been fully generated
        // Check if at least one chunk at a tile corner exists
        // (fast heuristic — not all chunks need to exist)
        int chunkX0 = tx * cov;
        int chunkZ0 = tz * cov;
        bool hasAnyChunk = false;
        for (int dcx = 0; dcx < cov && !hasAnyChunk; dcx += std::max(1, cov/2)) {
          for (int dcz = 0; dcz < cov && !hasAnyChunk; dcz += std::max(1, cov/2)) {
            if (getChunk(chunkX0 + dcx, chunkZ0 + dcz)) hasAnyChunk = true;
          }
        }
        if (!hasAnyChunk) continue;

        requestLODTile(tx, tz, level);
      }
    }
  }

  LODMeshResult result;
  while (worker->popFinishedLODMesh(result)) {
    if (result.generation != worker->generation.load()) {
      queuedLODTiles.erase(result.key);
      continue;
    }

    queuedLODTiles.erase(result.key);

    // Create or update LODChunk
    auto it = lodChunks.find(result.key);
    if (it == lodChunks.end()) {
      auto lc = std::make_shared<LODChunk>(result.tileX, result.tileZ, result.level);
      lc->pendingVertices = std::move(result.vertices);
      lc->uploadMesh();
      lodChunks[result.key] = std::move(lc);
    } else {
      it->second->pendingVertices = std::move(result.vertices);
      it->second->uploadMesh();
    }
  }
}

//  Update (main thread, every frame)

void World::update(glm::vec3 cameraPos, glm::vec3 cameraFront,
                   const Frustum &frustum, bool isLoading) {
  int playerChunkX = isLoading ? 0 : (int)std::floor(cameraPos.x / Chunk::SIZE);
  int playerChunkZ = isLoading ? 0 : (int)std::floor(cameraPos.z / Chunk::SIZE);

  static int lastChunkX = INT_MAX;
  static int lastChunkZ = INT_MAX;

  if (playerChunkX != lastChunkX || playerChunkZ != lastChunkZ) {
    worker->nextGeneration();
    worker->clearRequests();
    worker->flushFinished();
    queuedChunks.clear();
    queuedLODTiles.clear();
    remeshQueue.clear();

    for (auto &[key, chunk] : chunks) {
      chunk->dirty = false;
      if (!chunk->mesh || chunk->empty.load()) {
        markChunkDirty(chunk.get());
      }
    }

    // All LOD tiles are discarded when position changes significantly
    // to prevent stale tiles from lingering behind
    lodChunks.clear();

    lastChunkX = playerChunkX;
    lastChunkZ = playerChunkZ;
  }

  // Request terrain
  for (int x = -Setting::renderDistance; x <= Setting::renderDistance; x++) {
    for (int z = -Setting::renderDistance; z <= Setting::renderDistance; z++) {
      if (x * x + z * z > Setting::renderDistance * Setting::renderDistance)
        continue;
      loadChunk(playerChunkX + x, playerChunkZ + z, cameraPos, cameraFront,
                frustum, isLoading);
    }
  }

  // Accepting terrain from worker
  GeneratedChunk genResult;
  while (worker->popFinishedChunk(genResult)) {
    if (genResult.generation != worker->generation.load())
      continue;

    int cx = genResult.chunk->chunkX;
    int cz = genResult.chunk->chunkZ;
    long long key = getChunkKey(cx, cz);

    chunks[key] = std::move(genResult.chunk);
    queuedChunks.erase(key);

    for (auto [ncx, ncz] : std::initializer_list<std::pair<int, int>>{
             {cx, cz},
             {cx - 1, cz},
             {cx + 1, cz},
             {cx, cz - 1},
             {cx, cz + 1},
         }) {
      Chunk *n = getChunk(ncx, ncz);
      if (!n) continue;
      n->dirty = false;
      markChunkDirty(n);
    }
  }

  // Sort remesh queue by priority
  std::sort(
      remeshQueue.begin(), remeshQueue.end(), [&](long long a, long long b) {
        auto itA = chunks.find(a);
        auto itB = chunks.find(b);
        if (itA == chunks.end() || itB == chunks.end()) return false;
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
    if (it == chunks.end()) continue;
    auto chunk = it->second;
    if (!chunk->dirty) continue;

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
      requeue.push_back(key);
      continue;
    }

    if (dispatched >= MAX_MESH_DISPATCH) {
      requeue.push_back(key);
      continue;
    }

    MeshRequest meshReq;
    meshReq.chunk      = chunk.get();
    meshReq.priority   = calculatePriority(chunk->chunkX, chunk->chunkZ,
                                            cameraPos, cameraFront, frustum, isLoading);
    meshReq.generation = worker->generation.load();
    meshReq.mainChunk  = chunk;
    meshReq.nNX = nNX; meshReq.nPX = nPX;
    meshReq.nNZ = nNZ; meshReq.nPZ = nPZ;

    worker->enqueueMeshRequest(std::move(meshReq));
    dispatched++;
  }

  remeshQueue = std::move(requeue);

  // Accept mesh results
  MeshResult meshResult;
  while (worker->popFinishedMesh(meshResult)) {
    if (meshResult.generation != worker->generation.load()) continue;

    Chunk *chunk = meshResult.chunk;
    chunk->pendingVertices = std::move(meshResult.vertices);
    chunk->empty.store(chunk->pendingVertices.empty());
    chunk->uploadMesh();
    chunk->dirty = false;
  }

  unloadFarChunks(playerChunkX, playerChunkZ);

  // LOD — only update when not in loading screen
  if (!isLoading) {
    updateLOD(playerChunkX, playerChunkZ, cameraPos, frustum);
  }
}

//  Draw — regular chunks (full detail)

void World::draw(float playerX, float playerZ, const Frustum &frustum, GLuint shaderID) {
  glUniform1i(glGetUniformLocation(shaderID, "uIsLOD"), 0);

  float nowSec = (float)(SDL_GetTicks() / 1000.0);
  glUniform1f(glGetUniformLocation(shaderID, "uTime"), nowSec);

  GLint spawnTimeLoc = glGetUniformLocation(shaderID, "uLodSpawnTime");

  int playerChunkX = (int)std::floor(playerX / Chunk::SIZE);
  int playerChunkZ = (int)std::floor(playerZ / Chunk::SIZE);

  for (auto &[key, chunk] : chunks) {
    int dx = std::abs(chunk->chunkX - playerChunkX);
    int dz = std::abs(chunk->chunkZ - playerChunkZ);

    if (dx > Setting::renderDistance || dz > Setting::renderDistance)
      continue;
    if (chunk->empty) continue;
    if (!chunk->mesh)  continue;
    if (!frustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds()))
      continue;

    glUniform1f(spawnTimeLoc, chunk->spawnTime);
    chunk->draw();
  }
}

//  drawLOD — LOD tiles (low resolution, long range)

void World::drawLOD(const Frustum &frustum, GLuint shaderID) {
  // Set uIsLOD=1 so shader skips shadow lookup
  glUniform1i(glGetUniformLocation(shaderID, "uIsLOD"), 1);

  // uTime: global time in seconds (using SDL_GetTicks)
  float nowSec = (float)(SDL_GetTicks() / 1000.0);
  glUniform1f(glGetUniformLocation(shaderID, "uTime"), nowSec);

  // Cache uLodSpawnTime location to avoid querying it per-tile every frame
  GLint spawnTimeLoc = glGetUniformLocation(shaderID, "uLodSpawnTime");

  for (auto &[key, lc] : lodChunks) {
    if (lc->empty) continue;
    if (!lc->mesh)  continue;
    if (!frustum.isBoxVisible(lc->minBounds, lc->maxBounds)) continue;

    // Send this tile spawn time — shader uses it for animation
    glUniform1f(spawnTimeLoc, lc->spawnTime);

    lc->draw();
  }

  // Reset to clean state
  glUniform1i(glGetUniformLocation(shaderID, "uIsLOD"), 0);
  glUniform1f(spawnTimeLoc, -1.0f);
}

//  Block ops

bool World::isSolid(int x, int y, int z) {
  Chunk *chunk = getChunk((int)std::floor((float)x / Chunk::SIZE),
                          (int)std::floor((float)z / Chunk::SIZE));
  if (!chunk) return false;

  int lx = x % Chunk::SIZE; if (lx < 0) lx += Chunk::SIZE;
  int lz = z % Chunk::SIZE; if (lz < 0) lz += Chunk::SIZE;

  if (y < 0 || y >= Chunk::HEIGHT) return false;
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
  if (!chunk) return;

  int lx = x % Chunk::SIZE; if (lx < 0) lx += Chunk::SIZE;
  int lz = z % Chunk::SIZE; if (lz < 0) lz += Chunk::SIZE;
  if (y < 0 || y >= Chunk::HEIGHT) return;

  chunk->blocks[lx][y][lz] = type;
  markChunkDirty(chunk);

  if (lx == 0)             if (auto *n = getChunk(chunkX - 1, chunkZ)) markChunkDirty(n);
  if (lx == Chunk::SIZE-1) if (auto *n = getChunk(chunkX + 1, chunkZ)) markChunkDirty(n);
  if (lz == 0)             if (auto *n = getChunk(chunkX, chunkZ - 1)) markChunkDirty(n);
  if (lz == Chunk::SIZE-1) if (auto *n = getChunk(chunkX, chunkZ + 1)) markChunkDirty(n);
}

//  Unload

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

//  Priority calculation

int World::calculatePriority(int chunkX, int chunkZ, glm::vec3 cameraPos,
                             glm::vec3 cameraFront, const Frustum &frustum,
                             bool isLoading) {
  if (isLoading) {
    int dx = chunkX - 0;
    int dz = chunkZ - 0;
    return (dx * dx + dz * dz);
  }

  glm::vec3 minBounds = glm::vec3(chunkX * Chunk::SIZE, 0, chunkZ * Chunk::SIZE);
  glm::vec3 maxBounds = glm::vec3(chunkX * Chunk::SIZE + Chunk::SIZE, Chunk::HEIGHT,
                                  chunkZ * Chunk::SIZE + Chunk::SIZE);
  glm::vec3 chunkCenter = (minBounds + maxBounds) * 0.5f;

  float dist = glm::distance(cameraPos, chunkCenter);
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
    float crosshairFactor = 1.0f - dot;
    priority += static_cast<int>(crosshairFactor * 500.0f);
  } else {
    priority += 15000;
  }

  return priority;
}
