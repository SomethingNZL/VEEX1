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

    // ── Initialize tile renderer if needed ────────────────────────────────────
    if (width > 0 && height > 0) {
        // Only reinitialize if viewport size changed
        static int lastWidth = 0, lastHeight = 0;
        if (lastWidth != width || lastHeight != height) {
            m_tileRenderer.Initialize(width, height);
            lastWidth = width;
            lastHeight = height;
        }
        m_tileRenderer.UpdateTiles(camera, map);
    }

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
    scene.lightmapExposure = 2.0f;  // Source-style overbright exposure

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
        mp.lightmapBrightness    = 4.0f;   // Lightmap brightness multiplier (Source-style overbright)

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

// ── G-Buffer Operations ───────────────────────────────────────────────────────

void Renderer::CreateGBuffer(int width, int height)
{
    if (m_gBufferFBO) {
        DestroyGBuffer();
    }

    m_gBufferWidth = width;
    m_gBufferHeight = height;

    // ── Create FBO ────────────────────────────────────────────────────────────
    glGenFramebuffers(1, &m_gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_gBufferFBO);

    // ── Position texture (RGBA32F) ────────────────────────────────────────────
    glGenTextures(1, &m_gBufferPosition);
    glBindTexture(GL_TEXTURE_2D, m_gBufferPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gBufferPosition, 0);

    // ── Normal + roughness texture (RGBA8) ────────────────────────────────────
    glGenTextures(1, &m_gBufferNormal);
    glBindTexture(GL_TEXTURE_2D, m_gBufferNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gBufferNormal, 0);

    // ── Albedo + metallic texture (RGBA8) ─────────────────────────────────────
    glGenTextures(1, &m_gBufferAlbedo);
    glBindTexture(GL_TEXTURE_2D, m_gBufferAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gBufferAlbedo, 0);

    // ── Lightmap radiance texture (RGBA16F) ───────────────────────────────────
    glGenTextures(1, &m_gBufferLightmap);
    glBindTexture(GL_TEXTURE_2D, m_gBufferLightmap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_gBufferLightmap, 0);

    // ── Depth-stencil attachment ──────────────────────────────────────────────
    glGenTextures(1, &m_gBufferDepth);
    glBindTexture(GL_TEXTURE_2D, m_gBufferDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_gBufferDepth, 0);

    // ── Set up draw buffers ───────────────────────────────────────────────────
    GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0,  // Position
        GL_COLOR_ATTACHMENT1,  // Normal + roughness
        GL_COLOR_ATTACHMENT2,  // Albedo + metallic
        GL_COLOR_ATTACHMENT3   // Lightmap radiance
    };
    glDrawBuffers(4, drawBuffers);

    // ── Check completeness ────────────────────────────────────────────────────
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Logger::Error("[Renderer] G-Buffer FBO is incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Logger::Info("[Renderer] G-Buffer created: " + std::to_string(width) + "x" + std::to_string(height));
}

void Renderer::ResizeGBuffer(int width, int height)
{
    if (m_gBufferWidth == width && m_gBufferHeight == height) {
        return;  // No resize needed
    }
    DestroyGBuffer();
    CreateGBuffer(width, height);
}

void Renderer::DestroyGBuffer()
{
    if (m_gBufferFBO) {
        glDeleteFramebuffers(1, &m_gBufferFBO);
        m_gBufferFBO = 0;
    }
    if (m_gBufferPosition) {
        glDeleteTextures(1, &m_gBufferPosition);
        m_gBufferPosition = 0;
    }
    if (m_gBufferNormal) {
        glDeleteTextures(1, &m_gBufferNormal);
        m_gBufferNormal = 0;
    }
    if (m_gBufferAlbedo) {
        glDeleteTextures(1, &m_gBufferAlbedo);
        m_gBufferAlbedo = 0;
    }
    if (m_gBufferLightmap) {
        glDeleteTextures(1, &m_gBufferLightmap);
        m_gBufferLightmap = 0;
    }
    if (m_gBufferDepth) {
        glDeleteTextures(1, &m_gBufferDepth);
        m_gBufferDepth = 0;
    }
    m_gBufferWidth = 0;
    m_gBufferHeight = 0;
}

void Renderer::DrawGBufferPass(const BSP& map, const Camera& camera, int width, int height)
{
    // Bind G-Buffer FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_gBufferFBO);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    // Use the main BSP shader (which writes to G-buffer when in deferred mode)
    m_bspShader.Bind();
    
    // Set up scene data
    SceneParams scene{};
    const float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    scene.viewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    scene.cameraPos = glm::vec4(camera.GetPosition(), 1.0f);
    scene.sunDirection = glm::vec4(map.GetSun().direction, 0.0f);
    scene.sunColor = glm::vec4(map.GetSun().color, 1.0f);
    scene.fogParams = glm::vec4(500.0f, 4000.0f, 0.01f, 0.0f);
    scene.fogColor = glm::vec4(0.20f, 0.20f, 0.25f, 1.0f);
    scene.time = static_cast<float>(glfwGetTime());
    scene.lightmapExposure = 2.0f;

    m_bspShader.UploadSceneBlock(m_sceneUBO, scene);
    m_bspShader.SetMat4("u_Model", glm::value_ptr(glm::mat4(1.0f)));
    glm::mat3 normalMatrix = glm::mat3(1.0f);
    m_bspShader.SetMat3("u_NormalMatrix", glm::value_ptr(normalMatrix));

    // Bind samplers
    m_bspShader.BindSampler("u_MainTexture", 0);
    m_bspShader.BindSampler("u_LightmapTexture", 1);
    m_bspShader.BindSampler("u_RoughnessTexture", 2);
    m_bspShader.BindSampler("u_MetallicTexture", 3);
    m_bspShader.BindSampler("u_NormalTexture", 4);
    m_bspShader.BindSampler("u_EmissiveTexture", 5);
    m_bspShader.BindSampler("u_SpecMaskTexture", 6);

    // Bind lightmap atlas
    const uint32_t lmAtlas = map.GetParser().GetLightmapAtlasID();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lmAtlas ? lmAtlas : m_fallbackLightmap);

    // Draw batches
    glBindVertexArray(m_vao);
    for (const auto& batch : map.GetBatches()) {
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
        mp.roughness = batch.matParams.roughness;
        mp.metallic = batch.matParams.metallic;
        mp.emissiveScale = batch.matParams.emissiveScale;
        mp.hasNormalMap = batch.normalID() ? true : false;
        mp.hasRoughnessMap = batch.roughnessID() ? true : false;
        mp.hasMetallicMap = batch.metallicID() ? true : false;
        mp.hasSpecMaskMap = batch.key.specMaskID ? true : false;
        mp.hasEmissiveMap = batch.key.emissiveID ? true : false;
        mp.rnmScale = 1.0f;
        mp.lightmapSoftness = 0.5f;
        mp.diffuseFlattening = 0.5f;
        mp.edgePower = 2.0f;
        mp.geometricRoughnessPower = 4.0f;
        mp.lightmapBrightness = 4.0f;

        m_bspShader.UploadMaterialParams(mp);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.offset), static_cast<GLsizei>(batch.count));
    }

    glBindVertexArray(0);
    m_bspShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::DrawLightingPass(int width, int height)
{
    // For now, just blit the G-buffer albedo to screen
    // Full deferred lighting will be implemented with the lighting shader
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Simple fullscreen quad to display G-buffer albedo
    // This is a placeholder - full lighting pass will use the lighting shader
    glDisable(GL_DEPTH_TEST);
    // TODO: Implement fullscreen quad rendering with lighting shader
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawSkyboxToGBuffer(Skybox& skybox, const Camera& camera, int width, int height)
{
    // Skybox is typically rendered separately or to a special G-buffer attachment
    // For now, this is a placeholder for future implementation
    (void)skybox;
    (void)camera;
    (void)width;
    (void)height;
}

void Renderer::Shutdown()
{
    // Clean up G-buffer first
    DestroyGBuffer();

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
