#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
  GLuint id;

  Shader(const char *vertexPath, const char *fragmentPath);
  ~Shader();

  void use();

  void setInt(const std::string &name, int value);

  void setVec3(const std::string &name, const glm::vec3 &value);
};
