#pragma once

struct CellularSample {
    float f1;
    float f2;
    float cellId;
};

class CellularNoise {
public:
    static CellularSample generate(float x, float z, int seed);
    static CellularSample generate(float x, float z, float scale, int seed);
};