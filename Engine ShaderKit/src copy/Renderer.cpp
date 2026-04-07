#include "veex/Renderer.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstddef>
#include <iostream>

namespace veex {

Renderer::Renderer() : m_vao(0), m_vbo(0) {}
Renderer::~Renderer() { Shutdown(); }

bool Renderer::Init() {
    std::string root = Config::GetExecutableDir();
    if (!root.empty() && root.back() != '/') root += "/";
    
    std::string vertPath = root + "shaders/basic.vert";
    std::string fragPath = root + "shaders/basic.frag";

    if (!m_basicShader.LoadFromFiles(vertPath, fragPath)) {
        Logger::Error("Renderer: [FATAL] Failed to load shaders at: " + vertPath);
        return false;
    }
    Logger::Info("Renderer: Shaders loaded and linked successfully.");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // aPos (layout 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, position));
    
    // aNormal (layout 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, normal));

    // aTexCoord (layout 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BSPVertex), (void*)offsetof(BSPVertex, texCoord));

    glEnable(GL_DEPTH_TEST);
    Logger::Info("Renderer: VAO/VBO initialized and Depth Testing enabled.");
    return true;
}

void Renderer::Clear() {
    glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::BeginFrame(const Camera& camera, GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    
    glViewport(0, 0, width, height);
    Clear();

    m_basicShader.Use();
    float aspect = (float)width / (float)height;
    
    glm::mat4 proj = camera.GetProjectionMatrix(aspect);
    glm::mat4 view = camera.GetViewMatrix();
    
    m_basicShader.SetUniform("projection", &proj[0][0]);
    m_basicShader.SetUniform("view", &view[0][0]);
}

void Renderer::Draw(const BSP& map, const Camera& camera) {
    DrawMap(map); 
}

void Renderer::DrawMap(const BSP& map) {
    const auto& vertices = map.GetVertices();
    
    static int frameCount = 0;
    if (frameCount % 500 == 0) {
        Logger::Info("Renderer: DrawCall triggered with " + std::to_string(vertices.size()) + " vertices.");
    }
    frameCount++;

    if (vertices.empty()) return;

    m_basicShader.Use();

    // 1. FORCED SAMPLER BINDING
    // Using your Shader class's internal SetUniform to link 'uTexture' to Unit 0
    m_basicShader.SetUniform("uTexture", 0); 
    
    // 2. BIND ACTUAL TEXTURE TO UNIT 0
    map.GetTexture().Bind(0); 

    // 3. SET MATRICES
    glm::mat4 model = glm::mat4(1.0f);
    m_basicShader.SetUniform("model", &model[0][0]);

    // 4. UPLOAD AND DRAW
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BSPVertex), vertices.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    
    // Check for GL Errors
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        Logger::Error("OpenGL Error: " + std::to_string(err));
    }
}

void Renderer::Shutdown() {
    Logger::Info("Renderer: Shutting down and cleaning up GL buffers.");
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
}

void Renderer::EndFrame() {
    // Currently a no-op: swap buffers is handled by Application.
    // Reserved for post-process passes, bloom, tonemapping, etc.
}

} // namespace veex