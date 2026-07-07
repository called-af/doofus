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

//  Terrain generation

struct GeneratedChunk {
    std::unique_ptr<Chunk> chunk;
    uint32_t generation;
};

struct ChunkRequest {
    int x, z, priority;
    uint32_t generation;
    bool operator<(const ChunkRequest& o) const { return priority > o.priority; }
};

//  Regular mesh (full-detail chunk)

struct MeshRequest {
    Chunk* chunk;
    int priority;
    uint32_t generation;
    
    std::shared_ptr<Chunk> mainChunk;
    std::shared_ptr<Chunk> nNX;
    std::shared_ptr<Chunk> nPX;
    std::shared_ptr<Chunk> nNZ;
    std::shared_ptr<Chunk> nPZ;

    bool operator<(const MeshRequest& o) const { return priority > o.priority; } 
};

struct MeshResult {
    Chunk* chunk;
    std::vector<float> vertices;
    uint32_t generation;
};

//  LOD mesh generation
//
//  ChunkWorker receives LODMeshRequest and produces LODMeshResult.
//  The mesher uses blockQuery and heightQuery callbacks provided by World
//  so ChunkWorker does not need to know about World internals.

struct LODMeshRequest {
    // Tile coordinate and LOD level
    int tileX = 0;
    int tileZ = 0;
    int level  = 1;

    // Unique key for this tile used for result map lookup
    long long key = 0;

    uint32_t generation = 0;

    // Callbacks from World — thread-safe read-only data access
    std::function<BlockType(int, int, int)> blockQuery;
    std::function<int(int, int)>             heightQuery;

    bool operator<(const LODMeshRequest& o) const { return level < o.level; }
};

struct LODMeshResult {
    long long key = 0;
    int tileX = 0;
    int tileZ = 0;
    int level  = 1;
    std::vector<float> vertices;
    uint32_t generation = 0;
};

//  ChunkWorker

class ChunkWorker {
public:
    explicit ChunkWorker(World* world);
    ~ChunkWorker();

    // Terrain
    void requestChunk(int x, int z, int priority, uint32_t generation);
    bool popFinishedChunk(GeneratedChunk& result);

    // Regular mesh
    void enqueueMeshRequest(MeshRequest req);
    bool popFinishedMesh(MeshResult& result);

    // LOD mesh
    void enqueueLODMeshRequest(LODMeshRequest req);
    bool popFinishedLODMesh(LODMeshResult& result);

    void clearRequests();
    void flushFinished();

    std::atomic<uint32_t> generation = 0;
    void nextGeneration() { generation++; }

private:
    void run();

    World* world;
    std::vector<std::thread> workers;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> running = true;

    std::priority_queue<ChunkRequest>    requests;
    std::priority_queue<MeshRequest>     meshRequests;
    std::priority_queue<LODMeshRequest>  lodMeshRequests;

    std::queue<GeneratedChunk> finished;
    std::queue<MeshResult>     finishedMeshes;
    std::queue<LODMeshResult>  finishedLODMeshes;
};
