#pragma once

#include "Chunk.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class World;

struct GeneratedChunk {
  std::unique_ptr<Chunk> chunk;
};

struct ChunkRequest {
  int x;
  int z;
  int priority;

  bool operator<(const ChunkRequest &other) const {
    return priority > other.priority;
  }
};

class ChunkWorker {
public:
  explicit ChunkWorker(World *world);

  ~ChunkWorker();

  void requestChunk(int chunkX, int chunkZ, int priority);

  bool popFinishedChunk(GeneratedChunk &result);

private:
  void run();

  World *world;

  std::vector<std::thread> workers;

  std::mutex mutex;

  std::condition_variable cv;

  std::atomic<bool> running = true;

  std::priority_queue<ChunkRequest> requests;

  std::queue<GeneratedChunk> finished;
};