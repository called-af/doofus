#pragma once

#include "opengl/Shader.h"
#include <glad/gl.h>
#include <glm/glm.hpp>

class Sky {
public:
  Sky();
  ~Sky();

  void Init();

  void Render(const glm::vec3 &topColor, const glm::vec3 &horizonColor,
              const glm::vec3 &bottomColor, const glm::vec3 &sunDir,
              const glm::vec3 &moonDir);

public:
  GLuint VAO = 0;
  GLuint VBO = 0;
  Shader *shader = nullptr;

  void setupMesh();
};