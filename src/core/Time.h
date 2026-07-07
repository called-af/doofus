#pragma once

#include <glm/glm.hpp>

class Time
{
public:

    int ticks = 6000; // Start from morning time (tick 6000 equals sunrise similar to Minecraft day cycle)
    int realTicks = 0; // Real-time ticks not affected by daySpeed multiplier used for gameplay mechanics

    static constexpr int TICKS_PER_DAY = 24000;

    void update(float dt);

    float getDayProgress();

    glm::vec3 getSkyTopColor();
    glm::vec3 getSkyHorizonColor();
    glm::vec3 getSkyBottomColor();
    glm::vec3 getSunDirection();
    glm::vec3 getMoonDirection();
};