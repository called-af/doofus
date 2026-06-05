cmake_minimum_required(VERSION 3.20)
project(doofus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

add_executable(doofus

    src/main.cpp

    # core
    src/core/Application.cpp
    src/core/Time.cpp

    #game
    src/game/Game.cpp
    src/game/scene/Scene.cpp

    external/stb/stb_image_impl.cpp

    #player
    src/player/PlayerController.cpp

    #ui
    src/renderer/ui/Crosshair.cpp
    src/renderer/ui/DebugOverlay.cpp
    src/renderer/ui/Font.cpp

    #model
    src/renderer/model/Model.cpp
    
    src/renderer/model/OBJLoader.cpp
    
    # platform
    src/platform/sdl/SDLWindow.cpp
    src/platform/WindowFactory.cpp
    src/platform/linux/LinuxWindow.cpp
    src/platform/input/Input.cpp

    # renderer
    src/renderer/opengl/Shader.cpp
    src/renderer/opengl/Mesh.cpp
    src/renderer/opengl/TextureArray.cpp
    src/renderer/opengl/Texture.cpp
    src/renderer/Sky.cpp
    src/renderer/Frustum.cpp

    # camera
    src/camera/Camera.cpp

    # ecs
    src/ecs/systems/PhysicsSystem.cpp

    # world
    src/world/Chunk.cpp
    src/world/noise/Noise.cpp
    # src/world/noise/PerlinNoise.cpp
    # src/world/noise/CellularNoise.cpp
    src/world/noise/FBMNoise.cpp
    # src/world/noise/RidgeNoise.cpp
    # src/world/noise/ValueNoise.cpp
    src/world/TerrainGenerator.cpp
    src/world/mesher/GreedyMesher.cpp
    src/world/World.cpp
    src/world/ChunkWorker.cpp
    src/world/biome/BiomeManager.cpp
    src/world/biome/plain/PlainBiome.cpp
    src/world/biome/desert/DesertBiome.cpp

    # glad
    external/glad/src/gl.c

    # utils
    src/utils/Log.cpp
)

target_include_directories(doofus PRIVATE
    src
    external/glad/include
    external/stb
)

target_link_libraries(doofus PRIVATE
    SDL3::SDL3
    OpenGL::GL
    Freetype::Freetype
)

file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})