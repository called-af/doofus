#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <map>
#include <memory>
#include <string>

class Shader;

struct Character
{
    GLuint     textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
};

class Font
{
public:
    bool init(const std::string& fontPath, int pixelSize, Shader* sharedShader);

    void drawText(
        const std::string& text,
        float x, float y,
        float scale,
        const glm::vec3& color,
        int screenWidth, int screenHeight
    );

private:
    std::map<char, Character> chars;
    GLuint vao = 0;
    GLuint vbo = 0;

    Shader* shader = nullptr;
};