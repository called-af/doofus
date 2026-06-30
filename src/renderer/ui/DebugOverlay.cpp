#include "DebugOverlay.h"
#include "../../core/Setting.h"
#include "../opengl/Shader.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

static std::string fmt1(float v) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(1) << v;
  return ss.str();
}

void DebugOverlay::init(Shader *sharedShader) {
  font.init(Setting::fontPath.c_str(), Setting::fontSize, sharedShader);
}

std::string DebugOverlay::getCardinalDirection(const glm::vec3 &front) const {
  float angle = std::atan2(front.x, front.z) * (180.0f / 3.14159265f);
  if (angle < 0)
    angle += 360.0f;

  if (angle < 22.5f || angle >= 337.5f)
    return "North (-Z)";
  else if (angle < 67.5f)
    return "NE";
  else if (angle < 112.5f)
    return "East (+X)";
  else if (angle < 157.5f)
    return "SE";
  else if (angle < 202.5f)
    return "South (+Z)";
  else if (angle < 247.5f)
    return "SW";
  else if (angle < 292.5f)
    return "West (-X)";
  else
    return "NW";
}

void DebugOverlay::render(int screenWidth, int screenHeight, float fps,
                          const glm::vec3 &playerPos,
                          const glm::vec3 &cameraFront, int worldHeight,
                          const std::string &biomeName, bool visible) {
  if (!visible)
    return;

  glm::vec3 flatFront =
      glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));

  std::string dir = getCardinalDirection(flatFront);

  const float startX = 10.0f;
  float startY = 28.0f;
  const float lineH = 26.0f;
  const float scale = 1.0f;
  const glm::vec3 white = {1.0f, 1.0f, 1.0f};
  const glm::vec3 yellow = {1.0f, 0.9f, 0.2f};

  font.drawText("FPS: " + std::to_string((int)fps), startX, startY, scale,
                yellow, screenWidth, screenHeight);
  startY += lineH;

  font.drawText("XYZ: " + fmt1(playerPos.x) + " / " + fmt1(playerPos.y) +
                    " / " + fmt1(playerPos.z),
                startX, startY, scale, white, screenWidth, screenHeight);
  startY += lineH;

  font.drawText("Facing: " + dir, startX, startY, scale, white, screenWidth,
                screenHeight);
  startY += lineH;

  font.drawText("Ground Y: " + std::to_string(worldHeight), startX, startY,
                scale, white, screenWidth, screenHeight);

  startY += lineH;

  font.drawText("Biome: " + biomeName, startX, startY, scale, white,
                screenWidth, screenHeight);

  startY += lineH;
  font.drawText("Shadow Dist: " + std::to_string(Setting::shadowDistance) + " chks ([/])",
                startX, startY, scale, white, screenWidth, screenHeight);
}