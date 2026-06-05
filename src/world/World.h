#pragma once

#include "../renderer/Frustum.h"
#include "Chunk.h"
#include "ChunkWorker.h"

#include <queue>
#include <memory>
#include <unordered_map>
#include <unordered_set>

class World {
public:

    World();
    ~World();

    std::unordered_map<
        long long,
        std::unique_ptr<Chunk>
    > chunks;

    std::unordered_set<
        long long
    > queuedChunks;

    std::unique_ptr<
        ChunkWorker
    > worker;

    std::queue<long long> remeshQueue;

    void update(
        float playerX,
        float playerZ
    );

    void draw(
        float playerX,
        float playerZ,
        const Frustum& frustum
    );

    void markChunkDirty(
    Chunk* chunk
);

    bool isSolid(
        int x,
        int y,
        int z
    );

    int getHeight(
        int x,
        int z
    );

    Chunk* getChunk(
        int chunkX,
        int chunkZ
    );

    void loadChunk(
        int chunkX,
        int chunkZ,
        int playerChunkX,
        int playerChunkZ
    );

    void unloadFarChunks(
        int centerChunkX,
        int centerChunkZ
    );

    long long getChunkKey(
        int x,
        int z
    );

    void setBlock(
        int x,
        int y,
        int z,
        BlockType type
    );
};