#include "ChunkWorker.h"
#include "TerrainGenerator.h"
#include "World.h"
#include "mesher/GreedyMesher.h"
#include "mesher/LODMesher.h"
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

//  Terrain

void ChunkWorker::requestChunk(int x, int z, int priority, uint32_t gen) {
  {
    std::lock_guard lock(mutex);
    requests.push({x, z, priority, gen});
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

//  Regular mesh

void ChunkWorker::enqueueMeshRequest(MeshRequest req) {
  {
    std::lock_guard lock(mutex);
    meshRequests.push(std::move(req));
  }
  cv.notify_one();
}

bool ChunkWorker::popFinishedMesh(MeshResult &result) {
  std::lock_guard lock(mutex);
  if (finishedMeshes.empty())
    return false;
  result = std::move(finishedMeshes.front());
  finishedMeshes.pop();
  return true;
}

//  LOD mesh

void ChunkWorker::enqueueLODMeshRequest(LODMeshRequest req) {
  {
    std::lock_guard lock(mutex);
    lodMeshRequests.push(std::move(req));
  }
  cv.notify_one();
}

bool ChunkWorker::popFinishedLODMesh(LODMeshResult &result) {
  std::lock_guard lock(mutex);
  if (finishedLODMeshes.empty())
    return false;
  result = std::move(finishedLODMeshes.front());
  finishedLODMeshes.pop();
  return true;
}

//  Queue management

void ChunkWorker::clearRequests() {
  std::lock_guard lock(mutex);
  requests       = {};
  meshRequests   = {};
  lodMeshRequests = {};
}

void ChunkWorker::flushFinished() {
  std::lock_guard lock(mutex);
  uint32_t gen = generation.load();

  // Flush terrain
  {
    std::queue<GeneratedChunk> q;
    while (!finished.empty()) {
      if (finished.front().generation == gen)
        q.push(std::move(finished.front()));
      finished.pop();
    }
    finished = std::move(q);
  }

  // Flush regular mesh
  {
    std::queue<MeshResult> q;
    while (!finishedMeshes.empty()) {
      if (finishedMeshes.front().generation == gen)
        q.push(std::move(finishedMeshes.front()));
      finishedMeshes.pop();
    }
    finishedMeshes = std::move(q);
  }

  // Flush LOD mesh
  {
    std::queue<LODMeshResult> q;
    while (!finishedLODMeshes.empty()) {
      if (finishedLODMeshes.front().generation == gen)
        q.push(std::move(finishedLODMeshes.front()));
      finishedLODMeshes.pop();
    }
    finishedLODMeshes = std::move(q);
  }
}

//  Worker thread

void ChunkWorker::run() {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<> dis(1, 100);

  while (running.load()) {
    ChunkRequest    req;
    MeshRequest     meshReq;
    LODMeshRequest  lodReq;
    bool hasTerrain = false;
    bool hasMesh    = false;
    bool hasLOD     = false;

    {
      std::unique_lock lock(mutex);
      cv.wait(lock, [&] {
        return !requests.empty() || !meshRequests.empty()
            || !lodMeshRequests.empty() || !running.load();
      });
      if (!running.load()) break;

      // Job pickup priority:
      //  Terrain:  30% when competing
      //  Mesh:     55%
      //  LOD:      15%
      int total_pending = (!requests.empty() ? 1 : 0)
                        + (!meshRequests.empty() ? 1 : 0)
                        + (!lodMeshRequests.empty() ? 1 : 0);

      if (total_pending == 1) {
        if (!meshRequests.empty()) {
          meshReq = std::move(const_cast<MeshRequest&>(meshRequests.top()));
          meshRequests.pop();
          hasMesh = true;
        } else if (!requests.empty()) {
          req = requests.top();
          requests.pop();
          hasTerrain = true;
        } else {
          lodReq = std::move(const_cast<LODMeshRequest&>(lodMeshRequests.top()));
          lodMeshRequests.pop();
          hasLOD = true;
        }
      } else {
        int roll = dis(rng);
        if (!meshRequests.empty() && roll <= 55) {
          meshReq = std::move(const_cast<MeshRequest&>(meshRequests.top()));
          meshRequests.pop();
          hasMesh = true;
        } else if (!requests.empty() && roll <= 85) {
          req = requests.top();
          requests.pop();
          hasTerrain = true;
        } else if (!lodMeshRequests.empty()) {
          lodReq = std::move(const_cast<LODMeshRequest&>(lodMeshRequests.top()));
          lodMeshRequests.pop();
          hasLOD = true;
        } else if (!meshRequests.empty()) {
          meshReq = std::move(const_cast<MeshRequest&>(meshRequests.top()));
          meshRequests.pop();
          hasMesh = true;
        } else if (!requests.empty()) {
          req = requests.top();
          requests.pop();
          hasTerrain = true;
        }
      }
    }

    //  Regular mesh 
    if (hasMesh) {
      if (meshReq.generation != generation.load()) continue;

      std::vector<float> localVertices;
      GreedyMesher::build(*meshReq.mainChunk,
                          meshReq.nNX.get(), meshReq.nPX.get(),
                          meshReq.nNZ.get(), meshReq.nPZ.get(),
                          localVertices);

      if (meshReq.generation != generation.load()) continue;

      MeshResult result;
      result.chunk      = meshReq.chunk;
      result.vertices   = std::move(localVertices);
      result.generation = meshReq.generation;

      std::lock_guard lock(mutex);
      finishedMeshes.push(std::move(result));
    }

    //  Terrain generation 
    if (hasTerrain) {
      if (req.generation != generation.load()) continue;

      auto chunk = std::make_unique<Chunk>(req.x, req.z, world);
      TerrainGenerator::generate(*chunk);

      if (req.generation != generation.load()) continue;

      GeneratedChunk result;
      result.chunk      = std::move(chunk);
      result.generation = req.generation;

      std::lock_guard lock(mutex);
      finished.push(std::move(result));
    }

    //  LOD mesh generation 
    if (hasLOD) {
      if (lodReq.generation != generation.load()) continue;

      std::vector<float> lodVertices;
      LODMesher::build(
          lodReq.level,
          lodReq.tileX,
          lodReq.tileZ,
          lodReq.blockQuery,
          lodReq.heightQuery,
          lodVertices);

      if (lodReq.generation != generation.load()) continue;

      LODMeshResult result;
      result.key        = lodReq.key;
      result.tileX      = lodReq.tileX;
      result.tileZ      = lodReq.tileZ;
      result.level      = lodReq.level;
      result.vertices   = std::move(lodVertices);
      result.generation = lodReq.generation;

      std::lock_guard lock(mutex);
      finishedLODMeshes.push(std::move(result));
    }
  }
}
