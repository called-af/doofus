#include "ChunkWorker.h"

#include "TerrainGenerator.h"
#include "World.h"
#include <algorithm>

ChunkWorker::ChunkWorker(World *worldPtr) {
  world = worldPtr;

  const unsigned int THREAD_COUNT =
      std::max(1u, std::thread::hardware_concurrency());

  for (int i = 0; i < THREAD_COUNT; i++) {
    workers.emplace_back(&ChunkWorker::run, this);
  }
}

ChunkWorker::~ChunkWorker() {
  running.store(false);

  cv.notify_all();

  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ChunkWorker::requestChunk(int chunkX, int chunkZ, int priority) {
  {
    std::lock_guard lock(mutex);

    requests.push({chunkX, chunkZ, priority});
  }

  cv.notify_one();
}

bool ChunkWorker::popFinishedChunk(GeneratedChunk &result) {
  std::lock_guard lock(mutex);

  if (finished.empty()) {
    return false;
  }

  result = std::move(finished.front());

  finished.pop();

  return true;
}

void ChunkWorker::run() {
  while (running.load()) {
    ChunkRequest req;

    {
      std::unique_lock lock(mutex);

      cv.wait(lock, [&] { return !requests.empty() || !running.load(); });

      if (!running.load())
        break;

      req = requests.top();
      requests.pop();
    }

    auto chunk = std::make_unique<Chunk>(req.x, req.z, world);

    TerrainGenerator::generate(*chunk);

    GeneratedChunk result;

    result.chunk = std::move(chunk);

    {
      std::lock_guard lock(mutex);

      finished.push(std::move(result));
    }
  }
}