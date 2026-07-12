#include "Mesh.h"

Mesh::Mesh(const float *vertices, unsigned int size) {
  vertexCount = size / (7 * sizeof(float));
  bufferCapacity = size;

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_DYNAMIC_DRAW);

  // position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // uv
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // layer
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                        (void *)(5 * sizeof(float)));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                        (void *)(6 * sizeof(float)));

  glEnableVertexAttribArray(3);
}

void Mesh::update(const float *vertices, unsigned int size) {
  vertexCount = size / (7 * sizeof(float));

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  if (size > bufferCapacity) {
    bufferCapacity = size;
    glBufferData(GL_ARRAY_BUFFER, bufferCapacity, nullptr, GL_DYNAMIC_DRAW);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, size, vertices);
}

void Mesh::draw() {
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, vertexCount);
}

Mesh::~Mesh() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
}
