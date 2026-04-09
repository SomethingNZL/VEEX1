// veex/Renderer.cpp
// Updated to fix backface culling inversion for Source Engine geometry.
// Implements a decoupled Render Graph recording/execution model.

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
#include <unordered_map>

namespace veex {

// ── Fallback texture factory ──────────────────────────────────────────────────
static uint32_t MakeFallbackTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    uint32_t id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    const uint8_t px[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

Renderer::Renderer() = default;
Renderer::~Renderer() { Shutdown(); }

// ── Init ──────────────────────────────────────────────────────────────────────

bool Renderer::Init(const GameInfo& game)
{
    if (glad_glEnable == nullptr) {
        Logger::Error("[Renderer] OpenGL function pointers not loaded — check gladLoadGL().");
        return false;
    }

    // ── Scene UBO (binding 0) ─────────────────────────────────────────────────
    glGenBuffers(1, &m_sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_sceneUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneParams), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_sceneUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // ── ShaderKit: Compile standard world combo ───────────────────────────────
    ShaderCombo worldCombo;
    worldCombo.name  = "Standard_World_Opaque";
    worldCombo.flags = SHADER_FEATURE_LIGHTMAP 
                     | SHADER_FEATURE_FOG;

    const std::string vPath = ResolveAssetPath("shaders/vr_standard.vert", game);
    const std::string fPath = ResolveAssetPath("shaders/vr_standard.frag", game);

    if (!m_bspShader.LoadFromFiles(vPath, fPath, worldCombo)) {
        Logger::Error("[Renderer] Failed to compile world shader combo.");
        return false;
    }

    // ── VAO / VBO ─────────────────────────────────────────────────────────────
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // Must match veex/Common.h :: Vertex struct layout
    constexpr GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(0); // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1); // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2); // TexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord));
    glEnableVertexAttribArray(3); // LightmapCoord
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, lmCoord));
    glEnableVertexAttribArray(4); // Tangent
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tangent));
    glEnableVertexAttribArray(5); // RNM Radiosity U
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, rnmU));
    glEnableVertexAttribArray(6); // RNM Radiosity V
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, rnmV));
    glEnableVertexAttribArray(7); // RNM Radiosity N
    glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, rnmN));

    glBindVertexArray(0);

    // ── Global Rasterizer State ───────────────────────────────────────────────
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW); // Source Engine uses Clockwise winding for front faces

    m_fallbackAlbedo   = MakeFallbackTexture(255, 255, 255);
    // Lightmaps default to black when missing; no lightmap means direct/ambient only.
    m_fallbackLightmap = MakeFallbackTexture(0, 0, 0);
    m_fallbackNormal   = MakeFallbackTexture(128, 128, 255);
    m_roughnessDefault = MakeFallbackTexture(180, 180, 180);
    m_metallicDefault  = MakeFallbackTexture(0, 0, 0);
    m_emissiveDefault  = MakeFallbackTexture(0, 0, 0);
    m_specMaskDefault  = MakeFallbackTexture(0, 0, 0);

    // Keep the normal fallback separate for texture binding
    m_normalDefault    = MakeFallbackTexture(128, 128, 255);

    Logger::Info("[Renderer] Initialized with Render Graph backend and GL_CW winding.");
    return true;
}

// ── Map Geometry Upload ───────────────────────────────────────────────────────

void Renderer::UploadMap(const BSP& bsp)
{
    const auto& verts = bsp.GetVertices();
    m_currentVertexCount = static_cast<int>(verts.size());

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), 
                 verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    Logger::Info("[Renderer] Map VBO updated: " + std::to_string(m_currentVertexCount) + " vertices.");
}

// ── Render Loop (The Graph Orchestrator) ──────────────────────────────────────

void Renderer::Render(int width, int height, const Camera& camera, const BSP& map, Skybox& skybox)
{
    // Phase 1: Record
    BuildGraph(width, height, camera, map, skybox);

    // Phase 2: Execute
    ExecuteGraph();
}

void Renderer::BuildGraph(int width, int height, const Camera& camera, const BSP& map, Skybox& skybox)
{
    m_renderGraph.clear();

    // ── PASS: Opaque World ────────────────────────────────────────────────────
    m_renderGraph.push_back({ "OpaqueWorld", [=, &camera, &map]() {
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.04f, 0.05f, 1.0f);
        
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        
        // Ensure winding is CW for Source geometry; culling enabled
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW); 

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        this->DrawMapInternal(map, camera, width, height);
    }});

    // ── PASS: Debug Overlays ──────────────────────────────────────────────────
    m_renderGraph.push_back({ "DebugUI", [=, &camera, &map]() {
        // Reserved for ImGui or DrawDebugTBN
    }});
}

void Renderer::ExecuteGraph()
{
    for (const auto& pass : m_renderGraph) {
        pass.execute();
    }
}

// ── Internal Draw Call Logic ──────────────────────────────────────────────────

void Renderer::DrawMapInternal(const BSP& map, const Camera& camera, int width, int height)
{
    const auto& batches = map.GetBatches();
    if (batches.empty()) return;

    // ── 1. Populate SceneBlock ────────────────────────────────────────────────
    SceneParams scene{};
    const float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    
    scene.viewProj      = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    scene.cameraPos     = glm::vec4(camera.GetPosition(), 1.0f);
    scene.sunDirection  = glm::vec4(map.GetSun().direction, 0.0f);
    scene.sunColor      = glm::vec4(map.GetSun().color, 1.0f);
    scene.fogParams     = glm::vec4(500.0f, 4000.0f, 0.01f, 0.0f);
    scene.fogColor      = glm::vec4(0.20f, 0.20f, 0.25f, 1.0f);
    scene.time          = static_cast<float>(glfwGetTime());
    scene.lightmapExposure = 1.0f;

    // ── 2. Bind Shader and Upload Global State ────────────────────────────────
    m_bspShader.Bind();
    m_bspShader.UploadSceneBlock(m_sceneUBO, scene);
    m_bspShader.SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));
    glm::mat3 normalMatrix = glm::mat3(1.0f);
    m_bspShader.SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

    // Abstracted sampler bindings (ShaderKit)
    m_bspShader.BindSampler("u_MainTexture",      0);
    m_bspShader.BindSampler("u_LightmapTexture",  1);
    m_bspShader.BindSampler("u_RoughnessTexture", 2);
    m_bspShader.BindSampler("u_MetallicTexture",  3);
    m_bspShader.BindSampler("u_NormalTexture",    4);
    m_bspShader.BindSampler("u_EmissiveTexture",  5);
    m_bspShader.BindSampler("u_SpecMaskTexture",  6);

    // ── 3. Bind Shared Assets ─────────────────────────────────────────────────
    const uint32_t lmAtlas = map.GetParser().GetLightmapAtlasID();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lmAtlas ? lmAtlas : m_fallbackLightmap);

    // ── 4. Execute Batched Draw Calls ─────────────────────────────────────────
    glBindVertexArray(m_vao);
    for (const auto& batch : batches) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.textureID() ? batch.textureID() : m_fallbackAlbedo);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, batch.roughnessID() ? batch.roughnessID() : m_roughnessDefault);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, batch.metallicID() ? batch.metallicID() : m_metallicDefault);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, batch.normalID() ? batch.normalID() : m_normalDefault);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, batch.key.emissiveID ? batch.key.emissiveID : m_emissiveDefault);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, batch.key.specMaskID ? batch.key.specMaskID : m_specMaskDefault);

        MaterialParams mp;
        mp.roughness       = batch.matParams.roughness;
        mp.metallic        = batch.matParams.metallic;
        mp.emissiveScale   = batch.matParams.emissiveScale;
        mp.hasNormalMap    = batch.normalID() ? true : false;
        mp.hasRoughnessMap = batch.roughnessID() ? true : false;
        mp.hasMetallicMap  = batch.metallicID() ? true : false;
        mp.hasSpecMaskMap  = batch.key.specMaskID ? true : false;
        mp.hasEmissiveMap  = batch.key.emissiveID ? true : false;

        // ── PBR-lite tuning parameters ─────────────────────────────────────────────
        // Set default values for the hybrid RNM + Source lightmap shading model.
        // These can be tuned per-material if needed in the future.
        mp.rnmScale              = 1.0f;   // RNM sharpness scale
        mp.lightmapSoftness      = 0.5f;   // Lightmap directional softness
        mp.diffuseFlattening     = 0.5f;   // Diffuse flattening for rough surfaces
        mp.edgePower             = 2.0f;   // Edge term power (grazing angle control)
        mp.geometricRoughnessPower = 4.0f; // Curvature sensitivity for geometric roughness fallback

        m_bspShader.UploadMaterialParams(mp);
        glDrawArrays(GL_TRIANGLES,
                     static_cast<GLint>(batch.offset),
                     static_cast<GLsizei>(batch.count));
    }
    
    glBindVertexArray(0);
    m_bspShader.Unbind();
}

void Renderer::DrawDebugTBN(const BSP&, const Camera&, int, int) 
{
    Logger::Warn("[Renderer] DrawDebugTBN not yet implemented.");
}

void Renderer::Shutdown()
{
    if (m_sceneUBO) glDeleteBuffers(1, &m_sceneUBO);
    if (m_vao)      glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)      glDeleteBuffers(1, &m_vbo);
    
    if (m_fallbackAlbedo)   glDeleteTextures(1, &m_fallbackAlbedo);
    if (m_fallbackLightmap) glDeleteTextures(1, &m_fallbackLightmap);
    if (m_fallbackNormal)   glDeleteTextures(1, &m_fallbackNormal);
    if (m_roughnessDefault) glDeleteTextures(1, &m_roughnessDefault);
    if (m_metallicDefault)  glDeleteTextures(1, &m_metallicDefault);
    if (m_emissiveDefault)  glDeleteTextures(1, &m_emissiveDefault);
    if (m_specMaskDefault)  glDeleteTextures(1, &m_specMaskDefault);
    if (m_normalDefault)    glDeleteTextures(1, &m_normalDefault);

    Logger::Info("[Renderer] Shutdown complete.");
}

} // namespace veex