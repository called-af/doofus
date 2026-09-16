#pragma once

enum class RealmBand { Hell, Normal, Heaven };

struct TerrainSample {
    float continentalness;   // base landmass shape
    float peaks;             // mountain ridges
    float erosion;           // surface roughness
    float river;             // river carving
    float plateau;           // plateau top flatness mask
    float pillar;            // support pillar ridge noise
    float cliffMask;         // cliff edge erosion
    
    float islandF1;
    float islandF2;
    float islandCellId;
    float islandMountain;

    // Large-scale mountain massif — low-frequency, wide base, tall peaks
    float mountainMassif;
};