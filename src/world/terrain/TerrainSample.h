#pragma once

struct TerrainSample {
    float continentalness;   // base landmass shape
    float peaks;             // mountain ridges
    float erosion;           // surface roughness
    float river;             // river carving
    float plateau;           // plateau top flatness mask
    float pillar;            // support pillar ridge noise
    float cliffMask;         // cliff edge erosion
};