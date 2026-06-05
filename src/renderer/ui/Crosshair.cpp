#include "Crosshair.h"
#include "../../renderer/opengl/Shader.h"
#include "../../renderer/opengl/Texture.h"

Crosshair::Crosshair() {}

Crosshair::~Crosshair()
{
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void Crosshair::init(Shader* sharedShader)
{
    shader = sharedShader;

    texture = std::make_unique<Texture>("assets/textures/crosshair.png", true);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Crosshair::render(int screenWidth, int screenHeight)
{
    if (!shader || !texture) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->use();
    shader->setInt("mode", 0);   // mode texture
    shader->setInt("tex", 0);
    texture->bind(0);
    glUniform2f(glGetUniformLocation(shader->id, "screenSize"),
                (float)screenWidth, (float)screenHeight);

    float cx = screenWidth  * 0.5f;
    float cy = screenHeight * 0.5f;
    float half = size * 0.5f;

    float vertices[] = {
        cx - half, cy - half, 0.0f, 0.0f,
        cx + half, cy - half, 1.0f, 0.0f,
        cx + half, cy + half, 1.0f, 1.0f,
        cx + half, cy + half, 1.0f, 1.0f,
        cx - half, cy + half, 0.0f, 1.0f,
        cx - half, cy - half, 0.0f, 0.0f
    };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
}