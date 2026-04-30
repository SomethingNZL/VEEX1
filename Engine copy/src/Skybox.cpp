#include "veex/GLHeaders.h"
#include "veex/Skybox.h"
#include "veex/Logger.h"
#include "veex/FileSystem.h"
#include "../third_party/stb/stb_image.h" 
#include <vector>
#include <glm/gtc/type_ptr.hpp>

namespace veex {

static const float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
};

bool Skybox::Initialize(const std::string& skyName, const GameInfo& game) {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::string skyPath = "materials/skybox/" + skyName;
    
    // Correct Source to OpenGL Cubemap Mapping
    std::vector<std::string> faces = {
        ResolveAssetPath(skyPath + "rt.png", game), // +X
        ResolveAssetPath(skyPath + "lf.png", game), // -X
        ResolveAssetPath(skyPath + "up.png", game), // +Y
        ResolveAssetPath(skyPath + "dn.png", game), // -Y
        ResolveAssetPath(skyPath + "bk.png", game), // +Z
        ResolveAssetPath(skyPath + "ft.png", game)  // -Z
    };

    m_textureID = LoadCubemap(faces);
    
    std::string vPath = ResolveAssetPath("shaders/skybox.vert", game);
    std::string fPath = ResolveAssetPath("shaders/skybox.frag", game);

    if (!m_shader.LoadFromFiles(vPath, fPath)) {
        Logger::Error("Skybox: Shaders failed to load.");
        return false;
    }
    return true;
}

uint32_t Skybox::LoadCubemap(const std::vector<std::string>& faces) {
    uint32_t textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            Logger::Error("Skybox: stbi_load failed for: " + faces[i]);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void Skybox::Render(const glm::mat4& view, const glm::mat4& projection) {
    // 1. DEPTH FIX: Ensure skybox doesn't block map geo
    glDepthMask(GL_FALSE); 
    glDepthFunc(GL_LEQUAL); 
    glDisable(GL_CULL_FACE); 
    
    // FIX: Renamed from .Use() to .Bind() to match the new Shader system
    m_shader.Bind();
    
    // 2. SMEAR FIX: Strip translation so skybox is at infinity
    glm::mat4 skyView = glm::mat4(glm::mat3(view)); 
    
    // Use the new simplified aliases from our Shader update
    m_shader.SetMat4("view", glm::value_ptr(skyView));
    m_shader.SetMat4("projection", glm::value_ptr(projection));

    glBindVertexArray(m_vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    
    glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // 3. RESTORE STATE for the Map Renderer
    glBindVertexArray(0);
    m_shader.Unbind(); // Added for safety/consistency
    glDepthMask(GL_TRUE); 
    glDepthFunc(GL_LESS); 
    glEnable(GL_CULL_FACE);
}

void Skybox::Shutdown() {
    // Add NULL checks to prevent crashes if OpenGL context is invalid
    if (glad_glDeleteVertexArrays) glDeleteVertexArrays(1, &m_vao);
    if (glad_glDeleteBuffers) glDeleteBuffers(1, &m_vbo);
    if (glad_glDeleteTextures) glDeleteTextures(1, &m_textureID);
    
    // Reset IDs to prevent double deletion
    m_vao = 0;
    m_vbo = 0;
    m_textureID = 0;
}

} // namespace veex