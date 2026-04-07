// veex/Renderer.cpp
// Main render loop.  All GPU-state setup lives here; all uniform/sampler
// uploads are delegated to ShaderKit (veex::Shader) — Renderer never calls
// glUniform* directly.
//
// Draw-call budget per frame (world geometry):
//   • 1 UBO update      (SceneParams — camera, sun, fog, ambient cube)
//   • N glDrawArrays    (one per RenderBatch == one per unique material tuple)
// N is driven by BSP::BuildVertexBuffer's batch-grouping logic.

#include "veex/Renderer.h"
#include "veex/Logger.h"
#include "veex/Config.h"
#include "veex/FileSystem.h"
#include "veex/BSP.h"
#include "veex/Camera.h"
#include "veex/Skybox.h"
#include "veex/GameInfo.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace veex {

// ── Scene Constant Buffer ─────────────────────────────────────────────────────
// MUST match the layout(std140) SceneBlock in both shaders exactly.
// std140 rule: every member is padded to a vec4 (16-byte) boundary.
struct SceneData {
    glm::mat4 viewProj;         // 64
    glm::vec4 cameraPos;        // 16  (w unused)
    glm::vec4 sunDir;           // 16  (w unused)
    glm::vec4 sunColor;         // 16  (w = intensity scale)
    glm::vec4 fogParams;        // 16  x=start, y=end, z=density, w unused
    glm::vec4 fogColor;         // 16
    glm::vec4 ambientCube[6];   // 96
    // Total: 240 bytes
};
static_assert(sizeof(SceneData) == 240, "SceneData size mismatch — check std140 alignment");

// ── White 1×1 fallback texture ────────────────────────────────────────────────
static uint32_t CreateFallbackTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    uint32_t id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    uint8_t px[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

Renderer::Renderer() : m_vao(0), m_vbo(0), m_sceneUBO(0), m_currentVertexCount(0) {}
Renderer::~Renderer() { Shutdown(); }

bool Renderer::Init(const GameInfo& game) {
    if (glad_glEnable == nullptr) {
        Logger::Error("[Renderer] OpenGL function pointers not loaded!");
        return false;
    }

    // ── UBO (Slot 0) ──────────────────────────────────────────────────────────
    glGenBuffers(1, &m_sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_sceneUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneData), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // ── Shader ────────────────────────────────────────────────────────────────
    std::string vPath = ResolveAssetPath("shaders/vr_standard.vert", game);
    std::string fPath = ResolveAssetPath("shaders/vr_standard.frag", game);

    ShaderCombo worldCombo;
    worldCombo.name  = "Standard_World_Opaque";
    worldCombo.flags = SHADER_FEATURE_NORMAL_MAP | SHADER_FEATURE_LIGHTMAP | SHADER_FEATURE_FOG;

    if (!m_bspShader.LoadFromFiles(vPath, fPath, worldCombo)) {
        Logger::Error("[Renderer] Failed to compile world shader.");
        return false;
    }

    // ── VAO / VBO ─────────────────────────────────────────────────────────────
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    const size_t stride = sizeof(Vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, lmCoord));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tangent));
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW); 

    m_fallbackLightmap = CreateFallbackTexture(255, 255, 255);
    m_fallbackNormal   = CreateFallbackTexture(128, 128, 255);

    Logger::Info("[Renderer] Initialized.");
    return true;
}

void Renderer::UploadMap(const BSP& bsp) {
    const auto& verts = bsp.GetVertices();
    m_currentVertexCount = static_cast<int>(verts.size());
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    Logger::Info("[Renderer] Uploaded " + std::to_string(m_currentVertexCount) + " vertices.");
}

void Renderer::Render(int width, int height, const Camera& camera, const BSP& map, Skybox& skybox) {
    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    DrawMap(map, camera, width, height);
}

void Renderer::DrawMap(const BSP& map, const Camera& camera, int width, int height) {
    const auto& batches = map.GetBatches();
    if (batches.empty()) return;

    SceneData scene{};
    const float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    scene.viewProj  = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    scene.cameraPos = glm::vec4(camera.GetPosition(), 1.0f);
    scene.sunDir    = glm::vec4(map.GetSun().direction, 0.0f);
    scene.sunColor  = glm::vec4(map.GetSun().color, map.GetSun().intensity);
    scene.fogParams = glm::vec4(500.0f, 4000.0f, 0.01f, 0.0f);
    scene.fogColor  = glm::vec4(0.2f, 0.2f, 0.25f, 1.0f);

    const glm::vec4 kAmbBase(0.08f, 0.08f, 0.10f, 1.0f);
    for (int i = 0; i < 6; ++i) scene.ambientCube[i] = kAmbBase;
    scene.ambientCube[2] = glm::vec4(0.18f, 0.22f, 0.28f, 1.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, m_sceneUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SceneData), &scene);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    m_bspShader.Bind();
    uint32_t blockIdx = glGetUniformBlockIndex(m_bspShader.GetProgramID(), "SceneBlock");
    if (blockIdx != GL_INVALID_INDEX) glUniformBlockBinding(m_bspShader.GetProgramID(), blockIdx, 0);

    const glm::mat4 model(1.0f);
    const glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
    m_bspShader.SetMat4("u_Model", glm::value_ptr(model));
    m_bspShader.SetMat3("u_NormalMatrix", glm::value_ptr(normalMat));

    m_bspShader.BindSampler("u_MainTexture", 0);
    m_bspShader.BindSampler("u_LightmapTexture", 1);
    m_bspShader.BindSampler("u_NormalTexture", 4);

    const uint32_t lmAtlas = map.GetParser().GetLightmapAtlasID();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (lmAtlas != 0) ? lmAtlas : m_fallbackLightmap);

    int drawCalls = 0;
    glBindVertexArray(m_vao);
    for (const auto& batch : batches) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.textureID());
        const bool hasNormal = (batch.normalID() != 0);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, hasNormal ? batch.normalID() : m_fallbackNormal);
        m_bspShader.SetInt("u_HasNormal", hasNormal ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, (GLint)batch.offset, (GLsizei)batch.count);
        ++drawCalls;
    }
    glBindVertexArray(0);

    Logger::Info("[Renderer] Frame: draw_calls=" + std::to_string(drawCalls)
                  + "  vertices=" + std::to_string(m_currentVertexCount));
}

void Renderer::DrawDebugTBN(const BSP&, const Camera&, int, int) {
    Logger::Warn("[Renderer] DrawDebugTBN is not yet implemented.");
}

void Renderer::Shutdown() {
    if (m_fallbackLightmap) { glDeleteTextures(1, &m_fallbackLightmap); m_fallbackLightmap = 0; }
    if (m_fallbackNormal)   { glDeleteTextures(1, &m_fallbackNormal);   m_fallbackNormal   = 0; }
    if (m_vao)      { glDeleteVertexArrays(1, &m_vao);   m_vao      = 0; }
    if (m_vbo)      { glDeleteBuffers(1, &m_vbo);        m_vbo      = 0; }
    if (m_sceneUBO) { glDeleteBuffers(1, &m_sceneUBO);   m_sceneUBO = 0; }
    Logger::Info("[Renderer] Shutdown complete.");
}

} // namespace veex