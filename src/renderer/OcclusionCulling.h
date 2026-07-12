#pragma once

#include <glad/gl.h>

//  OcclusionCulling
//
//  Non-blocking OpenGL query state shared by chunk and LOD draw paths.  Query
//  results are polled only when the driver reports them available, so this
//  never forces a CPU/GPU synchronization stall.
class OcclusionCulling {
public:
    struct Query {
        GLuint id = 0;
        bool visible = true;
        bool pending = false;
        bool discardPendingResult = false;
        unsigned int lastTestFrame = 0;
    };

    static bool poll(Query& query);
    static bool isTestDue(const Query& query, unsigned int frame);
    static void invalidate(Query& query);
    static void destroy(Query& query);
};
