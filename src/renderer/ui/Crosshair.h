#pragma once

#include <memory>
#include <glad/gl.h>

class Shader;
class Texture;

class Crosshair
{
public:
    Crosshair();
    ~Crosshair();

    void init(Shader* sharedShader);

    void render(int screenWidth, int screenHeight);

private:
    float size = 16.0f;
    GLuint vao = 0;
    GLuint vbo = 0;

    Shader* shader = nullptr;
    std::unique_ptr<Texture> texture;
};