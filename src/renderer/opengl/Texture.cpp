#include "Texture.h"
#include "../../external/stb/stb_image.h"

#include <iostream>

Texture::Texture(
    const std::string& path,
    bool pixelated
) {
    glGenTextures(1, &id);

    glBindTexture(
        GL_TEXTURE_2D,
        id
    );

    // FILTERING

    if (pixelated) {

        // voxel / pixel art
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_NEAREST
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_NEAREST
        );
    }
    else {

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_NEAREST
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_NEAREST
        );
    }

    // WRAP

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    // WRAP

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    // LOAD IMAGE

    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(false);

    unsigned char* data =
        stbi_load(
            path.c_str(),
            &width,
            &height,
            &channels,
            0
        );

    std::cout
        << width
        << "x"
        << height
        << "\n";

    if (!data) {

        std::cout
            << "FAILED LOAD TEXTURE: "
            << path
            << "\n";

        std::cout
            << stbi_failure_reason()
            << "\n";

        return;
    }

    GLenum format =
        channels == 4
        ? GL_RGBA
        : GL_RGB;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data
    );

    // glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
}

void Texture::bind(int unit) {

    glActiveTexture(
        GL_TEXTURE0 + unit
    );

    glBindTexture(
        GL_TEXTURE_2D,
        id
    );
}