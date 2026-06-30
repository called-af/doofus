#include "Sky.h"

static float skyVertices[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f,

                              -1.0f, 1.0f,  1.0f, -1.0f, 1.0f,  1.0f};

Sky::Sky() {}

Sky::~Sky() {
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  delete shader;
}

void Sky::Init() {
  shader = new Shader("assets/shaders/sky.vert", "assets/shaders/sky.frag");

  setupMesh();
}

void Sky::setupMesh() {
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices), skyVertices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

void Sky::Render(const glm::vec3 &topColor, const glm::vec3 &horizonColor,
                 const glm::vec3 &bottomColor, const glm::vec3 &sunDir,
                 const glm::vec3 &moonDir) {
  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);

  shader->use();

  shader->setVec3("uTopColor", topColor);
  shader->setVec3("uHorizonColor", horizonColor);
  shader->setVec3("uBottomColor", bottomColor);
  shader->setVec3("uSunDir", sunDir);
  shader->setVec3("uMoonDir", moonDir);

  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}