#pragma once

#include <glad/gl.h>

class Mesh {
public:
    GLuint VAO;
    GLuint VBO;
    int vertexCount;
    unsigned int bufferCapacity;

    Mesh(const float* vertices, unsigned int size);
    ~Mesh();

    void update(const float* vertices, unsigned int size);
    void draw();
};
