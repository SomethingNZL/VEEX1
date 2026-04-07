#include "veex/Renderer.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include "veex/FileSystem.h"
#include "veex/BSP.h"
#include "veex/Camera.h"
#include "veex/Skybox.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace veex {

Renderer::Renderer() : m_vao(0), m_vbo(0), m_currentVertexCount(0) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Init(const GameInfo& game) {
    if (glad_glEnable == nullptr) {
        Logger::Error("Renderer: OpenGL pointers are null! Initialize GLAD first.");
        return false;
    }

    std::string vPath = ResolveAssetPath("shaders/basic.vert", game);
    std::string fPath = ResolveAssetPath("shaders/basic.frag", game);
    
    if (vPath.empty() || fPath.empty()) {
        Logger::Error("Renderer: Failed to resolve shader paths.");
        return false;
    }

    if (!m_bspShader.LoadFromFiles(vPath, fPath)) {
        Logger::Error("Renderer: Failed to load shaders.");
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    Logger::Info("Renderer: Initialized Successfully.");
    return true;
}

void Renderer::UploadMap(const BSP& bsp) {
    const auto& vertices = bsp.GetVertices();
    m_currentVertexCount = static_cast<int>(vertices.size());

    if (m_currentVertexCount > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        Logger::Info("Renderer: Uploaded " + std::to_string(m_currentVertexCount) + " vertices to GPU.");
    } else {
        Logger::Error("Renderer: UploadMap called but vertex list is EMPTY!");
    }
}

void Renderer::Render(int width, int height, const Camera& camera, const BSP& map, Skybox& skybox) {
    glViewport(0, 0, width, height);
    glClearColor(0.39f, 0.58f, 0.93f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    float aspect = (float)width / (float)(height ? height : 1);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 proj = camera.GetProjectionMatrix(aspect);

    // --- SKYBOX PASS ---
    glDepthMask(GL_FALSE); 
    glDepthFunc(GL_LEQUAL);
    skybox.Render(view, proj); 
    glDepthMask(GL_TRUE);  
    glDepthFunc(GL_LESS);

    // --- MAP PASS ---
    DrawMap(map, camera, width, height);
}

void Renderer::DrawMap(const BSP& map, const Camera& camera, int width, int height) {
    if (m_currentVertexCount <= 0) return;
    
    const auto& batches = map.GetBatches();
    if (batches.empty()) return;

    m_bspShader.Use();

    float aspect = (float)width / (float)(height ? height : 1);
    glm::mat4 proj = camera.GetProjectionMatrix(aspect);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 model = glm::mat4(1.0f); // Vertices are already scaled in BSP.cpp

    // REMOVED "u_" PREFIX TO MATCH YOUR SHADER
    m_bspShader.SetUniform("projection", glm::value_ptr(proj));
    m_bspShader.SetUniform("view", glm::value_ptr(view));
    m_bspShader.SetUniform("model", glm::value_ptr(model));
    
    // Check your basic.frag: if the sampler is named "u_MainTexture", keep this.
    // If it's just "tex" or "diffuse", change it here too!
    m_bspShader.SetUniform("u_MainTexture", 0); 

    glBindVertexArray(m_vao);
    glDisable(GL_CULL_FACE); 

    for (const auto& batch : batches) {
        if (batch.count == 0) continue;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.textureID);
        glDrawArrays(GL_TRIANGLES, batch.offset, batch.count);
    }

    glBindVertexArray(0);
}

void Renderer::Shutdown() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    m_vao = 0;
    m_vbo = 0;
}

} // namespace veex