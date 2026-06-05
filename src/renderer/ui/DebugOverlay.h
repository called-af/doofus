#pragma once

#include "Font.h"
#include <glm/glm.hpp>
#include <string>

class Shader;

class DebugOverlay
{
public:
    void init(Shader* sharedShader);

    void render(
        int screenWidth,
        int screenHeight,
        float fps,
        const glm::vec3& playerPos,
        const glm::vec3& cameraFront,
        int worldHeight,
        bool visible
    );

private:
    std::string getCardinalDirection(const glm::vec3& front) const;

    Font font;
};