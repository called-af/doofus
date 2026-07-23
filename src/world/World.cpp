#include "World.h"
#include "TerrainGenerator.h"
#include "../core/Setting.h"
#include "../renderer/Frustum.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <glad/gl.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor & Destructor
// ─────────────────────────────────────────────────────────────────────────────

World::World() { worker = std::make_unique<ChunkWorker>(this); }

World::~World()
{
    for (auto &[key, query] : occlusionQueries)
    {
        OcclusionCulling::destroy(query);
    }
    for (auto &[key, query] : lodOcclusionQueries)
    {
        OcclusionCulling::destroy(query);
    }

    if (occlusionVBO != 0)
        glDeleteBuffers(1, &occlusionVBO);
    if (occlusionVAO != 0)
        glDeleteVertexArrays(1, &occlusionVAO);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chunk & LOD tile keys
// ─────────────────────────────────────────────────────────────────────────────

// Encode chunk coordinates into a single long long (unique key for unordered_map)
long long World::getChunkKey(int x, int z)
{
    return ((long long)(unsigned int)x << 32) | (unsigned int)z;
}

// Encode a LOD tile into a single long long
// Bit layout: [level 4bit | tileZ 30bit | tileX 30bit]
long long World::getLODKey(int tileX, int tileZ, int level)
{
    return ((long long)(level & 0xF) << 60) | (((long long)(unsigned int)tileZ & 0x0FFFFFFF) << 30) | ((long long)(unsigned int)tileX & 0x0FFFFFFF);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chunk access
// ─────────────────────────────────────────────────────────────────────────────

Chunk *World::getChunk(int chunkX, int chunkZ)
{
    long long key = getChunkKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    return it != chunks.end() ? it->second.get() : nullptr;
}

std::shared_ptr<Chunk> World::getChunkShared(int chunkX, int chunkZ)
{
    std::shared_lock lock(chunksMutex);
    long long key = getChunkKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    return it != chunks.end() ? it->second : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mark a chunk as needing a remesh
// ─────────────────────────────────────────────────────────────────────────────

void World::markChunkDirty(Chunk *chunk)
{
    if (!chunk || chunk->dirty)
        return;
    chunk->dirty = true;
    remeshQueue.push_back(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load chunk — submit a generation request to the worker if not already queued
// ─────────────────────────────────────────────────────────────────────────────

void World::loadChunk(int chunkX, int chunkZ, glm::vec3 cameraPos,
                      glm::vec3 cameraFront, const Frustum &frustum,
                      bool isLoading)
{
    long long key = getChunkKey(chunkX, chunkZ);

    // Do this before a generation job is created.  Priority alone still made
    // workers spend CPU generating the entire circle behind the camera.
    if (!isLoading)
    {
        const glm::vec3 minBounds(chunkX * Chunk::SIZE, 0.0f,
                                  chunkZ * Chunk::SIZE);
        const glm::vec3 maxBounds = minBounds + glm::vec3(
                                                    (float)Chunk::SIZE, (float)Chunk::HEIGHT, (float)Chunk::SIZE);
        if (!frustum.isBoxVisible(minBounds, maxBounds))
            return;
    }

    // Already in memory
    if (chunks.contains(key))
        return;

    // Already queued with the same generation
    auto it = queuedChunks.find(key);
    if (it != queuedChunks.end() && it->second == worker->generation.load())
        return;

    queuedChunks[key] = worker->generation.load();

    int priority = calculatePriority(chunkX, chunkZ, cameraPos,
                                     cameraFront, frustum, isLoading);
    worker->requestChunk(chunkX, chunkZ, priority, worker->generation.load());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cache shader uniform locations — avoid glGetUniformLocation every frame
// ─────────────────────────────────────────────────────────────────────────────

void World::cacheUniformLocations(GLuint shaderID)
{
    if (shaderID == cachedShaderID)
        return; // already cached for this shader
    cachedShaderID = shaderID;
    uIsLODLoc = glGetUniformLocation(shaderID, "uIsLOD");
    uTimeLoc = glGetUniformLocation(shaderID, "uTime");
    uSpawnTimeLoc = glGetUniformLocation(shaderID, "uLodSpawnTime");
}

// ─────────────────────────────────────────────────────────────────────────────
//  inLODRing — check whether a tile falls within the correct LOD ring for its level
//
//  A level-L tile covers (2^L × 2^L) chunks. tileX/tileZ are tile coordinates
//  (not chunk coordinates). A tile is considered valid when its radial
//  distance from the player falls within [lodLStart, lodLEnd).
// ─────────────────────────────────────────────────────────────────────────────

bool World::inLODRing(int tileX, int tileZ, int level,
                      int playerChunkX, int playerChunkZ) const
{
    const int cov = (1 << level); // chunks per tile side

    // Tile bounds in chunk coordinates
    const float minX = (float)(tileX * cov);
    const float maxX = (float)((tileX + 1) * cov);
    const float minZ = (float)(tileZ * cov);
    const float maxZ = (float)((tileZ + 1) * cov);

    // Find closest point on the tile to the player chunk
    const float pX = (float)playerChunkX;
    const float pZ = (float)playerChunkZ;
    const float closestX = std::max(minX, std::min(pX, maxX));
    const float closestZ = std::max(minZ, std::min(pZ, maxZ));
    const float dxClosest = closestX - pX;
    const float dzClosest = closestZ - pZ;
    const float minDistSquared = dxClosest * dxClosest + dzClosest * dzClosest;

    // Find furthest point on the tile to the player chunk
    const float furthestX = (std::abs(minX - pX) > std::abs(maxX - pX)) ? minX : maxX;
    const float furthestZ = (std::abs(minZ - pZ) > std::abs(maxZ - pZ)) ? minZ : maxZ;
    const float dxFurthest = furthestX - pX;
    const float dzFurthest = furthestZ - pZ;
    const float maxDistSquared = dxFurthest * dxFurthest + dzFurthest * dzFurthest;

    int startDist, endDist;
    switch (level)
    {
    case 1:
        startDist = Setting::lod1Start;
        endDist = Setting::lod1End;
        break;
    case 2:
        startDist = Setting::lod2Start;
        endDist = Setting::lod2End;
        break;
    case 3:
        startDist = Setting::lod3Start;
        endDist = Setting::lod3End;
        break;
    case 4:
        startDist = Setting::lod4Start;
        endDist = Setting::lod4End;
        break;
    case 5:
        startDist = Setting::lod5Start;
        endDist = Setting::lod5End;
        break;
    default:
        return false;
    }

    const float startDistSq = (float)(startDist * startDist);
    const float endDistSq = (float)(endDist * endDist);

    return maxDistSquared >= startDistSq && minDistSquared < endDistSq;
}

// ─────────────────────────────────────────────────────────────────────────────
//  requestLODTile — submit a LOD mesh build request to the worker thread
// ─────────────────────────────────────────────────────────────────────────────

void World::requestLODTile(int tileX, int tileZ, int level)
{
    long long key = getLODKey(tileX, tileZ, level);

    // Mesh already exists
    if (lodChunks.count(key))
        return;
    // Already queued
    if (queuedLODTiles.count(key))
        return;

    queuedLODTiles[key] = {tileX, tileZ, level};

    LODMeshRequest req;
    req.tileX = tileX;
    req.tileZ = tileZ;
    req.level = level;
    req.key = key;
    req.generation = worker->lodGeneration.load();

    // Never make a far ring depend on the full-detail cache: LOD1 begins
    // outside renderDistance, so waiting for those chunks left intermittent
    // holes.  The deterministic sampler also keeps every LOD boundary stable.
    req.blockQuery = [](int wx, int wy, int wz) -> BlockType
    {
        return TerrainGenerator::sampleBlockAt(wx, wz, wy);
    };
    req.heightQuery = [level](int wx, int wz) -> int
    {
        return TerrainGenerator::sampleLODHeightAt(wx, wz, level);
    };

    worker->enqueueLODMeshRequest(std::move(req));
}

// ─────────────────────────────────────────────────────────────────────────────
//  updateLOD — manage the LOD tile registry every frame (5 levels)
//
//  Ring logic mirrors CDLOD QuadTree::selectNodes:
//    Level 1: ring from lod1Start to lod1End (in chunk units)
//    Level 2: ring from lod2Start to lod2End
//    Level 3: ring from lod3Start to lod3End
//    Level 4: ring from lod4Start to lod4End
//    Level 5: ring from lod5Start to lod5End
//
//  Each frame:
//    1. Remove tiles that have moved outside their ring
//    2. Periodically request tiles that should exist but do not yet
//    3. Receive finished meshes from the worker and upload them to the GPU
//    4. Clean up queued tiles that have moved outside their ring
// ─────────────────────────────────────────────────────────────────────────────

void World::updateLOD(int playerChunkX, int playerChunkZ, const Frustum &frustum,
                      bool isLoading, bool refreshRequests)
{
    // ── Step 1: remove tiles that have left their ring ────────────────────
    for (auto it = lodChunks.begin(); it != lodChunks.end();)
    {
        auto &lc = it->second;
        if (!inLODRing(lc->tileX, lc->tileZ, lc->level, playerChunkX, playerChunkZ))
        {
            queuedLODTiles.erase(it->first);
            if (auto queryIt = lodOcclusionQueries.find(it->first);
                queryIt != lodOcclusionQueries.end())
            {
                OcclusionCulling::destroy(queryIt->second);
                lodOcclusionQueries.erase(queryIt);
            }
            it = lodChunks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // ── Step 2: request missing tiles on a fixed cadence ──────────────────
    // The registry itself is maintained every frame, but scanning all five
    // rings is amortized to keep gameplay frame times stable.
    if (refreshRequests)
    {
        for (int level = 1; level <= 5; level++)
        {
            int startDist, endDist;
            switch (level)
            {
            case 1:
                startDist = Setting::lod1Start;
                endDist = Setting::lod1End;
                break;
            case 2:
                startDist = Setting::lod2Start;
                endDist = Setting::lod2End;
                break;
            case 3:
                startDist = Setting::lod3Start;
                endDist = Setting::lod3End;
                break;
            case 4:
                startDist = Setting::lod4Start;
                endDist = Setting::lod4End;
                break;
            case 5:
                startDist = Setting::lod5Start;
                endDist = Setting::lod5End;
                break;
            default:
                continue;
            }

            const int cov = (1 << level);              // chunks per tile side
            const int tileRange = (endDist / cov) + 2; // slight overestimate; inLODRing will filter
            const int playerTileX = (int)std::floor((float)playerChunkX / cov);
            const int playerTileZ = (int)std::floor((float)playerChunkZ / cov);

            for (int dtx = -tileRange; dtx <= tileRange; dtx++)
            {
                for (int dtz = -tileRange; dtz <= tileRange; dtz++)
                {
                    const int tx = playerTileX + dtx;
                    const int tz = playerTileZ + dtz;

                    if (!inLODRing(tx, tz, level, playerChunkX, playerChunkZ))
                        continue;

                    const float tileSize = (float)(cov * Chunk::SIZE);
                    const glm::vec3 minBounds(tx * tileSize, 0.0f, tz * tileSize);
                    const glm::vec3 maxBounds = minBounds + glm::vec3(
                                                                tileSize, (float)Chunk::HEIGHT, tileSize);
                    // Loading used to enqueue the complete 1200-chunk circle.
                    // Those thousands of unseen LOD jobs kept CPU cores busy
                    // after gameplay began and caused movement hitches.
                    if (!frustum.isBoxVisible(minBounds, maxBounds))
                        continue;

                    requestLODTile(tx, tz, level);
                }
            }
        }
    }

    // ── Step 3: receive results from the worker, upload to GPU ────────────
    LODMeshResult result;
    while (worker->popFinishedLODMesh(result))
    {
        // Discard stale results
        if (result.generation != worker->lodGeneration.load())
        {
            queuedLODTiles.erase(result.key);
            continue;
        }

        queuedLODTiles.erase(result.key);

        // The tile may have left its ring while its worker job was running.
        // Do not resurrect it; this is the lifecycle generation guard.
        if (!inLODRing(result.tileX, result.tileZ, result.level,
                       playerChunkX, playerChunkZ))
            continue;

        // Create or update the LODChunk
        auto it = lodChunks.find(result.key);
        if (it == lodChunks.end())
        {
            auto lc = std::make_shared<LODChunk>(result.tileX, result.tileZ, result.level);
            lc->pendingVertices = std::move(result.vertices);
            lc->uploadMesh();
            lodChunks[result.key] = std::move(lc);
        }
        else
        {
            it->second->pendingVertices = std::move(result.vertices);
            it->second->uploadMesh();
        }
    }

    // ── Step 4: clean up queued tiles that have left their ring ──────────
    for (auto it = queuedLODTiles.begin(); it != queuedLODTiles.end();)
    {
        auto &[tx, tz, lvl] = it->second;
        if (!inLODRing(tx, tz, lvl, playerChunkX, playerChunkZ))
            it = queuedLODTiles.erase(it);
        else
            ++it;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  update — called every frame from the main thread
// ─────────────────────────────────────────────────────────────────────────────

void World::update(glm::vec3 cameraPos, glm::vec3 cameraFront,
                   const Frustum &frustum, bool isLoading)
{
    ++worldUpdateFrame;
    // Player's chunk coordinates (or (0,0) during the loading screen)
    int playerChunkX = isLoading ? 0 : (int)std::floor(cameraPos.x / Chunk::SIZE);
    int playerChunkZ = isLoading ? 0 : (int)std::floor(cameraPos.z / Chunk::SIZE);

    // ── Detect player chunk movement ──────────────────────────────────────
    const bool playerChunkChanged = playerChunkX != lastChunkX || playerChunkZ != lastChunkZ;

    if (playerChunkChanged)
    {
        // Bump the generation so stale results still in the queue are ignored
        worker->nextGeneration();
        worker->clearRequests();
        worker->flushFinished();
        queuedChunks.clear();
        remeshQueue.clear();

        // Mark every loaded chunk as needing a remesh
        for (auto &[key, chunk] : chunks)
        {
            chunk->dirty = false;
            if (!chunk->mesh || chunk->empty.load())
                markChunkDirty(chunk.get());
        }

        lastChunkX = playerChunkX;
        lastChunkZ = playerChunkZ;
        // NOTE: LOD state (lodChunks / queuedLODTiles) is intentionally left intact.
        // updateLOD() already removes tiles that leave their ring every frame —
        // that is the correct granularity. Do not wipe the LOD cache here.
    }

    // ── Track tile movement without invalidating the complete LOD cache ────
    // Tiles are immutable deterministic samples.  updateLOD removes only a
    // tile that fully leaves its own ring, so crossing a small chunk boundary
    // does not throw away hundreds of useful worker results.
    int lodTileX = (int)std::floor((float)playerChunkX / 2);
    int lodTileZ = (int)std::floor((float)playerChunkZ / 2);
    if (lodTileX != lastLodTileX || lodTileZ != lastLodTileZ)
    {
        lastLodTileX = lodTileX;
        lastLodTileZ = lodTileZ;
    }

    // ── Request terrain generation within render distance ─────────────────
    const int rd = Setting::renderDistance;
    const int genRd = rd + 1;
    // Revisit the frustum periodically.  Chunks behind the player are not
    // generated, but turning must promptly enqueue the newly visible wedge.
    constexpr unsigned int chunkRequestRefreshFrames = 8;
    const bool refreshChunkRequests = playerChunkChanged || rd != lastRequestedRenderDistance || worldUpdateFrame % chunkRequestRefreshFrames == 0;
    if (refreshChunkRequests)
    {
        for (int x = -genRd; x <= genRd; x++)
        {
            for (int z = -genRd; z <= genRd; z++)
            {
                if (x * x + z * z > genRd * genRd)
                    continue;
                loadChunk(playerChunkX + x, playerChunkZ + z,
                          cameraPos, cameraFront, frustum, isLoading);
            }
        }
        lastRequestedRenderDistance = rd;
        unloadFarChunks(playerChunkX, playerChunkZ);
    }

    // ── Receive finished terrain results from the worker ──────────────────
    bool receivedChunk = false;
    GeneratedChunk genResult;
    while (worker->popFinishedChunk(genResult))
    {
        if (genResult.generation != worker->generation.load())
            continue;

        int cx = genResult.chunk->chunkX;
        int cz = genResult.chunk->chunkZ;
        long long key = getChunkKey(cx, cz);

        chunks[key] = std::move(genResult.chunk);
        queuedChunks.erase(key);
        receivedChunk = true;

        // Mark this chunk and its 4 neighbours as needing a remesh
        for (auto [ncx, ncz] : std::initializer_list<std::pair<int, int>>{
                 {cx, cz},
                 {cx - 1, cz},
                 {cx + 1, cz},
                 {cx, cz - 1},
                 {cx, cz + 1},
             })
        {
            Chunk *n = getChunk(ncx, ncz);
            if (!n)
                continue;
            n->dirty = false;
            markChunkDirty(n);
        }
    }

    // ── Dispatch mesh requests (batched to avoid frame hitches) ───────────
    // Sort the remesh queue by priority (closest + in-frustum first)
    struct PrioritySortedKey
    {
        int priority;
        long long key;
    };
    std::vector<PrioritySortedKey> sortedQueue;
    sortedQueue.reserve(remeshQueue.size());

    for (long long key : remeshQueue)
    {
        auto it = chunks.find(key);
        if (it == chunks.end())
            continue;
        int p = calculatePriority(it->second->chunkX, it->second->chunkZ,
                                  cameraPos, cameraFront, frustum, isLoading);
        sortedQueue.push_back({p, key});
    }

    std::sort(sortedQueue.begin(), sortedQueue.end(),
              [](const PrioritySortedKey &a, const PrioritySortedKey &b)
              {
                  return a.priority < b.priority;
              });

    remeshQueue.clear();
    for (const auto &item : sortedQueue)
    {
        remeshQueue.push_back(item.key);
    }

    // Maximum dispatches per frame is controlled by Setting::maxMeshDispatchPerFrame
    const int MAX_DISPATCH = Setting::maxMeshDispatchPerFrame;
    int dispatched = 0;
    std::vector<long long> requeue;

    for (long long key : remeshQueue)
    {
        auto it = chunks.find(key);
        if (it == chunks.end())
            continue;
        auto chunk = it->second;
        if (!chunk->dirty)
            continue;

        // Skip chunks that are outside render distance
        int dx = std::abs(chunk->chunkX - playerChunkX);
        int dz = std::abs(chunk->chunkZ - playerChunkZ);
        if (dx > rd || dz > rd)
        {
            chunk->dirty = false;
            continue;
        }

        // Keep mesh work for visible terrain. Chunks outside the camera frustum
        // remain dirty and are dispatched immediately when the player turns.
        if (!isLoading && !frustum.isBoxVisible(chunk->getMinBounds(),
                                                chunk->getMaxBounds()))
        {
            requeue.push_back(key);
            continue;
        }

        // All 4 neighbours are required for correct cross-chunk face culling
        auto nNX = getChunkShared(chunk->chunkX - 1, chunk->chunkZ);
        auto nPX = getChunkShared(chunk->chunkX + 1, chunk->chunkZ);
        auto nNZ = getChunkShared(chunk->chunkX, chunk->chunkZ - 1);
        auto nPZ = getChunkShared(chunk->chunkX, chunk->chunkZ + 1);

        if (!nNX || !nPX || !nNZ || !nPZ)
        {
            requeue.push_back(key); // neighbours not ready yet, retry next frame
            continue;
        }

        if (dispatched >= MAX_DISPATCH)
        {
            requeue.push_back(key); // dispatch cap reached for this frame
            continue;
        }

        MeshRequest meshReq;
        meshReq.chunk = chunk.get();
        meshReq.priority = calculatePriority(chunk->chunkX, chunk->chunkZ,
                                             cameraPos, cameraFront, frustum, isLoading);
        meshReq.generation = worker->generation.load();
        meshReq.mainChunk = chunk;
        meshReq.nNX = nNX;
        meshReq.nPX = nPX;
        meshReq.nNZ = nNZ;
        meshReq.nPZ = nPZ;

        worker->enqueueMeshRequest(std::move(meshReq));
        dispatched++;
    }

    remeshQueue = std::move(requeue);

    // ── Receive finished meshes from the worker, upload to GPU ───────────
    MeshResult meshResult;
    while (worker->popFinishedMesh(meshResult))
    {
        if (meshResult.generation != worker->generation.load())
            continue;

        Chunk *chunk = meshResult.chunk;
        chunk->pendingVertices = std::move(meshResult.vertices);
        chunk->empty.store(chunk->pendingVertices.empty());
        chunk->uploadMesh();
        chunk->dirty = false;

        // A remesh can change the visible surface, so do not reuse its old result.
        const long long key = getChunkKey(chunk->chunkX, chunk->chunkZ);
        if (auto queryIt = occlusionQueries.find(key); queryIt != occlusionQueries.end())
        {
            OcclusionCulling::invalidate(queryIt->second);
        }
    }

    // ── Update LOD — active during initial world loading as well ──────────
    // This requests rear and side rings before the player can move.
    constexpr unsigned int lodRequestRefreshFrames = 8;
    const bool refreshLODRequests = playerChunkChanged || (isLoading && receivedChunk) || worldUpdateFrame - lastLODRequestRefreshFrame >= lodRequestRefreshFrames;
    if (refreshLODRequests)
        lastLODRequestRefreshFrame = worldUpdateFrame;
    updateLOD(playerChunkX, playerChunkZ, frustum, isLoading, refreshLODRequests);
}

// ─────────────────────────────────────────────────────────────────────────────
//  draw — render full-detail regular chunks
// ─────────────────────────────────────────────────────────────────────────────

void World::invalidateOcclusion(const glm::vec3 &cameraPos,
                                const glm::vec3 &cameraFront)
{
    constexpr float cameraMoveThresholdSquared = 16.0f;
    constexpr float cameraDirectionThreshold = 0.98f;

    const glm::vec3 cameraDelta = cameraPos - lastOcclusionCameraPos;
    const bool cameraMoved = !occlusionCameraValid || glm::dot(cameraDelta, cameraDelta) > cameraMoveThresholdSquared || glm::dot(cameraFront, lastOcclusionCameraFront) < cameraDirectionThreshold;
    if (!cameraMoved)
        return;

    for (auto &[key, query] : occlusionQueries)
    {
        OcclusionCulling::invalidate(query);
    }
    for (auto &[key, query] : lodOcclusionQueries)
    {
        OcclusionCulling::invalidate(query);
    }

    lastOcclusionCameraPos = cameraPos;
    lastOcclusionCameraFront = cameraFront;
    occlusionCameraValid = true;
}

bool World::shouldDrawChunk(OcclusionQuery &query)
{
    return OcclusionCulling::poll(query);
}

bool World::isOcclusionTestDue(const OcclusionQuery &query) const
{
    return OcclusionCulling::isTestDue(query, renderFrame);
}

void World::ensureOcclusionResources()
{
    if (occlusionVAO != 0)
        return;

    constexpr float boxVertices[] = {
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        1.0f,
        0.0f,
        0.0f,
    };

    occlusionShader = std::make_unique<Shader>("assets/shaders/occlusion.vert",
                                               "assets/shaders/occlusion.frag");
    uOcclusionViewProjectionLoc = glGetUniformLocation(occlusionShader->id, "uViewProjection");
    uOcclusionModelLoc = glGetUniformLocation(occlusionShader->id, "uModel");

    glGenVertexArrays(1, &occlusionVAO);
    glGenBuffers(1, &occlusionVBO);
    glBindVertexArray(occlusionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, occlusionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(boxVertices), boxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void World::issueOcclusionQuery(OcclusionQuery &query,
                                const glm::vec3 &minBounds,
                                const glm::vec3 &maxBounds,
                                const glm::mat4 &viewProjection, GLuint terrainShaderID)
{
    ensureOcclusionResources();
    if (query.id == 0)
        glGenQueries(1, &query.id);

    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean colorMask[4];
    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

    const glm::vec3 size = maxBounds - minBounds;
    const glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), minBounds), size);

    occlusionShader->use();
    glUniformMatrix4fv(uOcclusionViewProjectionLoc, 1, GL_FALSE, &viewProjection[0][0]);
    glUniformMatrix4fv(uOcclusionModelLoc, 1, GL_FALSE, &model[0][0]);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    glBeginQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, query.id);
    glBindVertexArray(occlusionVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glEndQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE);

    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glDepthMask(depthMask);
    if (blendEnabled)
        glEnable(GL_BLEND);
    if (cullEnabled)
        glEnable(GL_CULL_FACE);
    glUseProgram(terrainShaderID);

    query.pending = true;
    query.discardPendingResult = false;
    query.lastTestFrame = renderFrame;
}

void World::draw(const glm::vec3 &cameraPos, const glm::vec3 &cameraFront,
                 const Frustum &frustum, const glm::mat4 &viewProjection,
                 GLuint shaderID)
{
    // Cache uniform locations once per shader (avoids glGetUniformLocation every frame)
    cacheUniformLocations(shaderID);

    glUniform1i(uIsLODLoc, 0);
    glUniform1f(uTimeLoc, (float)(SDL_GetTicks() / 1000.0));

    ++renderFrame;
    invalidateOcclusion(cameraPos, cameraFront);

    const int playerChunkX = (int)std::floor(cameraPos.x / Chunk::SIZE);
    const int playerChunkZ = (int)std::floor(cameraPos.z / Chunk::SIZE);
    const int rd = Setting::renderDistance;
    struct DistanceSortedChunk
    {
        float distanceSq;
        long long key;
        Chunk *chunk;
    };
    std::vector<DistanceSortedChunk> visibleChunks;
    visibleChunks.reserve(chunks.size());

    for (auto &[key, chunk] : chunks)
    {
        const int dx = std::abs(chunk->chunkX - playerChunkX);
        const int dz = std::abs(chunk->chunkZ - playerChunkZ);
        if (dx > rd || dz > rd || chunk->empty || !chunk->mesh)
            continue;
        if (!frustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds()))
            continue;

        const glm::vec3 center = (chunk->getMinBounds() + chunk->getMaxBounds()) * 0.5f;
        const glm::vec3 delta = center - cameraPos;
        const float distSq = glm::dot(delta, delta);
        visibleChunks.push_back({distSq, key, chunk.get()});
    }

    // Draw near terrain first so depth testing rejects hidden fragments early.
    std::sort(visibleChunks.begin(), visibleChunks.end(),
              [](const DistanceSortedChunk &a, const DistanceSortedChunk &b)
              {
                  return a.distanceSq < b.distanceSq;
              });

    constexpr int minimumOcclusionDistance = 2;
    int issuedQueries = 0;
    constexpr int maximumQueriesPerFrame = 32;

    for (const auto &item : visibleChunks)
    {
        Chunk *chunk = item.chunk;
        long long key = item.key;
        const int chunkDistance = std::max(
            std::abs(chunk->chunkX - playerChunkX),
            std::abs(chunk->chunkZ - playerChunkZ));
        if (chunkDistance < minimumOcclusionDistance)
        {
            glUniform1f(uSpawnTimeLoc, chunk->spawnTime);
            chunk->draw();
            continue;
        }

        OcclusionQuery &query = occlusionQueries[key];
        if (!shouldDrawChunk(query) && !isOcclusionTestDue(query))
            continue;

        if (issuedQueries < maximumQueriesPerFrame && isOcclusionTestDue(query))
        {
            // Query the conservative AABB before drawing this chunk, so only
            // already rendered nearer terrain can occlude it.
            glm::vec3 minBounds = chunk->getMinBounds();
            // Regular chunks can rise by up to eight blocks while spawning.
            // Keep the proxy larger than the animated mesh to avoid false culls.
            minBounds.y -= 8.0f;
            issueOcclusionQuery(query, minBounds, chunk->getMaxBounds(),
                                viewProjection, shaderID);
            ++issuedQueries;
        }

        if (!query.visible)
            continue;

        glUniform1f(uSpawnTimeLoc, chunk->spawnTime);
        chunk->draw();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawLOD — render low-resolution LOD tiles (far-range coverage)
// ─────────────────────────────────────────────────────────────────────────────

void World::drawLOD(const glm::vec3 &cameraPos, const Frustum &frustum,
                    const glm::mat4 &viewProjection, GLuint shaderID)
{
    // Cache uniform locations once per shader
    cacheUniformLocations(shaderID);

    // uIsLOD=1 tells the shader to skip shadow lookups
    glUniform1i(uIsLODLoc, 1);
    glUniform1f(uTimeLoc, (float)(SDL_GetTicks() / 1000.0));

    struct DistanceSortedTile
    {
        float distanceSq;
        long long key;
        LODChunk *tile;
    };
    std::vector<DistanceSortedTile> visibleTiles;
    visibleTiles.reserve(lodChunks.size());

    for (auto &[key, lc] : lodChunks)
    {
        if (lc->empty)
            continue;
        if (!lc->mesh)
            continue;
        if (!frustum.isBoxVisible(lc->minBounds, lc->maxBounds))
            continue;

        const glm::vec3 center = (lc->minBounds + lc->maxBounds) * 0.5f;
        const glm::vec3 delta = center - cameraPos;
        const float distSq = glm::dot(delta, delta);
        visibleTiles.push_back({distSq, key, lc.get()});
    }

    // Process near tiles first so they populate depth before the conservative
    // occlusion proxy tests for farther LOD levels.
    std::sort(visibleTiles.begin(), visibleTiles.end(),
              [](const DistanceSortedTile &a, const DistanceSortedTile &b)
              {
                  return a.distanceSq < b.distanceSq;
              });

    int issuedQueries = 0;
    constexpr int maximumQueriesPerFrame = 32;

    for (const auto &item : visibleTiles)
    {
        LODChunk *tile = item.tile;
        long long key = item.key;
        OcclusionQuery &query = lodOcclusionQueries[key];
        if (!shouldDrawChunk(query) && !isOcclusionTestDue(query))
            continue;

        if (issuedQueries < maximumQueriesPerFrame && isOcclusionTestDue(query))
        {
            glm::vec3 minBounds = tile->minBounds;
            // LOD tiles can rise by up to eighteen blocks during their spawn animation.
            minBounds.y -= 18.0f;
            issueOcclusionQuery(query, minBounds, tile->maxBounds,
                                viewProjection, shaderID);
            ++issuedQueries;
        }

        if (!query.visible)
            continue;

        // Send spawn time for the tile fade-in animation
        glUniform1f(uSpawnTimeLoc, tile->spawnTime);
        tile->draw();
    }

    // Reset to a clean state after LOD rendering is complete
    glUniform1i(uIsLODLoc, 0);
    glUniform1f(uSpawnTimeLoc, -1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Block operations
// ─────────────────────────────────────────────────────────────────────────────

bool World::isSolid(int x, int y, int z)
{
    Chunk *chunk = getChunk((int)std::floor((float)x / Chunk::SIZE),
                            (int)std::floor((float)z / Chunk::SIZE));
    if (!chunk)
        return false;

    int lx = x % Chunk::SIZE;
    if (lx < 0)
        lx += Chunk::SIZE;
    int lz = z % Chunk::SIZE;
    if (lz < 0)
        lz += Chunk::SIZE;

    if (y < 0 || y >= Chunk::HEIGHT)
        return false;
    return chunk->blocks[lx][y][lz] != BlockType::Air;
}

int World::getHeight(int x, int z)
{
    for (int y = Chunk::HEIGHT - 1; y >= 0; y--)
        if (isSolid(x, y, z))
            return y;
    return 0;
}

void World::setBlock(int x, int y, int z, BlockType type)
{
    int chunkX = (int)std::floor((float)x / Chunk::SIZE);
    int chunkZ = (int)std::floor((float)z / Chunk::SIZE);
    Chunk *chunk = getChunk(chunkX, chunkZ);
    if (!chunk)
        return;

    int lx = x % Chunk::SIZE;
    if (lx < 0)
        lx += Chunk::SIZE;
    int lz = z % Chunk::SIZE;
    if (lz < 0)
        lz += Chunk::SIZE;
    if (y < 0 || y >= Chunk::HEIGHT)
        return;

    chunk->blocks[lx][y][lz] = type;
    markChunkDirty(chunk);

    // Mark neighbours dirty if the block sits on a chunk border
    if (lx == 0)
        if (auto *n = getChunk(chunkX - 1, chunkZ))
            markChunkDirty(n);
    if (lx == Chunk::SIZE - 1)
        if (auto *n = getChunk(chunkX + 1, chunkZ))
            markChunkDirty(n);
    if (lz == 0)
        if (auto *n = getChunk(chunkX, chunkZ - 1))
            markChunkDirty(n);
    if (lz == Chunk::SIZE - 1)
        if (auto *n = getChunk(chunkX, chunkZ + 1))
            markChunkDirty(n);
}

// ─────────────────────────────────────────────────────────────────────────────
//  unloadFarChunks — remove chunks that are too far from the player
// ─────────────────────────────────────────────────────────────────────────────

void World::unloadFarChunks(int centerChunkX, int centerChunkZ)
{
    const int UNLOAD_DISTANCE = Setting::renderDistance + 2;

    for (auto it = chunks.begin(); it != chunks.end();)
    {
        Chunk *chunk = it->second.get();
        int dx = std::abs(chunk->chunkX - centerChunkX);
        int dz = std::abs(chunk->chunkZ - centerChunkZ);

        if (dx > UNLOAD_DISTANCE || dz > UNLOAD_DISTANCE)
        {
            const long long key = it->first;
            it = chunks.erase(it);

            auto queryIt = occlusionQueries.find(key);
            if (queryIt != occlusionQueries.end())
            {
                OcclusionCulling::destroy(queryIt->second);
                occlusionQueries.erase(queryIt);
            }
        }
        else
            ++it;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  calculatePriority — compute generation/mesh priority for a chunk
//
//  Lower value = higher priority.
//  Factors:
//    - Distance from the player (closer = higher priority)
//    - Whether the chunk is inside the camera frustum (in-frustum = higher priority)
//    - View direction (directly in front of the camera = highest priority)
// ─────────────────────────────────────────────────────────────────────────────

int World::calculatePriority(int chunkX, int chunkZ, glm::vec3 cameraPos,
                             glm::vec3 cameraFront, const Frustum &frustum,
                             bool isLoading)
{
    if (isLoading)
    {
        // During loading: prioritise chunks closest to the origin (0,0)
        int dx = chunkX - 0;
        int dz = chunkZ - 0;
        return (dx * dx + dz * dz);
    }

    glm::vec3 minBounds = glm::vec3(chunkX * Chunk::SIZE, 0, chunkZ * Chunk::SIZE);
    glm::vec3 maxBounds = glm::vec3(chunkX * Chunk::SIZE + Chunk::SIZE,
                                    Chunk::HEIGHT,
                                    chunkZ * Chunk::SIZE + Chunk::SIZE);
    glm::vec3 chunkCenter = (minBounds + maxBounds) * 0.5f;

    // Base priority: distance from the camera
    float dist = glm::distance(cameraPos, chunkCenter);
    int priority = static_cast<int>(dist * 100.0f);

    if (frustum.isBoxVisible(minBounds, maxBounds))
    {
        // Inside frustum: apply a bonus based on how far off-centre the chunk is
        glm::vec3 toChunk = chunkCenter - cameraPos;
        float len = glm::length(toChunk);
        float dot = 0.0f;
        if (len > 0.001f)
        {
            toChunk /= len;
            dot = glm::dot(cameraFront, toChunk);
        }
        float crosshairFactor = 1.0f - dot; // 0 = dead ahead, 2 = behind
        priority += static_cast<int>(crosshairFactor * 500.0f);
    }
    else
    {
        // Outside frustum: large penalty so it is processed last
        priority += 15000;
    }

    return priority;
}
