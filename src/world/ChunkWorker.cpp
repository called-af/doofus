#include "ChunkWorker.h"
#include "TerrainGenerator.h"
#include "World.h"
#include "mesher/GreedyMesher.h"
#include "mesher/LODMesher.h"
#include "../core/Setting.h"
#include <algorithm>
#include <random>

//  ChunkWorker — constructor & destructor

ChunkWorker::ChunkWorker(World *worldPtr)
{
    world = worldPtr;

    // Determine the optimal thread count:
    //   If Setting::maxWorkerThreads == 0 → auto = hardware_concurrency - 1
    //   Minimum 2 threads so terrain + mesh can run in parallel
    //   Capped to avoid flooding the CPU while the game is running
    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int n;
    if (Setting::maxWorkerThreads > 0)
    {
        n = (unsigned int)Setting::maxWorkerThreads;
    }
    else
    {
        // Terrain sampling is CPU heavy.  Reserving only one core can starve
        // the main/render thread and cause sharp FPS drops while moving.
        // Four workers keep mesh streaming responsive without saturating CPU.
        n = (hw > 2) ? std::min(hw - 1, 4u) : 2;
    }
    // Explicit settings may still choose fewer/more workers, but avoid an
    // accidental oversized pool on high-core CPUs.
    n = std::clamp(n, 2u, 8u);

    for (unsigned int i = 0; i < n; i++)
        workers.emplace_back(&ChunkWorker::run, this);
}

ChunkWorker::~ChunkWorker()
{
    // Signal all threads to stop, then wait for them to finish
    running.store(false);
    cv.notify_all();
    for (auto &w : workers)
        if (w.joinable())
            w.join();
}

//  Terrain API

void ChunkWorker::requestChunk(int x, int z, int priority, uint32_t gen)
{
    {
        std::lock_guard lock(mutex);
        requests.push({x, z, priority, gen});
    }
    cv.notify_one();
}

bool ChunkWorker::popFinishedChunk(GeneratedChunk &result)
{
    std::lock_guard lock(mutex);
    if (finished.empty())
        return false;
    result = std::move(finished.front());
    finished.pop();
    return true;
}

//  Regular Mesh API

void ChunkWorker::enqueueMeshRequest(MeshRequest req)
{
    {
        std::lock_guard lock(mutex);
        meshRequests.push(std::move(req));
    }
    cv.notify_one();
}

bool ChunkWorker::popFinishedMesh(MeshResult &result)
{
    std::lock_guard lock(mutex);
    if (finishedMeshes.empty())
        return false;
    result = std::move(finishedMeshes.front());
    finishedMeshes.pop();
    return true;
}

//  LOD Mesh API

void ChunkWorker::enqueueLODMeshRequest(LODMeshRequest req)
{
    {
        std::lock_guard lock(mutex);
        lodMeshRequests.push(std::move(req));
    }
    cv.notify_one();
}

bool ChunkWorker::popFinishedLODMesh(LODMeshResult &result)
{
    std::lock_guard lock(mutex);
    if (finishedLODMeshes.empty())
        return false;
    result = std::move(finishedLODMeshes.front());
    finishedLODMeshes.pop();
    return true;
}

//  Queue management

void ChunkWorker::clearRequests()
{
    std::lock_guard lock(mutex);
    // Clear terrain and regular mesh queues (a new generation immediately replaces them)
    requests = {};
    meshRequests = {};
    // lodMeshRequests is NOT cleared — LOD generation is managed separately
    // and does not need to be discarded every time the player moves one chunk
}

void ChunkWorker::flushFinished()
{
    std::lock_guard lock(mutex);
    uint32_t gen = generation.load();
    uint32_t lodGen = lodGeneration.load();

    // Discard terrain results whose generation is outdated
    {
        std::queue<GeneratedChunk> q;
        while (!finished.empty())
        {
            if (finished.front().generation == gen)
                q.push(std::move(finished.front()));
            finished.pop();
        }
        finished = std::move(q);
    }

    // Discard outdated regular mesh results
    {
        std::queue<MeshResult> q;
        while (!finishedMeshes.empty())
        {
            if (finishedMeshes.front().generation == gen)
                q.push(std::move(finishedMeshes.front()));
            finishedMeshes.pop();
        }
        finishedMeshes = std::move(q);
    }

    // Discard outdated LOD mesh results (uses lodGen, not gen)
    {
        std::queue<LODMeshResult> q;
        while (!finishedLODMeshes.empty())
        {
            if (finishedLODMeshes.front().generation == lodGen)
                q.push(std::move(finishedLODMeshes.front()));
            finishedLODMeshes.pop();
        }
        finishedLODMeshes = std::move(q);
    }
}

//  Worker thread function — main loop
//
//  Job selection priority (when multiple queues have work):
//    Regular mesh : 55% — most frequently requested during normal gameplay
//    Terrain      : 30% — important during loading or when the player moves fast
//    LOD mesh     : 15% — background work, not urgent
//
//  If only one queue has work, always pick from that queue (100%).

void ChunkWorker::run()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<> dis(1, 100);

    while (running.load())
    {
        ChunkRequest req;
        MeshRequest meshReq;
        LODMeshRequest lodReq;
        bool hasTerrain = false;
        bool hasMesh = false;
        bool hasLOD = false;

        {
            std::unique_lock lock(mutex);
            cv.wait(lock, [&]
                    { return !requests.empty() || !meshRequests.empty() || !lodMeshRequests.empty() || !running.load(); });
            if (!running.load())
                break;

            // Count how many queue types have work
            int total = (!requests.empty() ? 1 : 0) + (!meshRequests.empty() ? 1 : 0) + (!lodMeshRequests.empty() ? 1 : 0);

            if (total == 1)
            {
                // Only one type available — pick it directly
                if (!meshRequests.empty())
                {
                    meshReq = std::move(const_cast<MeshRequest &>(meshRequests.top()));
                    meshRequests.pop();
                    hasMesh = true;
                }
                else if (!requests.empty())
                {
                    req = requests.top();
                    requests.pop();
                    hasTerrain = true;
                }
                else
                {
                    lodReq = std::move(const_cast<LODMeshRequest &>(lodMeshRequests.top()));
                    lodMeshRequests.pop();
                    hasLOD = true;
                }
            }
            else
            {
                // Multiple types available — use probability to distribute work
                // Mesh=45%, Terrain=30%, LOD=25%
                int roll = dis(rng);

                if (!meshRequests.empty() && roll <= 45)
                {
                    meshReq = std::move(const_cast<MeshRequest &>(meshRequests.top()));
                    meshRequests.pop();
                    hasMesh = true;
                }
                else if (!requests.empty() && roll <= 75)
                {
                    req = requests.top();
                    requests.pop();
                    hasTerrain = true;
                }
                else if (!lodMeshRequests.empty())
                {
                    lodReq = std::move(const_cast<LODMeshRequest &>(lodMeshRequests.top()));
                    lodMeshRequests.pop();
                    hasLOD = true;
                }
                else if (!meshRequests.empty())
                {
                    // Fallback: take mesh if LOD queue is empty
                    meshReq = std::move(const_cast<MeshRequest &>(meshRequests.top()));
                    meshRequests.pop();
                    hasMesh = true;
                }
                else if (!requests.empty())
                {
                    // Fallback: take terrain if everything else is empty
                    req = requests.top();
                    requests.pop();
                    hasTerrain = true;
                }
            }
        }

        // Process regular mesh
        if (hasMesh)
        {
            // Skip if the request is already outdated
            if (meshReq.generation != generation.load())
                continue;

            std::vector<float> localVertices;
            GreedyMesher::build(*meshReq.mainChunk,
                                meshReq.nNX.get(), meshReq.nPX.get(),
                                meshReq.nNZ.get(), meshReq.nPZ.get(),
                                localVertices);

            // Check again after the build (could have become outdated during processing)
            if (meshReq.generation != generation.load())
                continue;

            MeshResult result;
            result.chunk = meshReq.chunk;
            result.vertices = std::move(localVertices);
            result.generation = meshReq.generation;

            std::lock_guard lock(mutex);
            finishedMeshes.push(std::move(result));
        }

        // Process terrain generation
        if (hasTerrain)
        {
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

        // Process LOD mesh
        if (hasLOD)
        {
            if (lodReq.generation != lodGeneration.load())
                continue;

            std::vector<float> localVertices;
            LODMesher::build(lodReq, localVertices);

            if (lodReq.generation != lodGeneration.load())
                continue;

            LODMeshResult result;
            result.key = lodReq.key;
            result.tileX = lodReq.tileX;
            result.tileZ = lodReq.tileZ;
            result.level = lodReq.level;
            result.vertices = std::move(localVertices);
            result.generation = lodReq.generation;

            std::lock_guard lock(mutex);
            finishedLODMeshes.push(std::move(result));
        }
    }
}
