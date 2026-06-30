#include "Shader.h"

#include <fstream>
#include <iostream>
#include <sstream>

// Resolve #include "path" directives relative to the including file's directory
static std::string resolveIncludes(const std::string &src, const std::string &dir) {
  std::istringstream stream(src);
  std::ostringstream out;
  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("#include", 0) == 0) {
      auto a = line.find('"');
      auto b = line.rfind('"');
      if (a != std::string::npos && b != a) {
        // Build path: dir + "/" + relative path, then normalize ".."
        std::string rel = line.substr(a + 1, b - a - 1);
        // Simple join — OS will handle ".." in fopen
        std::string includePath = dir + "/" + rel;
        std::ifstream f(includePath);
        if (f) {
          std::stringstream buf;
          buf << f.rdbuf();
          out << buf.str() << "\n";
          continue;
        } else {
          std::cerr << "[Shader] Could not open include: " << includePath << "\n";
        }
      }
    }
    out << line << "\n";
  }
  return out.str();
}

static std::string readFile(const char *path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "[Shader] Could not open: " << path << "\n";
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string dir = std::string(path);
  auto slash = dir.rfind('/');
  dir = (slash != std::string::npos) ? dir.substr(0, slash) : ".";
  return resolveIncludes(buffer.str(), dir);
}

Shader::Shader(const char *vertexPath, const char *fragmentPath) {
  std::string vertexCode   = readFile(vertexPath);
  std::string fragmentCode = readFile(fragmentPath);

  const char *vertexSource   = vertexCode.c_str();
  const char *fragmentSource = fragmentCode.c_str();

  auto checkCompile = [](GLuint shader, const char *name) {
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[1024];
      glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
      std::cerr << "[Shader] Compile error in " << name << ":\n" << log << "\n";
    }
  };

  GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vertexSource, nullptr);
  glCompileShader(vertex);
  checkCompile(vertex, vertexPath);

  GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fragmentSource, nullptr);
  glCompileShader(fragment);
  checkCompile(fragment, fragmentPath);

  id = glCreateProgram();
  glAttachShader(id, vertex);
  glAttachShader(id, fragment);
  glLinkProgram(id);

  GLint linkOk = 0;
  glGetProgramiv(id, GL_LINK_STATUS, &linkOk);
  if (!linkOk) {
    char log[1024];
    glGetProgramInfoLog(id, sizeof(log), nullptr, log);
    std::cerr << "[Shader] Link error: " << log << "\n";
  }

  glDeleteShader(vertex);
  glDeleteShader(fragment);
}

void Shader::use() { glUseProgram(id); }

void Shader::setInt(const std::string &name, int value) {
  glUniform1i(glGetUniformLocation(id, name.c_str()), value);
}
void Shader::setVec3(const std::string &name, const glm::vec3 &value) {
  glUniform3f(glGetUniformLocation(id, name.c_str()), value.x, value.y,
              value.z);
}