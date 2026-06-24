#include "ChunkWorker.h"
#include "TerrainGenerator.h"
#include "World.h"
#include "mesher/GreedyMesher.h"
#include <algorithm>
#include <random>

ChunkWorker::ChunkWorker(World *worldPtr) {
  world = worldPtr;
  const unsigned int N = std::max(1u, std::thread::hardware_concurrency());
  for (unsigned int i = 0; i < N; i++)
    workers.emplace_back(&ChunkWorker::run, this);
}

ChunkWorker::~ChunkWorker() {
  running.store(false);
  cv.notify_all();
  for (auto &w : workers)
    if (w.joinable())
      w.join();
}

void ChunkWorker::requestChunk(int x, int z, int priority, uint32_t gen) {
  {
    std::lock_guard lock(mutex);
    requests.push({x, z, priority, gen});
  }
  cv.notify_one();
}

void ChunkWorker::enqueueMeshRequest(MeshRequest req) {
  {
    std::lock_guard lock(mutex);
    meshRequests.push(std::move(req));
  }
  cv.notify_one();
}

bool ChunkWorker::popFinishedChunk(GeneratedChunk &result) {
  std::lock_guard lock(mutex);
  if (finished.empty())
    return false;
  result = std::move(finished.front());
  finished.pop();
  return true;
}

bool ChunkWorker::popFinishedMesh(MeshResult &result) {
  std::lock_guard lock(mutex);
  if (finishedMeshes.empty())
    return false;
  result = std::move(finishedMeshes.front());
  finishedMeshes.pop();
  return true;
}

void ChunkWorker::clearRequests() {
  std::lock_guard lock(mutex);
  requests = {};
  meshRequests = {};
}

void ChunkWorker::flushFinished() {
  std::lock_guard lock(mutex);
  uint32_t gen = generation.load();

  std::queue<GeneratedChunk> newFinished;
  while (!finished.empty()) {
    if (finished.front().generation == gen)
      newFinished.push(std::move(finished.front()));
    finished.pop();
  }
  finished = std::move(newFinished);

  std::queue<MeshResult> newMeshes;
  while (!finishedMeshes.empty()) {
    if (finishedMeshes.front().generation == gen)
      newMeshes.push(std::move(finishedMeshes.front()));
    finishedMeshes.pop();
  }
  finishedMeshes = std::move(newMeshes);
}

void ChunkWorker::run() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1, 100);

  while (running.load()) {
    ChunkRequest req;
    MeshRequest meshReq;
    bool hasTerrain = false;
    bool hasMesh = false;

    {
      std::unique_lock lock(mutex);
      cv.wait(lock, [&] {
        return !requests.empty() || !meshRequests.empty() || !running.load();
      });
      if (!running.load())
        break;

      if (!meshRequests.empty() && !requests.empty()) {
        int chance = dis(gen);
        if (chance <= 70) { // 70% Ambil Mesh
          meshReq = std::move(const_cast<MeshRequest &>(meshRequests.top()));
          meshRequests.pop();
          hasMesh = true;
        } else { // 30% Ambil Terrain
          req = requests.top();
          requests.pop();
          hasTerrain = true;
        }
      }
      else if (!meshRequests.empty()) {
        meshReq = std::move(const_cast<MeshRequest &>(meshRequests.top()));
        meshRequests.pop();
        hasMesh = true;
      } else if (!requests.empty()) {
        req = requests.top();
        requests.pop();
        hasTerrain = true;
      }
    }

    if (hasMesh) {
      if (meshReq.generation != generation.load())
        continue;

      std::vector<float> localVertices;

      GreedyMesher::build(*meshReq.mainChunk, meshReq.nNX.get(),
                          meshReq.nPX.get(), meshReq.nNZ.get(),
                          meshReq.nPZ.get(), localVertices);

      if (meshReq.generation != generation.load())
        continue;

      MeshResult result;
      result.chunk = meshReq.chunk;
      result.vertices = std::move(localVertices);
      result.generation = meshReq.generation;

      std::lock_guard lock(mutex);
      finishedMeshes.push(std::move(result));
    }

    if (hasTerrain) {
      if (req.generation != generation.load())
        continue;

      auto chunk = std::make_unique<Chunk>(req.x, req.z, world);
      TerrainGenerator::generate(*chunk);

      if (req.generation != generation.load())
        continue;

      GeneratedChunk result;
      result.chunk = std::move(chunk);
      result.generation = req.generation;

      std::lock_guard lock(mutex);
      finished.push(std::move(result));
    }
  }
}