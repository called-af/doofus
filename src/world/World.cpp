#include "World.h"
#include "../core/Setting.h"
#include "../renderer/Frustum.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <glad/gl.h>
#include <SDL3/SDL.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor & Destructor
// ─────────────────────────────────────────────────────────────────────────────

World::World() { worker = std::make_unique<ChunkWorker>(this); }

World::~World() = default;

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
    return ((long long)(level & 0xF) << 60)
         | (((long long)(unsigned int)tileZ & 0x0FFFFFFF) << 30)
         |  ((long long)(unsigned int)tileX & 0x0FFFFFFF);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chunk access
// ─────────────────────────────────────────────────────────────────────────────

Chunk* World::getChunk(int chunkX, int chunkZ)
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

void World::markChunkDirty(Chunk* chunk)
{
    if (!chunk || chunk->dirty) return;
    chunk->dirty = true;
    remeshQueue.push_back(getChunkKey(chunk->chunkX, chunk->chunkZ));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load chunk — submit a generation request to the worker if not already queued
// ─────────────────────────────────────────────────────────────────────────────

void World::loadChunk(int chunkX, int chunkZ, glm::vec3 cameraPos,
                      glm::vec3 cameraFront, const Frustum& frustum,
                      bool isLoading)
{
    long long key = getChunkKey(chunkX, chunkZ);

    // Already in memory
    if (chunks.contains(key)) return;

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
    if (shaderID == cachedShaderID) return; // already cached for this shader
    cachedShaderID = shaderID;
    uIsLODLoc    = glGetUniformLocation(shaderID, "uIsLOD");
    uTimeLoc     = glGetUniformLocation(shaderID, "uTime");
    uSpawnTimeLoc = glGetUniformLocation(shaderID, "uLodSpawnTime");
}

// ─────────────────────────────────────────────────────────────────────────────
//  inLODRing — check whether a tile falls within the correct LOD ring for its level
//
//  A level-L tile covers (2^L × 2^L) chunks. tileX/tileZ are tile coordinates
//  (not chunk coordinates). A tile is considered valid when its Chebyshev
//  distance from the player falls within [lodLStart, lodLEnd).
// ─────────────────────────────────────────────────────────────────────────────

bool World::inLODRing(int tileX, int tileZ, int level,
                      int playerChunkX, int playerChunkZ) const
{
    int cov = (1 << level); // chunks per tile side

    // Tile centre in chunk units
    float cx = tileX * cov + cov * 0.5f;
    float cz = tileZ * cov + cov * 0.5f;

    // Chebyshev distance from tile centre to player (in chunk units)
    float dx   = std::abs(cx - playerChunkX);
    float dz   = std::abs(cz - playerChunkZ);
    float dist = std::max(dx, dz);

    int startDist, endDist;
    switch (level) {
        case 1: startDist = Setting::lod1Start; endDist = Setting::lod1End; break;
        case 2: startDist = Setting::lod2Start; endDist = Setting::lod2End; break;
        case 3: startDist = Setting::lod3Start; endDist = Setting::lod3End; break;
        case 4: startDist = Setting::lod4Start; endDist = Setting::lod4End; break;
        case 5: startDist = Setting::lod5Start; endDist = Setting::lod5End; break;
        default: return false;
    }

    return dist >= startDist && dist < endDist;
}

// ─────────────────────────────────────────────────────────────────────────────
//  requestLODTile — submit a LOD mesh build request to the worker thread
// ─────────────────────────────────────────────────────────────────────────────

void World::requestLODTile(int tileX, int tileZ, int level)
{
    long long key = getLODKey(tileX, tileZ, level);

    // Mesh already exists
    if (lodChunks.count(key)) return;
    // Already queued
    if (queuedLODTiles.count(key)) return;

    queuedLODTiles[key] = {tileX, tileZ, level};

    LODMeshRequest req;
    req.tileX      = tileX;
    req.tileZ      = tileZ;
    req.level      = level;
    req.key        = key;
    req.generation = worker->lodGeneration.load();

    // blockQuery: read chunk data read-only (safe across threads after generate())
    req.blockQuery = [this](int wx, int wy, int wz) -> BlockType {
        int cx = (int)std::floor((float)wx / Chunk::SIZE);
        int cz = (int)std::floor((float)wz / Chunk::SIZE);
        Chunk* ch = getChunk(cx, cz);
        if (!ch) return BlockType::Air;
        int lx = wx - cx * Chunk::SIZE;
        int lz = wz - cz * Chunk::SIZE;
        if (lx < 0) lx += Chunk::SIZE;
        if (lz < 0) lz += Chunk::SIZE;
        if (wy < 0 || wy >= Chunk::HEIGHT) return BlockType::Air;
        return ch->blocks[lx][wy][lz];
    };

    // heightQuery: use the heightMap already cached on the Chunk
    req.heightQuery = [this](int wx, int wz) -> int {
        int cx = (int)std::floor((float)wx / Chunk::SIZE);
        int cz = (int)std::floor((float)wz / Chunk::SIZE);
        Chunk* ch = getChunk(cx, cz);
        if (!ch) return -1;
        int lx = wx - cx * Chunk::SIZE;
        int lz = wz - cz * Chunk::SIZE;
        if (lx < 0) lx += Chunk::SIZE;
        if (lz < 0) lz += Chunk::SIZE;
        return ch->heightMap[lx][lz];
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
//    2. Iterate every level, request tiles that should exist but don't yet
//    3. Receive finished meshes from the worker and upload them to the GPU
//    4. Clean up queued tiles that have moved outside their ring
// ─────────────────────────────────────────────────────────────────────────────

void World::updateLOD(int playerChunkX, int playerChunkZ,
                      glm::vec3 cameraPos, const Frustum& frustum)
{
    // ── Step 1: remove tiles that have left their ring ────────────────────
    for (auto it = lodChunks.begin(); it != lodChunks.end(); ) {
        auto& lc = it->second;
        if (!inLODRing(lc->tileX, lc->tileZ, lc->level, playerChunkX, playerChunkZ)) {
            queuedLODTiles.erase(it->first);
            it = lodChunks.erase(it);
        } else {
            ++it;
        }
    }

    // ── Step 2: iterate 5 levels, request tiles that should be present ────
    for (int level = 1; level <= 5; level++) {
        int startDist, endDist;
        switch (level) {
            case 1: startDist = Setting::lod1Start; endDist = Setting::lod1End; break;
            case 2: startDist = Setting::lod2Start; endDist = Setting::lod2End; break;
            case 3: startDist = Setting::lod3Start; endDist = Setting::lod3End; break;
            case 4: startDist = Setting::lod4Start; endDist = Setting::lod4End; break;
            case 5: startDist = Setting::lod5Start; endDist = Setting::lod5End; break;
            default: continue;
        }

        int cov       = (1 << level); // chunks per tile side
        int tileRange = (endDist / cov) + 2; // slight overestimate; inLODRing will filter
        int playerTileX = (int)std::floor((float)playerChunkX / cov);
        int playerTileZ = (int)std::floor((float)playerChunkZ / cov);

        for (int dtx = -tileRange; dtx <= tileRange; dtx++) {
            for (int dtz = -tileRange; dtz <= tileRange; dtz++) {
                int tx = playerTileX + dtx;
                int tz = playerTileZ + dtz;

                if (!inLODRing(tx, tz, level, playerChunkX, playerChunkZ))
                    continue;

                // Don't request a tile if its underlying terrain data isn't available yet.
                // Check at least one chunk at a corner of the tile (fast heuristic).
                int chunkX0    = tx * cov;
                int chunkZ0    = tz * cov;
                bool hasAnyChunk = false;
                int  step        = std::max(1, cov / 2);
                for (int dcx = 0; dcx < cov && !hasAnyChunk; dcx += step)
                    for (int dcz = 0; dcz < cov && !hasAnyChunk; dcz += step)
                        if (getChunk(chunkX0 + dcx, chunkZ0 + dcz))
                            hasAnyChunk = true;
                if (!hasAnyChunk) continue;

                requestLODTile(tx, tz, level);
            }
        }
    }

    // ── Step 3: receive results from the worker, upload to GPU ────────────
    LODMeshResult result;
    while (worker->popFinishedLODMesh(result)) {
        // Discard stale results
        if (result.generation != worker->lodGeneration.load()) {
            queuedLODTiles.erase(result.key);
            continue;
        }

        queuedLODTiles.erase(result.key);

        // Create or update the LODChunk
        auto it = lodChunks.find(result.key);
        if (it == lodChunks.end()) {
            auto lc = std::make_shared<LODChunk>(result.tileX, result.tileZ, result.level);
            lc->pendingVertices = std::move(result.vertices);
            lc->uploadMesh();
            lodChunks[result.key] = std::move(lc);
        } else {
            it->second->pendingVertices = std::move(result.vertices);
            it->second->uploadMesh();
        }
    }

    // ── Step 4: clean up queued tiles that have left their ring ──────────
    for (auto it = queuedLODTiles.begin(); it != queuedLODTiles.end(); ) {
        auto& [tx, tz, lvl] = it->second;
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
                   const Frustum& frustum, bool isLoading)
{
    // Player's chunk coordinates (or (0,0) during the loading screen)
    int playerChunkX = isLoading ? 0 : (int)std::floor(cameraPos.x / Chunk::SIZE);
    int playerChunkZ = isLoading ? 0 : (int)std::floor(cameraPos.z / Chunk::SIZE);

    // ── Detect player chunk movement ──────────────────────────────────────
    static int lastChunkX = INT_MAX;
    static int lastChunkZ = INT_MAX;

    if (playerChunkX != lastChunkX || playerChunkZ != lastChunkZ) {
        // Bump the generation so stale results still in the queue are ignored
        worker->nextGeneration();
        worker->clearRequests();
        worker->flushFinished();
        queuedChunks.clear();
        remeshQueue.clear();

        // Mark every loaded chunk as needing a remesh
        for (auto& [key, chunk] : chunks) {
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

    // ── Bump LOD generation only when the player crosses a level-1 tile boundary ─
    // (a level-1 tile = 2 chunks per side), the coarsest movement that can
    // invalidate LOD.
    static int lastLodTileX = INT_MAX, lastLodTileZ = INT_MAX;
    int lodTileX = (int)std::floor((float)playerChunkX / 2);
    int lodTileZ = (int)std::floor((float)playerChunkZ / 2);
    if (lodTileX != lastLodTileX || lodTileZ != lastLodTileZ) {
        worker->nextLODGeneration();
        lastLodTileX = lodTileX;
        lastLodTileZ = lodTileZ;
    }

    // ── Request terrain generation within render distance ─────────────────
    int rd = Setting::renderDistance;
    for (int x = -rd; x <= rd; x++) {
        for (int z = -rd; z <= rd; z++) {
            if (x * x + z * z > rd * rd) continue;
            loadChunk(playerChunkX + x, playerChunkZ + z,
                      cameraPos, cameraFront, frustum, isLoading);
        }
    }

    // ── Receive finished terrain results from the worker ──────────────────
    GeneratedChunk genResult;
    while (worker->popFinishedChunk(genResult)) {
        if (genResult.generation != worker->generation.load()) continue;

        int cx  = genResult.chunk->chunkX;
        int cz  = genResult.chunk->chunkZ;
        long long key = getChunkKey(cx, cz);

        chunks[key] = std::move(genResult.chunk);
        queuedChunks.erase(key);

        // Mark this chunk and its 4 neighbours as needing a remesh
        for (auto [ncx, ncz] : std::initializer_list<std::pair<int,int>>{
                 {cx,   cz  },
                 {cx-1, cz  },
                 {cx+1, cz  },
                 {cx,   cz-1},
                 {cx,   cz+1},
             })
        {
            Chunk* n = getChunk(ncx, ncz);
            if (!n) continue;
            n->dirty = false;
            markChunkDirty(n);
        }
    }

    // ── Dispatch mesh requests (batched to avoid frame hitches) ───────────
    // Sort the remesh queue by priority (closest + in-frustum first)
    std::sort(remeshQueue.begin(), remeshQueue.end(),
        [&](long long a, long long b) {
            auto itA = chunks.find(a);
            auto itB = chunks.find(b);
            if (itA == chunks.end() || itB == chunks.end()) return false;
            int pA = calculatePriority(itA->second->chunkX, itA->second->chunkZ,
                                       cameraPos, cameraFront, frustum, isLoading);
            int pB = calculatePriority(itB->second->chunkX, itB->second->chunkZ,
                                       cameraPos, cameraFront, frustum, isLoading);
            return pA < pB;
        });

    // Maximum dispatches per frame is controlled by Setting::maxMeshDispatchPerFrame
    const int MAX_DISPATCH = Setting::maxMeshDispatchPerFrame;
    int dispatched = 0;
    std::vector<long long> requeue;

    for (long long key : remeshQueue) {
        auto it = chunks.find(key);
        if (it == chunks.end()) continue;
        auto chunk = it->second;
        if (!chunk->dirty) continue;

        // Skip chunks that are outside render distance
        int dx = std::abs(chunk->chunkX - playerChunkX);
        int dz = std::abs(chunk->chunkZ - playerChunkZ);
        if (dx > rd || dz > rd) {
            chunk->dirty = false;
            continue;
        }

        // All 4 neighbours are required for correct cross-chunk face culling
        auto nNX = getChunkShared(chunk->chunkX - 1, chunk->chunkZ);
        auto nPX = getChunkShared(chunk->chunkX + 1, chunk->chunkZ);
        auto nNZ = getChunkShared(chunk->chunkX,     chunk->chunkZ - 1);
        auto nPZ = getChunkShared(chunk->chunkX,     chunk->chunkZ + 1);

        if (!nNX || !nPX || !nNZ || !nPZ) {
            requeue.push_back(key); // neighbours not ready yet, retry next frame
            continue;
        }

        if (dispatched >= MAX_DISPATCH) {
            requeue.push_back(key); // dispatch cap reached for this frame
            continue;
        }

        MeshRequest meshReq;
        meshReq.chunk      = chunk.get();
        meshReq.priority   = calculatePriority(chunk->chunkX, chunk->chunkZ,
                                               cameraPos, cameraFront, frustum, isLoading);
        meshReq.generation = worker->generation.load();
        meshReq.mainChunk  = chunk;
        meshReq.nNX = nNX; meshReq.nPX = nPX;
        meshReq.nNZ = nNZ; meshReq.nPZ = nPZ;

        worker->enqueueMeshRequest(std::move(meshReq));
        dispatched++;
    }

    remeshQueue = std::move(requeue);

    // ── Receive finished meshes from the worker, upload to GPU ───────────
    MeshResult meshResult;
    while (worker->popFinishedMesh(meshResult)) {
        if (meshResult.generation != worker->generation.load()) continue;

        Chunk* chunk = meshResult.chunk;
        chunk->pendingVertices = std::move(meshResult.vertices);
        chunk->empty.store(chunk->pendingVertices.empty());
        chunk->uploadMesh();
        chunk->dirty = false;
    }

    // ── Update LOD — only when not on the loading screen ─────────────────
    if (!isLoading) {
        updateLOD(playerChunkX, playerChunkZ, cameraPos, frustum);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  draw — render full-detail regular chunks
// ─────────────────────────────────────────────────────────────────────────────

void World::draw(float playerX, float playerZ, const Frustum& frustum, GLuint shaderID)
{
    // Cache uniform locations once per shader (avoids glGetUniformLocation every frame)
    cacheUniformLocations(shaderID);

    glUniform1i(uIsLODLoc, 0);
    glUniform1f(uTimeLoc, (float)(SDL_GetTicks() / 1000.0));

    int playerChunkX = (int)std::floor(playerX / Chunk::SIZE);
    int playerChunkZ = (int)std::floor(playerZ / Chunk::SIZE);
    int rd           = Setting::renderDistance;

    for (auto& [key, chunk] : chunks) {
        // Skip chunks outside render distance
        int dx = std::abs(chunk->chunkX - playerChunkX);
        int dz = std::abs(chunk->chunkZ - playerChunkZ);
        if (dx > rd || dz > rd) continue;

        if (chunk->empty)  continue;
        if (!chunk->mesh)  continue;

        // Frustum culling — skip chunks not visible to the camera
        if (!frustum.isBoxVisible(chunk->getMinBounds(), chunk->getMaxBounds()))
            continue;

        // Send spawn time for the fade-in animation
        glUniform1f(uSpawnTimeLoc, chunk->spawnTime);
        chunk->draw();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  drawLOD — render low-resolution LOD tiles (far-range coverage)
// ─────────────────────────────────────────────────────────────────────────────

void World::drawLOD(const Frustum& frustum, GLuint shaderID)
{
    // Cache uniform locations once per shader
    cacheUniformLocations(shaderID);

    // uIsLOD=1 tells the shader to skip shadow lookups
    glUniform1i(uIsLODLoc, 1);
    glUniform1f(uTimeLoc, (float)(SDL_GetTicks() / 1000.0));

    for (auto& [key, lc] : lodChunks) {
        if (lc->empty)  continue;
        if (!lc->mesh)  continue;

        // Per-tile frustum culling
        if (!frustum.isBoxVisible(lc->minBounds, lc->maxBounds)) continue;

        // Send spawn time for the tile fade-in animation
        glUniform1f(uSpawnTimeLoc, lc->spawnTime);
        lc->draw();
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
    Chunk* chunk = getChunk((int)std::floor((float)x / Chunk::SIZE),
                            (int)std::floor((float)z / Chunk::SIZE));
    if (!chunk) return false;

    int lx = x % Chunk::SIZE; if (lx < 0) lx += Chunk::SIZE;
    int lz = z % Chunk::SIZE; if (lz < 0) lz += Chunk::SIZE;

    if (y < 0 || y >= Chunk::HEIGHT) return false;
    return chunk->blocks[lx][y][lz] != BlockType::Air;
}

int World::getHeight(int x, int z)
{
    for (int y = Chunk::HEIGHT - 1; y >= 0; y--)
        if (isSolid(x, y, z)) return y;
    return 0;
}

void World::setBlock(int x, int y, int z, BlockType type)
{
    int chunkX = (int)std::floor((float)x / Chunk::SIZE);
    int chunkZ = (int)std::floor((float)z / Chunk::SIZE);
    Chunk* chunk = getChunk(chunkX, chunkZ);
    if (!chunk) return;

    int lx = x % Chunk::SIZE; if (lx < 0) lx += Chunk::SIZE;
    int lz = z % Chunk::SIZE; if (lz < 0) lz += Chunk::SIZE;
    if (y < 0 || y >= Chunk::HEIGHT) return;

    chunk->blocks[lx][y][lz] = type;
    markChunkDirty(chunk);

    // Mark neighbours dirty if the block sits on a chunk border
    if (lx == 0)             if (auto* n = getChunk(chunkX-1, chunkZ)) markChunkDirty(n);
    if (lx == Chunk::SIZE-1) if (auto* n = getChunk(chunkX+1, chunkZ)) markChunkDirty(n);
    if (lz == 0)             if (auto* n = getChunk(chunkX, chunkZ-1)) markChunkDirty(n);
    if (lz == Chunk::SIZE-1) if (auto* n = getChunk(chunkX, chunkZ+1)) markChunkDirty(n);
}

// ─────────────────────────────────────────────────────────────────────────────
//  unloadFarChunks — remove chunks that are too far from the player
// ─────────────────────────────────────────────────────────────────────────────

void World::unloadFarChunks(int centerChunkX, int centerChunkZ)
{
    const int UNLOAD_DISTANCE = Setting::renderDistance + 2;

    for (auto it = chunks.begin(); it != chunks.end(); ) {
        Chunk* chunk = it->second.get();
        int dx = std::abs(chunk->chunkX - centerChunkX);
        int dz = std::abs(chunk->chunkZ - centerChunkZ);

        if (dx > UNLOAD_DISTANCE || dz > UNLOAD_DISTANCE)
            it = chunks.erase(it);
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
                             glm::vec3 cameraFront, const Frustum& frustum,
                             bool isLoading)
{
    if (isLoading) {
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
    float dist    = glm::distance(cameraPos, chunkCenter);
    int   priority = static_cast<int>(dist * 100.0f);

    if (frustum.isBoxVisible(minBounds, maxBounds)) {
        // Inside frustum: apply a bonus based on how far off-centre the chunk is
        glm::vec3 toChunk = chunkCenter - cameraPos;
        float len = glm::length(toChunk);
        float dot = 0.0f;
        if (len > 0.001f) {
            toChunk /= len;
            dot = glm::dot(cameraFront, toChunk);
        }
        float crosshairFactor = 1.0f - dot; // 0 = dead ahead, 2 = behind
        priority += static_cast<int>(crosshairFactor * 500.0f);
    } else {
        // Outside frustum: large penalty so it is processed last
        priority += 15000;
    }

    return priority;
}
