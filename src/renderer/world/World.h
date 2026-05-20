#pragma once

#include <vector>
#include <memory>
#include "Chunk.h"

class World
{
public:
    std::vector<std::unique_ptr<Chunk>> chunks;

    void generate();
    void draw();
    bool isSolid(int x, int y, int z);
};