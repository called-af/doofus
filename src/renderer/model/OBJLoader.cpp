#include "OBJLoader.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <glm/glm.hpp>

std::vector<float> OBJLoader::load(const std::string& path) {

    std::ifstream file(path);

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;

    std::vector<float> vertices;

    std::string line;

    while (std::getline(file, line)) {

        std::stringstream ss(line);

        std::string prefix;

        ss >> prefix;

        // vertex
        if (prefix == "v") {

            glm::vec3 pos;

            ss >> pos.x >> pos.y >> pos.z;

            positions.push_back(pos);
        }

        // uv
        else if (prefix == "vt") {

            glm::vec2 uv;

            ss >> uv.x >> uv.y;

            uvs.push_back(uv);
        }

        // face
        else if (prefix == "f") {

    std::vector<std::string> faceVertices;

    std::string vert;

    while (ss >> vert) {
        faceVertices.push_back(vert);
    }

    // triangulate quad
    int indices[6] = {0, 1, 2, 0, 2, 3};

    int count =
        faceVertices.size() == 4
        ? 6
        : 3;

    for (int i = 0; i < count; i++) {

        std::string current =
            faceVertices[
                faceVertices.size() == 4
                ? indices[i]
                : i
            ];

        std::replace(
            current.begin(),
            current.end(),
            '/',
            ' '
        );

        std::stringstream vs(current);

        int posIndex = 0, uvIndex = 0, normalIndex = 0;

        vs >> posIndex >> uvIndex >> normalIndex;

        if (posIndex < 1 || posIndex > (int)positions.size())
            continue;

        glm::vec3 pos = positions[posIndex - 1];
        glm::vec2 uv = (uvIndex >= 1 && uvIndex <= (int)uvs.size()) ? uvs[uvIndex - 1] : glm::vec2(0.0f);

        vertices.push_back(pos.x);
        vertices.push_back(pos.y);
        vertices.push_back(pos.z);

        vertices.push_back(uv.x);

        // IMPORTANT
        vertices.push_back(1.0f - uv.y);
    }
}
    }

    return vertices;
}