#pragma once
#include "Chunk.h"
#include "LODChunk.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class World;

// ─────────────────────────────────────────────────────────────────────────────
//  Data Structures for Terrain Generation
// ─────────────────────────────────────────────────────────────────────────────

// Result of terrain generation from a worker thread
struct GeneratedChunk
{
    std::unique_ptr<Chunk> chunk;
    uint32_t generation; // generation counter for validating stale results
};

// Terrain generation request for a single chunk
struct ChunkRequest
{
    int x, z;       // chunk coordinates
    int priority;   // lower value = higher priority
    uint32_t generation;

    // Priority queue is a max-heap, so we invert to make lower value = higher priority
    bool operator<(const ChunkRequest& o) const { return priority > o.priority; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Data Structures for Regular (full-detail) Mesh
// ─────────────────────────────────────────────────────────────────────────────

// Mesh build request for a single chunk (requires 4 neighbours)
struct MeshRequest
{
    Chunk*   chunk;
    int      priority;
    uint32_t generation;

    // Main chunk + 4 neighbours (shared_ptr so data stays alive when chunk is unloaded)
    std::shared_ptr<Chunk> mainChunk;
    std::shared_ptr<Chunk> nNX; // neighbour -X
    std::shared_ptr<Chunk> nPX; // neighbour +X
    std::shared_ptr<Chunk> nNZ; // neighbour -Z
    std::shared_ptr<Chunk> nPZ; // neighbour +Z

    bool operator<(const MeshRequest& o) const { return priority > o.priority; }
};

// Result of a regular mesh build from a worker thread
struct MeshResult
{
    Chunk*             chunk;
    std::vector<float> vertices;
    uint32_t           generation;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Data Structures for LOD Mesh
//
//  ChunkWorker accepts LODMeshRequests and produces LODMeshResults.
//  The mesher uses blockQuery & heightQuery callbacks provided by World,
//  so ChunkWorker does not need to know World internals.
// ─────────────────────────────────────────────────────────────────────────────

// LOD mesh build request for a single tile
struct LODMeshRequest
{
    int      tileX = 0;
    int      tileZ = 0;
    int      level = 1; // 1–5

    long long key        = 0; // unique tile key for result lookup
    uint32_t  generation = 0;

    // Callbacks from World — read-only data access, safe from any thread
    std::function<BlockType(int, int, int)> blockQuery;  // (wx, wy, wz) -> BlockType
    std::function<int(int, int)>            heightQuery; // (wx, wz)     -> int (highest Y)

    // priority_queue is a max-heap: invert the comparison so LOD1 is built
    // first, then LOD2 through LOD5.  This fills the visible near boundary
    // before spending worker time on the horizon.
    bool operator<(const LODMeshRequest& o) const { return level > o.level; }
};

// Result of a LOD mesh build from a worker thread
struct LODMeshResult
{
    long long          key        = 0;
    int                tileX      = 0;
    int                tileZ      = 0;
    int                level      = 1;
    std::vector<float> vertices;
    uint32_t           generation = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ChunkWorker
//
//  Thread pool that processes three types of work in parallel:
//    1. Terrain generation   (ChunkRequest)
//    2. Regular / full-detail mesh building (MeshRequest)
//    3. LOD / low-resolution mesh building  (LODMeshRequest)
//
//  Thread count: Setting::maxWorkerThreads (0 = auto, hardware_concurrency - 1, min 2)
//
//  Job selection priority (when all queues have work):
//    Regular mesh : 55% — most frequently requested during gameplay
//    Terrain      : 30% — important during initial loading
//    LOD mesh     : 15% — not urgent, processed with remaining capacity
// ─────────────────────────────────────────────────────────────────────────────
class ChunkWorker
{
public:
    explicit ChunkWorker(World* world);
    ~ChunkWorker();

    // ── Terrain ──────────────────────────────────────────────────────────
    void requestChunk(int x, int z, int priority, uint32_t generation);
    bool popFinishedChunk(GeneratedChunk& result);

    // ── Regular mesh ─────────────────────────────────────────────────────
    void enqueueMeshRequest(MeshRequest req);
    bool popFinishedMesh(MeshResult& result);

    // ── LOD mesh ─────────────────────────────────────────────────────────
    void enqueueLODMeshRequest(LODMeshRequest req);
    bool popFinishedLODMesh(LODMeshResult& result);

    // ── Queue management ──────────────────────────────────────────────────
    void clearRequests();  // discard all pending (unprocessed) requests
    void flushFinished();  // discard results that are no longer relevant (old generation)

    // Generation counters — increment when the player moves a large distance
    std::atomic<uint32_t> generation    = 0; // terrain + regular mesh
    std::atomic<uint32_t> lodGeneration = 0; // LOD mesh only
    void nextGeneration()    { generation++; }
    void nextLODGeneration() { lodGeneration++; }

private:
    // Main loop for each worker thread
    void run();

    World*                world;
    std::vector<std::thread> workers;
    std::mutex            mutex;
    std::condition_variable cv;
    std::atomic<bool>     running = true;

    // Priority queues for each job type
    std::priority_queue<ChunkRequest>    requests;
    std::priority_queue<MeshRequest>     meshRequests;
    std::priority_queue<LODMeshRequest>  lodMeshRequests;

    // Completed result queues (ready to be consumed by the main thread)
    std::queue<GeneratedChunk> finished;
    std::queue<MeshResult>     finishedMeshes;
    std::queue<LODMeshResult>  finishedLODMeshes;
};
