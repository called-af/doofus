#pragma once

#include "../renderer/Frustum.h"
#include "Chunk.h"
#include "ChunkWorker.h"
#include "LODChunk.h"

#include <memory>
#include <queue>
#include <vector>
#include <algorithm>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

class World {
public:
  World();
  ~World();

  void update(glm::vec3 cameraPos, glm::vec3 cameraFront,
              const Frustum &frustum, bool isLoading);
  void draw(float playerX, float playerZ, const Frustum &frustum, GLuint shaderID);

  // Render LOD chunks — called after draw() in Scene with uIsLOD=1 uniform flag
  void drawLOD(const Frustum &frustum, GLuint shaderID);

  bool isSolid(int x, int y, int z);
  int getHeight(int x, int z);
  void setBlock(int x, int y, int z, BlockType type);

  Chunk *getChunk(int chunkX, int chunkZ);
  std::shared_ptr<Chunk> getChunkShared(int chunkX, int chunkZ);

  void markChunkDirty(Chunk *chunk);
  long long getChunkKey(int x, int z);

private:
  // ──── Regular chunks ─────────────────────────────────────────────────────
  void loadChunk(int chunkX, int chunkZ, glm::vec3 cameraPos,
                 glm::vec3 cameraFront, const Frustum &frustum, bool isLoading);
  int calculatePriority(int chunkX, int chunkZ, glm::vec3 cameraPos,
                        glm::vec3 cameraFront, const Frustum &frustum,
                        bool isLoading);
  void unloadFarChunks(int playerChunkX, int playerChunkZ);

  std::unordered_map<long long, std::shared_ptr<Chunk>> chunks;
  mutable std::shared_mutex chunksMutex;

  std::unordered_map<long long, uint32_t> queuedChunks;
  std::vector<long long> remeshQueue;

  std::unique_ptr<ChunkWorker> worker;

  // ──── LOD ────────────────────────────────────────────────────────────────
  // Key: getLODKey(tileX, tileZ, level)
  static long long getLODKey(int tileX, int tileZ, int level);

  // Convert chunk coordinates to tile coordinates for the specified level
  // One level-L tile covers (2^L) chunks per side
  static int chunkToTile(int chunkCoord, int level) {
    int cov = (1 << level);
    return (int)std::floor((float)chunkCoord / cov);
  }

  // Check if this tile is within the correct LOD ring for its level
  // (not in the regular chunk area and not too far away)
  bool inLODRing(int tileX, int tileZ, int level,
                 int playerChunkX, int playerChunkZ) const;

  void updateLOD(int playerChunkX, int playerChunkZ,
                 glm::vec3 cameraPos, const Frustum &frustum);

  void requestLODTile(int tileX, int tileZ, int level);

  std::unordered_map<long long, std::shared_ptr<LODChunk>> lodChunks;
  std::unordered_set<long long> queuedLODTiles;  // Tiles currently being processed by worker threads
};
