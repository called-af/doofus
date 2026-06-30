#include "Time.h"
#include "Setting.h"

#include <cmath>

void Time::update(float dt)
{
    static float accumulator = 0.0f;
    static float realAccumulator = 0.0f;

    accumulator += dt * Setting::daySpeed;

    /*
        20 tick per second (at daySpeed 1.0)
    */

    while (accumulator >= (1.0f / 20.0f))
    {
        ticks++;
        accumulator -= (1.0f / 20.0f);
    }

    if (ticks >= TICKS_PER_DAY)
        ticks = 0;

    /*
        Real time ticks - not affected by daySpeed
        Used for gameplay mechanics like player cooldowns
    */
    realAccumulator += dt;

    while (realAccumulator >= (1.0f / 20.0f))
    {
        realTicks++;
        realAccumulator -= (1.0f / 20.0f);
    }
}

float Time::getDayProgress()
{
    return (float)ticks / (float)TICKS_PER_DAY;
}

// t = 1.0 → siang penuh, t = 0.0 → malam
// tick 6000 = tengah siang, tick 18000 = tengah malam
static float sunBrightness(float progress)
{
    float angle = (progress - 0.25f) * 2.0f * 3.141592f;
    return glm::clamp(std::cos(angle) * 1.3f, 0.0f, 1.0f);
}

glm::vec3 Time::getSkyTopColor()
{
    float t = sunBrightness(getDayProgress());
    return glm::mix(glm::vec3(0.02f, 0.02f, 0.08f), glm::vec3(0.2f, 0.4f, 0.9f), t);
}

glm::vec3 Time::getSkyHorizonColor()
{
    float t = sunBrightness(getDayProgress());
    // Sunrise/sunset: horizon jadi orange di transisi
    float sunriseGlow = 1.0f - std::abs(t - 0.5f) * 2.0f; // puncak di t=0.5 (transisi)
    glm::vec3 dayHorizon   = glm::mix(glm::vec3(1.0f, 0.5f, 0.1f), glm::vec3(0.5f, 0.7f, 1.0f), t);
    glm::vec3 nightHorizon = glm::vec3(0.03f, 0.03f, 0.10f);
    return glm::mix(nightHorizon, dayHorizon, t);
}

glm::vec3 Time::getSkyBottomColor()
{
    float t = sunBrightness(getDayProgress());
    return glm::mix(glm::vec3(0.0f, 0.0f, 0.02f), glm::vec3(0.1f, 0.1f, 0.2f), t);
}

glm::vec3 Time::getSunDirection()
{
    // tick 6000 = progress 0.25 = matahari di atas (siang)
    float angle = getDayProgress() * 2.0f * 3.141592f;
    return glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.0001f));
}

glm::vec3 Time::getMoonDirection()
{
    // Moon is opposite to sun
    float angle = (getDayProgress() + 0.5f) * 2.0f * 3.141592f;
    return glm::normalize(glm::vec3(std::cos(angle), std::sin(angle), 0.0001f));
}
