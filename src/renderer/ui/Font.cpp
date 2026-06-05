#include "Font.h"
#include "../opengl/Shader.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>

bool Font::init(const std::string& fontPath, int pixelSize, Shader* sharedShader)
{
    shader = sharedShader;

    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "FreeType init failed\n";
        return false;
    }

    FT_Face face;
    FT_Error err = FT_New_Face(ft, fontPath.c_str(), 0, &face);
    if (err) {
        std::cout << "Font load failed: " << fontPath << " (FT error: " << err << ")\n";
        return false;
    }

    FT_Set_Pixel_Sizes(face, 0, pixelSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     face->glyph->bitmap.width,
                     face->glyph->bitmap.rows,
                     0, GL_RED, GL_UNSIGNED_BYTE,
                     face->glyph->bitmap.buffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character ch;
        ch.textureID = tex;
        ch.size    = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
        ch.bearing = glm::ivec2(face->glyph->bitmap_left,  face->glyph->bitmap_top);
        ch.advance = face->glyph->advance.x;
        chars.insert({c, ch});
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void Font::drawText(
    const std::string& text,
    float x, float y, float scale,
    const glm::vec3& color,
    int screenWidth, int screenHeight)
{
    shader->use();
    shader->setInt("mode", 1);
    shader->setVec3("textColor", color);
    shader->setInt("tex", 0);
    glUniform2f(glGetUniformLocation(shader->id, "screenSize"),
                (float)screenWidth, (float)screenHeight);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    for (char c : text)
    {
        Character ch = chars[c];

        float xpos = x + ch.bearing.x * scale;
        float ypos = y - (ch.size.y - ch.bearing.y) * scale;
        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        float vertices[6][4] = {
          {xpos,     ypos + h, 0, 1},  
          {xpos,     ypos,     0, 0},  
          {xpos + w, ypos,     1, 0},  
          {xpos,     ypos + h, 0, 1},  
          {xpos + w, ypos,     1, 0},  
          {xpos + w, ypos + h, 1, 1}
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}