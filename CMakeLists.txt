cmake_minimum_required(VERSION 3.20)
project(doofus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(doofus

    src/main.cpp

    # core
    src/core/Application.cpp

    # platform
    src/platform/sdl/SDLWindow.cpp
    src/platform/WindowFactory.cpp
    src/platform/linux/LinuxWindow.cpp
    src/platform/input/Input.cpp

    # renderer
    src/renderer/Shader.cpp
    src/renderer/Mesh.cpp

    # camera
    src/camera/Camera.cpp

    # ecs
    src/ecs/systems/PhysicsSystem.cpp

    # world
    src/renderer/world/Chunk.cpp
    src/renderer/world/Noise.cpp
    src/renderer/world/TerrainGenerator.cpp
    src/renderer/world/World.cpp

    # glad
    external/glad/src/gl.c

    # utils
    src/utils/Log.cpp
)

target_include_directories(doofus PRIVATE
    src
    external/glad/include
)

target_link_libraries(doofus PRIVATE
    SDL3::SDL3
    OpenGL::GL
)