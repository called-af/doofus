#include "Noise.h"

#include <cmath>

float Noise::get(float x, float z)
{
    return
        sin(x * 0.05f)
        *
        cos(z * 0.05f);
}