#pragma once

#include <glm/glm.hpp>

class Time
{
public:

    int ticks = 6000; // mulai dari pagi (tick 6000 = sunrise, seperti Minecraft)
    int realTicks = 0; // real time ticks, not affected by daySpeed

    static constexpr int TICKS_PER_DAY = 24000;

    void update(float dt);

    float getDayProgress();

    glm::vec3 getSkyTopColor();
    glm::vec3 getSkyHorizonColor();
    glm::vec3 getSkyBottomColor();
    glm::vec3 getSunDirection();
    glm::vec3 getMoonDirection();
};