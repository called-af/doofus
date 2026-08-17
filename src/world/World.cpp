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

//  Constructor & Destructor

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

// Encode chunk coordinates into a single long long (unique key for unordered_map)
long long World::getChunkKey(int x, int z)
{
    return ((long long)(unsigned int)x << 32) | (unsigned int)z;
}

//  Chunk access

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

//  Mark a chunk as needing a remesh

void World::markChunkDirty(Chunk *chunk)
{
    if (!chunk || chunk->dirty)
        return;
    chunk->dirty = true;
    remeshQueue.push_back(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

//  Load chunk — submit a generation request to the worker if not already queued

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

//  Cache shader uniform locations — avoid glGetUniformLocation every frame

void World::cacheUniformLocations(GLuint shaderID)
{
    if (shaderID == cachedShaderID)
        return; // already cached for this shader
    cachedShaderID = shaderID;
    uTimeLoc = glGetUniformLocation(shaderID, "uTime");
    uIsLODLoc = glGetUniformLocation(shaderID, "uIsLOD");
    uSpawnTimeLoc = glGetUniformLocation(shaderID, "uLodSpawnTime");
}

//  update — called every frame from the main thread

void World::update(glm::vec3 cameraPos, glm::vec3 cameraFront,
                   const Frustum &frustum, bool isLoading)
{
    ++worldUpdateFrame;
    // Player's chunk coordinates (or (0,0) during the loading screen)
    int playerChunkX = isLoading ? 0 : (int)std::floor(cameraPos.x / Chunk::SIZE);
    int playerChunkZ = isLoading ? 0 : (int)std::floor(cameraPos.z / Chunk::SIZE);

    //  Detect player chunk movement 
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
    }

    //  Request terrain generation within render distance ─
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

    //  Receive finished terrain results from the worker 
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

    //  Dispatch mesh requests (batched to avoid frame hitches) ─
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

        // Skip chunks that are outside render distance (circular check)
        int dx = chunk->chunkX - playerChunkX;
        int dz = chunk->chunkZ - playerChunkZ;
        if (dx * dx + dz * dz > rd * rd)
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

    //  Receive finished meshes from the worker, upload to GPU ─
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

    //  LOD system update ─
    lastLodTileX = playerChunkX;
    lastLodTileZ = playerChunkZ;

    constexpr unsigned int lodRequestRefreshFrames = 4;
    const bool refreshLODRequests = playerChunkChanged || worldUpdateFrame % lodRequestRefreshFrames == 0;
    updateLOD(playerChunkX, playerChunkZ, cameraPos, cameraFront, frustum, isLoading, refreshLODRequests);
}

//  draw — render full-detail regular chunks

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

    glUniform1f(uTimeLoc, (float)(SDL_GetTicks() / 1000.0));

    ++renderFrame;
    invalidateOcclusion(cameraPos, cameraFront);

    const int playerChunkX = (int)std::floor(cameraPos.x / Chunk::SIZE);
    const int playerChunkZ = (int)std::floor(cameraPos.z / Chunk::SIZE);
    const int rd = Setting::renderDistance;
    const int rdSq = rd * rd;
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
        const int dx = chunk->chunkX - playerChunkX;
        const int dz = chunk->chunkZ - playerChunkZ;
        if (dx * dx + dz * dz > rdSq || chunk->empty || !chunk->mesh)
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
        const int dx = chunk->chunkX - playerChunkX;
        const int dz = chunk->chunkZ - playerChunkZ;
        if (dx * dx + dz * dz < minimumOcclusionDistance * minimumOcclusionDistance)
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

void World::drawShadowChunks(const Frustum &lightFrustum, GLint uSpawnTimeLoc,
                             int playerChunkX, int playerChunkZ, int maxDistance)
{
    const int maxDistSq = maxDistance * maxDistance;
    for (auto &[key, chunk] : chunks)
    {
        if (!chunk || chunk->empty || !chunk->mesh)
            continue;

        const int dx = chunk->chunkX - playerChunkX;
        const int dz = chunk->chunkZ - playerChunkZ;
        if (dx * dx + dz * dz > maxDistSq)
            continue;

        if (!lightFrustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds()))
            continue;

        glUniform1f(uSpawnTimeLoc, chunk->spawnTime);
        chunk->mesh->draw();
    }
}

//  Block operations

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

//  unloadFarChunks — remove chunks that are too far from the player

void World::unloadFarChunks(int centerChunkX, int centerChunkZ)
{
    const int UNLOAD_DISTANCE = Setting::renderDistance + 2;
    const int unloadDistSq = UNLOAD_DISTANCE * UNLOAD_DISTANCE;

    for (auto it = chunks.begin(); it != chunks.end();)
    {
        Chunk *chunk = it->second.get();
        int dx = chunk->chunkX - centerChunkX;
        int dz = chunk->chunkZ - centerChunkZ;

        if (dx * dx + dz * dz > unloadDistSq)
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

//  calculatePriority — compute generation/mesh priority for a chunk
//
//  Lower value = higher priority.
//  Factors:
//    - Distance from the player (closer = higher priority)
//    - Whether the chunk is inside the camera frustum (in-frustum = higher priority)
//    - View direction (directly in front of the camera = highest priority)

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

// ─────────────────────────────────────────────────────────────────────────────
//  LOD System Implementation
// ─────────────────────────────────────────────────────────────────────────────

long long World::getLODKey(int tileX, int tileZ, int level)
{
    uint64_t ux = (uint64_t)(uint32_t)tileX & 0x3FFFFFFFULL;
    uint64_t uz = (uint64_t)(uint32_t)tileZ & 0x3FFFFFFFULL;
    uint64_t ul = (uint64_t)(uint32_t)level & 0xFULL;
    return (long long)((ul << 60) | (uz << 30) | ux);
}

bool World::inLODRing(int tileX, int tileZ, int level,
                      int playerChunkX, int playerChunkZ) const
{
    const int maxLevel = std::min(5, Setting::maxLODLevel);
    if (level < 1 || level > maxLevel || Setting::maxLODLevel <= 0)
        return false;

    const int cov = 1 << (level - 1);
    const float playerCenterX = (float)playerChunkX + 0.5f;
    const float playerCenterZ = (float)playerChunkZ + 0.5f;
    const float centerX = (tileX + 0.5f) * (float)cov;
    const float centerZ = (tileZ + 0.5f) * (float)cov;
    const float dx = centerX - playerCenterX;
    const float dz = centerZ - playerCenterZ;
    const float dist = std::sqrt(dx * dx + dz * dz);

    const float maxDist = (float)Setting::getLODMaxChunkDistance(level);
    const float minDist = (level == 1)
                              ? (float)Setting::renderDistance
                              : (float)Setting::getLODMinChunkDistance(level);

    return dist <= (maxDist + 1.0f) && (level == 1 || dist >= (minDist - (float)cov));
}

int World::calculateLODPriority(int tileX, int tileZ, int level,
                                glm::vec3 cameraPos, glm::vec3 cameraFront,
                                const Frustum &frustum, bool isLoading)
{
    const int cov = 1 << (level - 1);
    const glm::vec3 minBounds(tileX * cov * Chunk::SIZE, 0.0f, tileZ * cov * Chunk::SIZE);
    const glm::vec3 maxBounds((tileX + 1) * cov * Chunk::SIZE, (float)Chunk::HEIGHT, (tileZ + 1) * cov * Chunk::SIZE);
    const glm::vec3 tileCenter = (minBounds + maxBounds) * 0.5f;

    if (isLoading)
    {
        int dx = tileX * cov;
        int dz = tileZ * cov;
        return (dx * dx + dz * dz) + (level * 500);
    }

    float dist = glm::distance(cameraPos, tileCenter);
    int priority = static_cast<int>(dist * 20.0f) + (level * 400);

    if (frustum.isBoxVisible(minBounds, maxBounds))
    {
        glm::vec3 toTile = tileCenter - cameraPos;
        float len = glm::length(toTile);
        float dot = 0.0f;
        if (len > 0.001f)
        {
            toTile /= len;
            dot = glm::dot(cameraFront, toTile);
        }
        float crosshairFactor = 1.0f - dot; // 0.0 = center of crosshair, 2.0 = behind
        priority += static_cast<int>(crosshairFactor * 1500.0f);
    }
    else
    {
        priority += 30000; // heavy penalty for tiles outside camera view
    }

    return priority;
}

void World::requestLODTile(int tileX, int tileZ, int level, int priority)
{
    long long key = getLODKey(tileX, tileZ, level);
    queuedLODTiles[key] = {tileX, tileZ, level};

    LODMeshRequest req;
    req.tileX = tileX;
    req.tileZ = tileZ;
    req.level = level;
    req.priority = priority;
    req.key = key;
    req.generation = worker->lodGeneration.load();

    req.solidQuery = [](int wx, int wy, int wz) {
        return TerrainGenerator::isSolidAt(wx, wz, wy);
    };
    req.heightQuery = [](int wx, int wz) {
        return TerrainGenerator::sampleHeightAt(wx, wz);
    };
    req.blockQuery = [](int wx, int wy, int wz) {
        return TerrainGenerator::sampleBlockAt(wx, wz, wy);
    };

    worker->enqueueLODMeshRequest(std::move(req));
}

void World::updateLOD(int playerChunkX, int playerChunkZ, glm::vec3 cameraPos,
                      glm::vec3 cameraFront, const Frustum &frustum,
                      bool isLoading, bool refreshRequests)
{
    // 1. Receive completed LOD results from worker (budgeted to avoid frame hitches)
    constexpr int MAX_LOD_UPLOADS_PER_FRAME = 24;
    int uploaded = 0;
    LODMeshResult lodResult;
    while (uploaded < MAX_LOD_UPLOADS_PER_FRAME && worker->popFinishedLODMesh(lodResult))
    {
        if (lodResult.generation != worker->lodGeneration.load())
            continue;

        long long key = lodResult.key;
        queuedLODTiles.erase(key);

        LODTile tile;
        tile.tileX = lodResult.tileX;
        tile.tileZ = lodResult.tileZ;
        tile.level = lodResult.level;
        tile.empty = lodResult.vertices.empty();
        tile.spawnTime = (float)(SDL_GetTicks() / 1000.0);

        int cov = 1 << (tile.level - 1);
        tile.minBounds = glm::vec3(tile.tileX * cov * Chunk::SIZE, 0.0f, tile.tileZ * cov * Chunk::SIZE);
        tile.maxBounds = glm::vec3((tile.tileX + 1) * cov * Chunk::SIZE, (float)Chunk::HEIGHT, (tile.tileZ + 1) * cov * Chunk::SIZE);

        if (!tile.empty)
        {
            tile.mesh = std::make_unique<Mesh>(lodResult.vertices.data(), (unsigned int)(lodResult.vertices.size() * sizeof(float)));
        }

        lodTiles[key] = std::move(tile);
        ++uploaded;
    }

    // 2. Unload far LOD tiles that are outside the maximum LOD distance buffer
    // Never unload inner tiles just because the player is close — retain them for smooth transitions & caching!
    const int maxLevel = std::min(5, Setting::maxLODLevel);
    const int maxChunkDist = (maxLevel > 0) ? Setting::getLODMaxChunkDistance(maxLevel) : Setting::renderDistance;
    constexpr int UNLOAD_CHUNK_BUFFER = 8;
    const float maxUnloadDistSq = (float)((maxChunkDist + UNLOAD_CHUNK_BUFFER) * (maxChunkDist + UNLOAD_CHUNK_BUFFER));
    const float playerCenterX = (float)playerChunkX + 0.5f;
    const float playerCenterZ = (float)playerChunkZ + 0.5f;

    for (auto it = lodTiles.begin(); it != lodTiles.end();)
    {
        const auto &tile = it->second;
        if (tile.level > maxLevel || Setting::maxLODLevel <= 0)
        {
            auto qIt = lodOcclusionQueries.find(it->first);
            if (qIt != lodOcclusionQueries.end())
            {
                OcclusionCulling::destroy(qIt->second);
                lodOcclusionQueries.erase(qIt);
            }
            it = lodTiles.erase(it);
            continue;
        }

        int cov = 1 << (tile.level - 1);
        float centerX = (tile.tileX + 0.5f) * (float)cov;
        float centerZ = (tile.tileZ + 0.5f) * (float)cov;
        float dx = centerX - playerCenterX;
        float dz = centerZ - playerCenterZ;

        if (dx * dx + dz * dz > maxUnloadDistSq)
        {
            auto qIt = lodOcclusionQueries.find(it->first);
            if (qIt != lodOcclusionQueries.end())
            {
                OcclusionCulling::destroy(qIt->second);
                lodOcclusionQueries.erase(qIt);
            }
            it = lodTiles.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Periodically clean up stale entries in queuedLODTiles
    if (worldUpdateFrame % 60 == 0)
    {
        for (auto it = queuedLODTiles.begin(); it != queuedLODTiles.end();)
        {
            int tx = it->second[0];
            int tz = it->second[1];
            int lvl = it->second[2];
            int cov = 1 << (lvl - 1);
            float cx = (tx + 0.5f) * (float)cov;
            float cz = (tz + 0.5f) * (float)cov;
            float dx = cx - playerCenterX;
            float dz = cz - playerCenterZ;
            if (dx * dx + dz * dz > maxUnloadDistSq)
                it = queuedLODTiles.erase(it);
            else
                ++it;
        }
    }

    // 3. Scan rings and enqueue missing tiles with directional priority
    if (refreshRequests && Setting::maxLODLevel > 0)
    {
        for (int level = 1; level <= maxLevel; ++level)
        {
            const int cov = 1 << (level - 1);
            const float maxChunkDistF = (float)Setting::getLODMaxChunkDistance(level);
            const float minChunkDistF = (level == 1) ? (float)Setting::renderDistance : (float)Setting::getLODMinChunkDistance(level);

            const int tileRadius = (int)std::ceil(maxChunkDistF / (float)cov) + 1;
            const int playerTileX = chunkToTile(playerChunkX, level);
            const int playerTileZ = chunkToTile(playerChunkZ, level);

            for (int dx = -tileRadius; dx <= tileRadius; ++dx)
            {
                for (int dz = -tileRadius; dz <= tileRadius; ++dz)
                {
                    int tx = playerTileX + dx;
                    int tz = playerTileZ + dz;

                    float centerX = (tx + 0.5f) * (float)cov;
                    float centerZ = (tz + 0.5f) * (float)cov;
                    float cdx = centerX - playerCenterX;
                    float cdz = centerZ - playerCenterZ;
                    float dist = std::sqrt(cdx * cdx + cdz * cdz);

                    if (dist > maxChunkDistF + 1.0f)
                        continue;
                    if (level > 1 && dist < minChunkDistF - (float)cov)
                        continue;

                    long long key = getLODKey(tx, tz, level);
                    if (lodTiles.contains(key) || queuedLODTiles.contains(key))
                        continue;

                    // Frustum culling check
                    glm::vec3 minB(tx * cov * Chunk::SIZE, 0.0f, tz * cov * Chunk::SIZE);
                    glm::vec3 maxB((tx + 1) * cov * Chunk::SIZE, (float)Chunk::HEIGHT, (tz + 1) * cov * Chunk::SIZE);

                    if (!isLoading && !frustum.isBoxVisible(minB, maxB))
                        continue;

                    int priority = calculateLODPriority(tx, tz, level, cameraPos, cameraFront, frustum, isLoading);
                    requestLODTile(tx, tz, level, priority);
                }
            }
        }
    }
}

bool World::isLODSubtreeReady(int tx, int tz, int level, int playerChunkX, int playerChunkZ) const
{
    if (level == 1)
    {
        float dx = (float)tx + 0.5f - ((float)playerChunkX + 0.5f);
        float dz = (float)tz + 0.5f - ((float)playerChunkZ + 0.5f);
        float dist = std::sqrt(dx * dx + dz * dz);

        if (dist <= (float)Setting::renderDistance)
        {
            long long chunkKey = const_cast<World*>(this)->getChunkKey(tx, tz);
            auto it = chunks.find(chunkKey);
            if (it != chunks.end() && it->second && (it->second->mesh != nullptr || it->second->empty.load()))
                return true;
        }

        long long key = getLODKey(tx, tz, 1);
        auto it = lodTiles.find(key);
        if (it != lodTiles.end() && (it->second.mesh != nullptr || it->second.empty))
            return true;

        return false;
    }
    else
    {
        long long key = getLODKey(tx, tz, level);
        auto it = lodTiles.find(key);
        if (it != lodTiles.end() && (it->second.mesh != nullptr || it->second.empty))
            return true;

        for (int cx = 0; cx < 2; ++cx)
        {
            for (int cz = 0; cz < 2; ++cz)
            {
                if (!isLODSubtreeReady(2 * tx + cx, 2 * tz + cz, level - 1, playerChunkX, playerChunkZ))
                    return false;
            }
        }
        return true;
    }
}

void World::collectLODTiles(int tx, int tz, int level,
                           int playerChunkX, int playerChunkZ,
                           const glm::vec3 &cameraPos, const Frustum &frustum,
                           std::vector<LODTile*> &outTiles)
{
    const int cov = 1 << (level - 1);
    const glm::vec3 minBounds(tx * cov * Chunk::SIZE, 0.0f, tz * cov * Chunk::SIZE);
    const glm::vec3 maxBounds((tx + 1) * cov * Chunk::SIZE, (float)Chunk::HEIGHT, (tz + 1) * cov * Chunk::SIZE);

    if (!frustum.isBoxVisible(minBounds, maxBounds))
        return;

    const float playerCenterX = (float)playerChunkX + 0.5f;
    const float playerCenterZ = (float)playerChunkZ + 0.5f;
    const float tileCenterX = (tx + 0.5f) * (float)cov;
    const float tileCenterZ = (tz + 0.5f) * (float)cov;
    const float dx = tileCenterX - playerCenterX;
    const float dz = tileCenterZ - playerCenterZ;
    const float dist = std::sqrt(dx * dx + dz * dz);

    const int maxLODLevel = std::min(5, Setting::maxLODLevel);
    if (level == maxLODLevel && dist > (float)Setting::getLODMaxChunkDistance(maxLODLevel))
        return;

    if (level == 1)
    {
        const float rd = (float)Setting::renderDistance;
        if (dist <= rd)
        {
            long long chunkKey = getChunkKey(tx, tz);
            auto it = chunks.find(chunkKey);
            bool chunkReady = (it != chunks.end() && it->second && it->second->mesh != nullptr && !it->second->empty.load());

            if (chunkReady)
            {
                // Full chunk is rendered by World::draw()
                return;
            }
            else
            {
                // Full chunk still meshing; fall back to LOD 1 so terrain doesn't vanish
                long long key = getLODKey(tx, tz, 1);
                auto tileIt = lodTiles.find(key);
                if (tileIt != lodTiles.end() && tileIt->second.mesh && !tileIt->second.empty)
                {
                    outTiles.push_back(&tileIt->second);
                }
                return;
            }
        }
        else
        {
            long long key = getLODKey(tx, tz, 1);
            auto tileIt = lodTiles.find(key);
            if (tileIt != lodTiles.end() && tileIt->second.mesh && !tileIt->second.empty)
            {
                outTiles.push_back(&tileIt->second);
            }
            return;
        }
    }

    // level > 1
    const float subdivideDist = (float)Setting::getLODMaxChunkDistance(level - 1);

    if (dist <= subdivideDist)
    {
        bool childrenReady = true;
        for (int cx = 0; cx < 2; ++cx)
        {
            for (int cz = 0; cz < 2; ++cz)
            {
                if (!isLODSubtreeReady(2 * tx + cx, 2 * tz + cz, level - 1, playerChunkX, playerChunkZ))
                {
                    childrenReady = false;
                    break;
                }
            }
            if (!childrenReady) break;
        }

        if (childrenReady)
        {
            for (int cx = 0; cx < 2; ++cx)
            {
                for (int cz = 0; cz < 2; ++cz)
                {
                    collectLODTiles(2 * tx + cx, 2 * tz + cz, level - 1,
                                    playerChunkX, playerChunkZ,
                                    cameraPos, frustum, outTiles);
                }
            }
        }
        else
        {
            // Children not ready yet; fallback to drawing this parent tile if available
            long long key = getLODKey(tx, tz, level);
            auto it = lodTiles.find(key);
            if (it != lodTiles.end() && it->second.mesh && !it->second.empty)
            {
                outTiles.push_back(&it->second);
            }
            else
            {
                for (int cx = 0; cx < 2; ++cx)
                {
                    for (int cz = 0; cz < 2; ++cz)
                    {
                        collectLODTiles(2 * tx + cx, 2 * tz + cz, level - 1,
                                        playerChunkX, playerChunkZ,
                                        cameraPos, frustum, outTiles);
                    }
                }
            }
        }
    }
    else
    {
        long long key = getLODKey(tx, tz, level);
        auto it = lodTiles.find(key);
        if (it != lodTiles.end() && it->second.mesh && !it->second.empty)
        {
            outTiles.push_back(&it->second);
        }
        else
        {
            bool anyChildInCache = false;
            for (int cx = 0; cx < 2; ++cx)
            {
                for (int cz = 0; cz < 2; ++cz)
                {
                    long long cKey = getLODKey(2 * tx + cx, 2 * tz + cz, level - 1);
                    if (lodTiles.contains(cKey))
                    {
                        anyChildInCache = true;
                        break;
                    }
                }
                if (anyChildInCache) break;
            }

            if (anyChildInCache)
            {
                for (int cx = 0; cx < 2; ++cx)
                {
                    for (int cz = 0; cz < 2; ++cz)
                    {
                        collectLODTiles(2 * tx + cx, 2 * tz + cz, level - 1,
                                        playerChunkX, playerChunkZ,
                                        cameraPos, frustum, outTiles);
                    }
                }
            }
        }
    }
}

void World::drawLOD(const glm::vec3 &cameraPos, const Frustum &frustum,
                    const glm::mat4 &viewProjection, GLuint shaderID)
{
    const int maxLevel = std::min(5, Setting::maxLODLevel);
    if (maxLevel <= 0 || lodTiles.empty())
        return;

    cacheUniformLocations(shaderID);

    glUniform1i(uIsLODLoc, 1);

    const int playerChunkX = (int)std::floor(cameraPos.x / Chunk::SIZE);
    const int playerChunkZ = (int)std::floor(cameraPos.z / Chunk::SIZE);

    const int maxChunkDist = Setting::getLODMaxChunkDistance(maxLevel);
    const int cov = 1 << (maxLevel - 1);
    const int tileRadius = (int)std::ceil((float)maxChunkDist / (float)cov) + 1;
    const int playerTileX = chunkToTile(playerChunkX, maxLevel);
    const int playerTileZ = chunkToTile(playerChunkZ, maxLevel);

    std::vector<LODTile*> tilesToDraw;
    tilesToDraw.reserve(256);

    for (int dx = -tileRadius; dx <= tileRadius; ++dx)
    {
        for (int dz = -tileRadius; dz <= tileRadius; ++dz)
        {
            int tx = playerTileX + dx;
            int tz = playerTileZ + dz;
            collectLODTiles(tx, tz, maxLevel, playerChunkX, playerChunkZ,
                            cameraPos, frustum, tilesToDraw);
        }
    }

    if (tilesToDraw.empty())
    {
        glUniform1i(uIsLODLoc, 0);
        return;
    }

    // Sort front to back for early depth rejection
    struct DistanceSortedLOD
    {
        float distSq;
        LODTile *tile;
    };
    std::vector<DistanceSortedLOD> visibleTiles;
    visibleTiles.reserve(tilesToDraw.size());

    for (LODTile *tile : tilesToDraw)
    {
        if (!tile || tile->empty || !tile->mesh)
            continue;

        glm::vec3 center = (tile->minBounds + tile->maxBounds) * 0.5f;
        glm::vec3 delta = center - cameraPos;
        float dSq = glm::dot(delta, delta);
        visibleTiles.push_back({dSq, tile});
    }

    std::sort(visibleTiles.begin(), visibleTiles.end(),
              [](const DistanceSortedLOD &a, const DistanceSortedLOD &b) {
                  return a.distSq < b.distSq;
              });

    for (const auto &item : visibleTiles)
    {
        LODTile *tile = item.tile;
        glUniform1f(uSpawnTimeLoc, tile->spawnTime);
        tile->mesh->draw();
    }

    glUniform1i(uIsLODLoc, 0);
}

