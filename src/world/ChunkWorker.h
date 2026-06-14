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
    uint32_t generation;
};

struct ChunkRequest {
    int x, z, priority;
    uint32_t generation;
    bool operator<(const ChunkRequest& o) const { return priority > o.priority; }
};

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

class ChunkWorker {
public:
    explicit ChunkWorker(World* world);
    ~ChunkWorker();

    void requestChunk(int x, int z, int priority, uint32_t generation);
    bool popFinishedChunk(GeneratedChunk& result);

    void enqueueMeshRequest(MeshRequest req);
    bool popFinishedMesh(MeshResult& result);

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

    std::priority_queue<ChunkRequest> requests;
    std::priority_queue<MeshRequest> meshRequests;

    std::queue<GeneratedChunk> finished;
    std::queue<MeshResult> finishedMeshes;
}; 