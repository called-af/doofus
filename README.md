
---

# Documentation

## Overview

This project is a real-time 3D voxel-based engine built using:

* SDL3 window system
* OpenGL rendering (GLAD loader)
* GLM math library
* CMake build system
* Custom ECS-style physics system
* Procedural voxel world (chunk-based)
* Terrain generation system (noise-based)
* First-person camera controller
* Basic collision system with voxel world integration

The engine is structured around **chunked infinite-style terrain simulation** using procedural generation.

---

# Features Implemented

## Window System

The application uses SDL3 to manage:

* OpenGL context creation
* Window lifecycle
* Input polling
* Mouse capture (relative mode)
* Buffer swapping

### Implemented Features

* SDL3 window initialization
* OpenGL context setup
* Relative mouse mode (FPS camera)
* Frame buffer swap system

---

## OpenGL Rendering

Rendering system is built on modern OpenGL:

### Features

* VAO / VBO mesh system
* GLSL shaders (vertex + fragment)
* Depth testing enabled
* Perspective projection
* Chunk-based mesh rendering

### Pipeline

```txt
World → Chunk Mesh → Mesh Buffer → Shader → Screen
```

---

## Camera System (First Person)

A FPS-style camera is implemented.

### Controls

| Key   | Action       |
| ----- | ------------ |
| W     | Move forward |
| S     | Move back    |
| A     | Move left    |
| D     | Move right   |
| SPACE | Jump         |
| Mouse | Look around  |
| ESC   | Toggle mouse |

### Features

* Yaw & pitch rotation
* Camera vector recalculation
* View matrix generation
* Mouse sensitivity control

---

## Physics System

Physics system is integrated with voxel world collision.

### Features

* Gravity simulation
* Velocity integration
* Ground detection via voxel queries
* Jumping system
* Friction (horizontal damping)
* Collision with world blocks

### Physics Flow

```txt
Input → Velocity → Gravity → Collision Check → Position Update
```

---

## Voxel World System

This is the core system of the engine.

---

### World Structure

The world is divided into chunks:

```
World
 ├── Chunk (-2, -2)
 ├── Chunk (-1, -2)
 ├── ...
 └── Chunk (2, 2)
```

Each chunk contains:

* SIZE = 16 (x and z)
* HEIGHT = 64 (y)
* 3D block array

---

### Chunk System

Each chunk:

* Stores voxel data
* Generates terrain
* Builds mesh (only visible faces)
* Handles rendering

#### Block Storage

```cpp
BlockType blocks[SIZE][HEIGHT][SIZE];
```

---

## Procedural Terrain Generation

Terrain is generated using **procedural noise-like functions**.

### Current Implementation

You are using:

* sine-based noise
* cosine-based variation
* amplitude scaling

```cpp
float noise = sin(worldX * 0.12f) * cos(worldZ * 0.12f);
int height = 20 + (noise * 8);
```

### Terrain Behavior

* Flat-ish hills
* Smooth variation
* Deterministic generation
* Chunk-independent consistency

---

## Terrain Generator System (Improved Version)

A modular terrain system exists:

### Features

* Noise-based heightmap generation
* Multi-layer terrain (grass/dirt/stone)
* Chunk-aware world coordinates
* Extensible biome system (planned)

### Block Layers

| Height Range | Block Type |
| ------------ | ---------- |
| Surface      | Grass      |
| Below        | Dirt       |
| Deep         | Stone      |

---

## Mesh Optimization (Face Culling)

Only visible faces are rendered.

### Logic

A face is added only if neighboring block is air:

```cpp
if (isAir(x, y + 1, z)) addFace(top);
if (isAir(x, y - 1, z)) addFace(bottom);
if (isAir(x, y, z + 1)) addFace(front);
```

### Result

* Massive performance improvement
* No internal cube faces rendered
* Reduced vertex count significantly

---

## World Collision System

The physics system queries the voxel world:

### isSolid(x, y, z)

* Converts world position → chunk coordinates
* Finds correct chunk
* Converts to local coordinates
* Checks block type

```cpp
return block != BlockType::Air;
```

---

### Collision Usage

Used in:

* Player grounding
* Movement restriction
* Jump validation

---

## Chunk Coordinate System

World space → Chunk space conversion:

```cpp
chunkX = floor(x / Chunk::SIZE)
chunkZ = floor(z / Chunk::SIZE)

localX = x - chunkX * Chunk::SIZE
localZ = z - chunkZ * Chunk::SIZE
```

Supports:

* Negative coordinates
* Infinite-style expansion (conceptually)

---

## Biome System (Planned Extension)

Biome system will allow:

* Grassland
* Desert
* Mountain
* Snow
* Ocean

### Planned Behavior

Each biome will modify:

* Height scale
* Block type distribution
* Noise frequency
* Terrain roughness

---

## Input System

SDL-based input handling:

### Features

* Keyboard state tracking
* Mouse delta tracking
* ESC toggle system
* FPS movement mapping

---

## Physics + World Interaction

Player movement is directly influenced by voxel world:

### Interaction Flow

```txt
Player Input
    ↓
Velocity Update
    ↓
Physics Simulation
    ↓
Voxel Collision Check (World::isSolid)
    ↓
Position Correction
```

### Ground Detection

Player is grounded if:

```cpp
world.isSolid(feetX, feetY, feetZ)
```

---

## Rendering Pipeline

```txt
Application Loop
    ↓
Input Update
    ↓
Camera Update
    ↓
Physics Update
    ↓
World Draw
    ↓
Swap Buffers
```

---

## Memory & Performance Notes

Current optimizations:

* Chunk mesh prebuilt once
* Face culling enabled
* Static world generation
* No runtime regeneration yet

---

## Known Issues / Bugs

* Possible memory leak risk if Mesh recreated often
* Chunk regeneration not dynamic
* No frustum culling yet
* No chunk unloading system
* Noise is basic (sin/cos, not real Perlin/Simplex)

---

## Planned Improvements

### World System

* Infinite chunk streaming
* Chunk unloading system
* Threaded terrain generation
* Real noise (Perlin / Simplex)

### Rendering

* Frustum culling
* Texture atlas (block textures)
* Greedy meshing

### Physics

* AABB collision system
* Slope handling
* Better grounding logic

### Gameplay

* Biomes system fully implemented
* Water system
* Trees / structures
* Inventory system

### Engine

* ECS expansion
* Save/load world
* Editor tools

---