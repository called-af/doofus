#include "OcclusionCulling.h"

bool OcclusionCulling::poll(Query& query)
{
    if (!query.pending) return query.visible;

    GLuint available = GL_FALSE;
    glGetQueryObjectuiv(query.id, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available == GL_FALSE) return query.visible;

    GLuint samplesPassed = GL_FALSE;
    glGetQueryObjectuiv(query.id, GL_QUERY_RESULT, &samplesPassed);
    if (!query.discardPendingResult)
        query.visible = samplesPassed == GL_TRUE;
    query.pending = false;
    query.discardPendingResult = false;
    return query.visible;
}

bool OcclusionCulling::isTestDue(const Query& query, unsigned int frame)
{
    constexpr unsigned int retestInterval = 12;
    return !query.pending && (query.lastTestFrame == 0
                              || frame - query.lastTestFrame >= retestInterval);
}

void OcclusionCulling::invalidate(Query& query)
{
    query.visible = true;
    query.discardPendingResult = true;
}

void OcclusionCulling::destroy(Query& query)
{
    if (query.id != 0) glDeleteQueries(1, &query.id);
    query = {};
}
