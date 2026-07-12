#pragma once

#include "../renderer/Frustum.h"
#include "Chunk.h"
#include "ChunkWorker.h"
#include "LODChunk.h"
#include "../renderer/opengl/Shader.h"
#include "../renderer/OcclusionCulling.h"

#include <memory>
#include <climits>
#include <queue>
#include <vector>
#include <algorithm>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

// ─────────────────────────────────────────────────────────────────────────────
//  World
//
//  Manages all full-detail chunks and low-resolution LOD tiles.
//
//  5-Level LOD System:
//    - Level 1: nearest ring  (2×2 chunks per tile)
//    - Level 2: mid ring      (4×4 chunks per tile)
//    - Level 3: far ring      (8×8 chunks per tile)
//    - Level 4: very far ring (16×16 chunks per tile)
//    - Level 5: ultra far ring (32×32 chunks per tile)
//
//  The distance for each ring is configured via Setting (lod1Start–lod5End).
// ─────────────────────────────────────────────────────────────────────────────

class World {
public:
    World();
    ~World();

    // Called every frame from Scene::update() — updates chunk & LOD state
    void update(glm::vec3 cameraPos, glm::vec3 cameraFront,
                const Frustum& frustum, bool isLoading);

    // Draw all full-detail chunks visible within the frustum
    void draw(const glm::vec3& cameraPos, const glm::vec3& cameraFront,
              const Frustum& frustum, const glm::mat4& viewProjection,
              GLuint shaderID);

    // Draw all LOD tiles — called after draw() in Scene, with uIsLOD=1
    void drawLOD(const glm::vec3& cameraPos, const Frustum& frustum,
                 const glm::mat4& viewProjection, GLuint shaderID);

    // ── Block operations ──────────────────────────────────────────────────
    bool isSolid(int x, int y, int z);
    int  getHeight(int x, int z);
    void setBlock(int x, int y, int z, BlockType type);

    // ── Chunk access ──────────────────────────────────────────────────────
    Chunk*                  getChunk(int chunkX, int chunkZ);
    std::shared_ptr<Chunk>  getChunkShared(int chunkX, int chunkZ);

    void      markChunkDirty(Chunk* chunk);
    long long getChunkKey(int x, int z);

private:
    // ── Regular chunks ────────────────────────────────────────────────────
    void loadChunk(int chunkX, int chunkZ, glm::vec3 cameraPos,
                   glm::vec3 cameraFront, const Frustum& frustum, bool isLoading);

    // Calculate generation/mesh priority for a chunk based on distance & frustum
    int  calculatePriority(int chunkX, int chunkZ, glm::vec3 cameraPos,
                           glm::vec3 cameraFront, const Frustum& frustum,
                           bool isLoading);

    void unloadFarChunks(int playerChunkX, int playerChunkZ);

    // ── Hardware occlusion culling ───────────────────────────────────────
    using OcclusionQuery = OcclusionCulling::Query;

    void invalidateOcclusion(const glm::vec3& cameraPos, const glm::vec3& cameraFront);
    bool shouldDrawChunk(OcclusionQuery& query);
    bool isOcclusionTestDue(const OcclusionQuery& query) const;
    void ensureOcclusionResources();
    void issueOcclusionQuery(OcclusionQuery& query,
                             const glm::vec3& minBounds,
                             const glm::vec3& maxBounds,
                             const glm::mat4& viewProjection, GLuint terrainShaderID);

    std::unordered_map<long long, OcclusionQuery> occlusionQueries;
    std::unordered_map<long long, OcclusionQuery> lodOcclusionQueries;
    glm::vec3 lastOcclusionCameraPos{0.0f};
    glm::vec3 lastOcclusionCameraFront{0.0f, 0.0f, -1.0f};
    bool occlusionCameraValid = false;
    unsigned int renderFrame = 0;
    GLuint occlusionVAO = 0;
    GLuint occlusionVBO = 0;
    GLint uOcclusionViewProjectionLoc = -1;
    GLint uOcclusionModelLoc = -1;
    std::unique_ptr<Shader> occlusionShader;

    int lastChunkX = INT_MAX;
    int lastChunkZ = INT_MAX;
    int lastLodTileX = INT_MAX;
    int lastLodTileZ = INT_MAX;
    int lastRequestedRenderDistance = -1;
    unsigned int worldUpdateFrame = 0;
    unsigned int lastLODRequestRefreshFrame = 0;

    std::unordered_map<long long, std::shared_ptr<Chunk>> chunks;
    mutable std::shared_mutex chunksMutex;

    std::unordered_map<long long, uint32_t> queuedChunks; // key → generation at the time of queuing
    std::vector<long long>                  remeshQueue;

    std::unique_ptr<ChunkWorker> worker;

    // ── Shader uniform cache (avoid glGetUniformLocation every frame) ─────
    GLint uIsLODLoc       = -1;
    GLint uTimeLoc        = -1;
    GLint uSpawnTimeLoc   = -1;
    GLuint cachedShaderID = 0;   // last shader whose uniforms were cached

    // Update the uniform location cache if the shader has changed
    void cacheUniformLocations(GLuint shaderID);

    // ── LOD system ────────────────────────────────────────────────────────
    // Unique tile key: encodes tileX, tileZ, and level into a single long long
    // Bit layout: [level 4bit][tileZ 30bit][tileX 30bit]
    static long long getLODKey(int tileX, int tileZ, int level);

    // Convert chunk coordinate to tile coordinate for a given level.
    // A level-L tile covers (2^L) chunks per side.
    static int chunkToTile(int chunkCoord, int level) {
        int cov = (1 << level);
        return (int)std::floor((float)chunkCoord / cov);
    }

    // Check whether a tile falls within the correct LOD ring for its level
    // (not inside the regular chunk area, not too far out)
    bool inLODRing(int tileX, int tileZ, int level,
                   int playerChunkX, int playerChunkZ) const;

    // Update the LOD tile registry every frame
    void updateLOD(int playerChunkX, int playerChunkZ, const Frustum& frustum,
                   bool isLoading, bool refreshRequests);

    // Submit a LOD mesh build request to the worker
    void requestLODTile(int tileX, int tileZ, int level);

    std::unordered_map<long long, std::shared_ptr<LODChunk>>   lodChunks;
    std::unordered_map<long long, std::array<int, 3>>          queuedLODTiles; // tiles currently being processed by the worker
};
