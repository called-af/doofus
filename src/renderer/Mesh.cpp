#include "Mesh.h"

Mesh::Mesh(
    float* vertices,
    unsigned int size
)
{
    vertexCount =
        size / (3 * sizeof(float));

    glGenVertexArrays(1, &VAO);

    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        size,
        vertices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glEnableVertexAttribArray(0);
}

void Mesh::draw()
{
    glBindVertexArray(VAO);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        vertexCount
    );
}