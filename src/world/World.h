#pragma once

#include "../renderer/Frustum.h"
#include "Chunk.h"
#include "ChunkWorker.h"

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
  void draw(float playerX, float playerZ, const Frustum &frustum);

  bool isSolid(int x, int y, int z);
  int getHeight(int x, int z);
  void setBlock(int x, int y, int z, BlockType type);

  Chunk *getChunk(int chunkX, int chunkZ);
  std::shared_ptr<Chunk> getChunkShared(int chunkX, int chunkZ);

  void markChunkDirty(Chunk *chunk);
  long long getChunkKey(int x, int z);

private:
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
};