# Documetation

voxel engine built with C++, OpenGL, and SDL3.

---

## Stack

- **Window**: SDL3
- **Rendering**: OpenGL (GLAD loader)
- **Math**: GLM
- **Build**: CMake + Ninja
- **Fonts**: JetBrains Mono, Space Grotesk
- **Image loading**: stb_image
- **Font rendering**: stb_truetype

---

## Architecture

```
Application
    ↓
Game
    ↓
Scene
    ↓
Renderer / World / Physics / Player / UI
```

---

## Project Structure

```
src/
├── camera/                       # First-person FPS camera
├── core/                         # Application loop, time system, settings
├── ecs/
│   ├── components/               # TransformComponent, RigidbodyComponent
│   └── systems/                  # PhysicsSystem
├── game/
│   ├── Game.*                    # High-level game manager
│   └── scene/                    # Scene (world, player, physics, rendering)
├── platform/
│   ├── input/                    # Keyboard & mouse input abstraction
│   ├── sdl/                      # SDLWindow (active)
│   ├── linux/                    # LinuxWindow (placeholder)
│   ├── windows/                  # Win32Window (placeholder)
│   ├── IWindow.h                 # Window interface
│   └── WindowFactory.*           # Backend factory
├── player/                       # PlayerController (movement, jump, mouse look)
├── renderer/
│   ├── opengl/                   # Mesh, Shader, Texture, TextureArray
│   ├── model/                    # OBJ loader, Model
│   ├── ui/                       # DebugOverlay, Crosshair, Font
│   ├── Frustum.*                 # Frustum culling
│   └── Sky.*                     # Procedural sky rendering
├── utils/                        # Logging
└── world/
    ├── biome/                    # BiomeManager, PlainBiome, DesertBiome
    ├── block/                    # BlockType, BlockData
    ├── mesher/                   # GreedyMesher
    ├── noise/                    # Perlin, FBM, Value, Ridge, Cellular noise
    ├── Chunk.*                   # 16×64×16 chunk storage
    ├── ChunkWorker.*             # Async chunk generation
    ├── TerrainGenerator.*        # Heightmap + layered terrain
    └── World.*                   # World management, chunk registry

assets/
├── shaders/
│   ├── block.vert / block.frag   # Terrain
│   ├── sky.vert / sky.frag       # Atmosphere
│   └── ui.vert / ui.frag         # HUD
├── textures/
│   ├── grass.png
│   ├── grass_side.png
│   ├── dirt.png
│   ├── stone.png
│   ├── sand.png
│   └── crosshair.png
├── models/                       # player.obj, player.mtl
└── fonts/                        # JetBrains Mono, Space Grotesk

external/
├── glad/                         # OpenGL loader
└── stb/                          # stb_image, stb_truetype
```

---

## Controls

| Key   | Action         |
|-------|----------------|
| W     | Move Forward   |
| S     | Move Backward  |
| A     | Move Left      |
| D     | Move Right     |
| Space | Jump           |
| Mouse | Look Around    |
| ESC   | Toggle Cursor  |
| B     | Debug Overlay  |

---

## Debug Overlay

- FPS counter
- Compass (N / S / W / E)
- Player coordinates (X, Y, Z)

---

## World System

### Chunk

- Size: **16 × 64 × 16** blocks
- Storage: `BlockType blocks[SIZE][HEIGHT][SIZE]`

### Terrain Layers

| Layer       | Block     |
|-------------|-----------|
| Surface     | Grass     |
| Underground | Dirt      |
| Deep        | Stone     |

### Noise Stack

- Perlin, FBM, Value, Ridge, Cellular — composable noise pipeline
- Chunk-independent deterministic generation

### Mesh Generation

- Greedy meshing (`GreedyMesher`)
- Face culling — only visible faces are emitted
- `GL_TEXTURE_2D_ARRAY` — per-face texture layer indexing
- Vertex format: `(x, y, z, u, v, layer)`
- Async chunk worker (`ChunkWorker`)

### Biomes

| Biome    | Status  |
| ----------| ---------|
| Plain    | Active  |
| Desert   | Active  |
| Mountain | Planned |
| Cave     | Planned |

---

## Rendering Pipeline

```
Input Update
    ↓
Camera Update
    ↓
Physics Update
    ↓
Frustum Culling
    ↓
World Rendering  (greedy mesh + texture array)
    ↓
Sky Rendering    (procedural atmosphere)
    ↓
UI Rendering     (crosshair, debug overlay)
    ↓
Swap Buffers
```

---

## Physics

```
Input
    ↓
Velocity Update
    ↓
Gravity
    ↓
Collision Detection  (world.isSolid)
    ↓
Position Correction
```

---

## Build

```bash
./build.sh
./run.sh
```

Requires: CMake, Ninja, SDL3, OpenGL.

---

## Performance Notes

- Chunk mesh caching
- Greedy meshing reduces vertex count
- Face culling — no hidden faces emitted
- Texture atlas batching → fewer draw calls
- Frustum culling — off-screen chunks skipped
- Delta-time simulation — FPS-independent movement

