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
#include "veex/GUI.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <cstring>

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

    // ── Reflection Probe System ─────────────────────────────────────────────────
    m_reflectionProbeSystem.Initialize();
    
    // Bind the reflection probe UBO to binding point 1
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_reflectionProbeSystem.GetProbeDataUBO());
    
    Logger::Info("[Renderer] Reflection Probe System initialized.");

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

    // ── PASS: Capture Reflection Probes ───────────────────────────────────────
    m_renderGraph.push_back({ "CaptureReflectionProbes", [=, &camera, &map]() {
        this->CaptureReflectionProbes(map, camera, width, height);
    }});

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
        (void)camera;
        (void)map;
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
    scene.lightmapExposure = 1.75f;  // Source-style overbright exposure

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
    m_bspShader.BindSampler("u_DetailTexture",    7);
    
    // ── 2.5. Bind Reflection Probe Cubemap ────────────────────────────────────
    // Bind reflection probe cubemap to texture unit 8
    m_bspShader.BindSampler("u_ReflectionCubemap", 8);
    
    // Set probe index and has reflection probe flag (defaults)
    m_bspShader.SetInt("u_ProbeIndex", -1);  // Default: no probe
    m_bspShader.SetInt("u_HasReflectionProbe", 0);  // Use int instead of bool

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

        // Bind detail texture (from VMT)
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, batch.key.detailID ? batch.key.detailID : 0);

        // ── Reflection Probe Setup ──────────────────────────────────────────────
        // Set reflection probe information for this batch
        // For now, use the closest probe to the camera position
        int probeIndex = -1;
        bool hasReflectionProbe = false;
        uint32_t cubemapID = 0;
        
        const auto& probes = m_reflectionProbeSystem.GetProbes();
        if (!probes.empty()) {
            // Find closest probe to camera
            glm::vec3 camPos = camera.GetPosition();
            float minDist = FLT_MAX;
            int closestIdx = 0;
            
            for (size_t i = 0; i < probes.size(); ++i) {
                float dist = glm::distance(camPos, probes[i]->GetPosition());
                if (dist < minDist && dist < probes[i]->GetRadius()) {
                    minDist = dist;
                    closestIdx = static_cast<int>(i);
                    hasReflectionProbe = true;
                }
            }
            
            if (hasReflectionProbe) {
                probeIndex = closestIdx;
                cubemapID = probes[closestIdx]->GetCubemapTexture();
            }
        }
        
        // Bind reflection probe cubemap
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);
        
        // Set shader uniforms for reflection probe
        m_bspShader.SetInt("u_ProbeIndex", probeIndex);
        m_bspShader.SetInt("u_HasReflectionProbe", hasReflectionProbe ? 1 : 0);

        MaterialParams mp;
        mp.roughness       = batch.matParams.roughness;
        mp.metallic        = batch.matParams.metallic;
        mp.emissiveScale   = batch.matParams.emissiveScale;
        mp.hasNormalMap    = batch.normalID() ? true : false;
        mp.hasRoughnessMap = batch.roughnessID() ? true : false;
        mp.hasMetallicMap  = batch.metallicID() ? true : false;
        mp.hasSpecMaskMap  = batch.key.specMaskID ? true : false;
        mp.hasEmissiveMap  = batch.key.emissiveID ? true : false;
        mp.hasDetail       = batch.key.detailID ? true : false;

        // ── Paper's Lighting Model Parameters ────────────────────────────────────────
        // Based on: "A Practical Real-Time Lighting Model for BSP-Based Renderers"
        mp.diffuseCoefficient    = 1.0f;   // k_D in paper (diffuse coefficient)
        mp.lightmapBrightness    = 4.0f;   // Lightmap brightness multiplier (Source-style overbright)

        // ── VMT Material Parameters ──────────────────────────────────────────────
        // Set detail texture parameters from VMT
        mp.detailScale       = batch.matParams.detailScale;
        mp.detailBlendFactor = batch.matParams.detailBlendFactor;
        mp.detailBlendMode   = batch.matParams.detailBlendMode;

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
        // Paper's Lighting Model Parameters
        mp.diffuseCoefficient = 1.0f;
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

    // Add NULL checks to prevent crashes if OpenGL context is invalid
    if (m_sceneUBO && glad_glDeleteBuffers) glDeleteBuffers(1, &m_sceneUBO);
    if (m_vao && glad_glDeleteVertexArrays) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo && glad_glDeleteBuffers) glDeleteBuffers(1, &m_vbo);
    
    if (m_fallbackAlbedo && glad_glDeleteTextures)   glDeleteTextures(1, &m_fallbackAlbedo);
    if (m_fallbackLightmap && glad_glDeleteTextures) glDeleteTextures(1, &m_fallbackLightmap);
    if (m_fallbackNormal && glad_glDeleteTextures)   glDeleteTextures(1, &m_fallbackNormal);
    if (m_roughnessDefault && glad_glDeleteTextures) glDeleteTextures(1, &m_roughnessDefault);
    if (m_metallicDefault && glad_glDeleteTextures)  glDeleteTextures(1, &m_metallicDefault);
    if (m_emissiveDefault && glad_glDeleteTextures)  glDeleteTextures(1, &m_emissiveDefault);
    if (m_specMaskDefault && glad_glDeleteTextures)  glDeleteTextures(1, &m_specMaskDefault);
    if (m_normalDefault && glad_glDeleteTextures)    glDeleteTextures(1, &m_normalDefault);

    // Reset IDs to prevent double deletion
    m_sceneUBO = 0;
    m_vao = 0;
    m_vbo = 0;
    m_fallbackAlbedo = 0;
    m_fallbackLightmap = 0;
    m_fallbackNormal = 0;
    m_roughnessDefault = 0;
    m_metallicDefault = 0;
    m_emissiveDefault = 0;
    m_specMaskDefault = 0;
    m_normalDefault = 0;

    Logger::Info("[Renderer] Shutdown complete.");
}

// ── Reflection Probe Operations ───────────────────────────────────────────────

void Renderer::CaptureReflectionProbes(const BSP& map, const Camera& camera, int width, int height)
{
    // Capture cubemaps for all dirty reflection probes
    auto& probes = m_reflectionProbeSystem.GetProbes();
    for (auto* probe : probes) {
        if (probe && probe->IsDirty()) {
            // Capture each face of the cubemap
            CaptureProbeCubemap(map, camera, probe, width, height);
        }
    }
    
    // Upload probe data to UBO for shader access
    m_reflectionProbeSystem.UploadProbeData();
}

void Renderer::CaptureProbeCubemap(const BSP& map, const Camera& camera, 
                                   ReflectionProbe* probe, int viewportWidth, int viewportHeight)
{
    if (!probe) return;
    
    // Get the probe's capture framebuffer from the system
    // For now, we'll create a temporary FBO here
    uint32_t captureFBO = 0;
    uint32_t captureDepthRB = 0;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureDepthRB);
    
    // Set up capture matrices
    glm::vec3 position = probe->GetPosition();
    float aspect = 1.0f;  // Cubemap faces are square
    float fov = 90.0f;
    float near = 0.1f;
    float far = 10000.0f;
    
    glm::mat4 captureProj = glm::perspective(glm::radians(fov), aspect, near, far);
    
    // 6 face view matrices
    const glm::mat4 captureViews[6] = {
        glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),  // +X
        glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)), // -X
        glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),   // +Y
        glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)), // -Y
        glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),  // +Z
        glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))  // -Z
    };
    
    // Bind capture framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    
    // Set up depth renderbuffer
    glBindRenderbuffer(GL_RENDERBUFFER, captureDepthRB);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 
                         probe->GetCubemapResolution(), probe->GetCubemapResolution());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureDepthRB);
    
    // Capture each face
    for (int i = 0; i < 6; i++) {
        // Attach the face to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                              probe->GetCubemapTexture(), 0);
        
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            continue;
        }
        
        // Set viewport to cubemap resolution
        glViewport(0, 0, probe->GetCubemapResolution(), probe->GetCubemapResolution());
        
        // Clear the face
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Enable depth test and culling for proper scene rendering
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CW);
        
        // Render the scene from this face's perspective
        RenderReflectionProbeCubemap(map, camera, 
                                     probe->GetCubemapResolution(), probe->GetCubemapResolution(),
                                     captureProj * captureViews[i]);
    }
    
    // Restore original viewport
    glViewport(0, 0, viewportWidth, viewportHeight);
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Clean up temporary resources
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureDepthRB);
    
    probe->MarkCaptured(); // Mark as captured and no longer dirty
}

void Renderer::RenderReflectionProbeCubemap(const BSP& map, const Camera& camera,
                                           int width, int height, const glm::mat4& viewProj)
{
    // This method renders the scene from a specific view/projection for cubemap capture
    // It's called by ReflectionProbeSystem::CaptureProbe for each face
    
    const auto& batches = map.GetBatches();
    if (batches.empty()) return;
    
    // Set up scene data with the probe's view/projection
    SceneParams scene{};
    const float aspect = (height > 0) ? (float)width / (float)height : 1.0f;
    
    scene.viewProj = viewProj;
    scene.cameraPos = glm::vec4(camera.GetPosition(), 1.0f);
    scene.sunDirection = glm::vec4(map.GetSun().direction, 0.0f);
    scene.sunColor = glm::vec4(map.GetSun().color, 1.0f);
    scene.fogParams = glm::vec4(500.0f, 4000.0f, 0.01f, 0.0f);
    scene.fogColor = glm::vec4(0.20f, 0.20f, 0.25f, 1.0f);
    scene.time = static_cast<float>(glfwGetTime());
    scene.lightmapExposure = 1.0f; // Lower exposure for probe capture
    
    // Bind shader and upload scene data
    m_bspShader.Bind();
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
    m_bspShader.BindSampler("u_DetailTexture", 7);
    
    // Bind lightmap atlas
    const uint32_t lmAtlas = map.GetParser().GetLightmapAtlasID();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lmAtlas ? lmAtlas : m_fallbackLightmap);
    
    // Draw batches
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
        
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, batch.key.detailID ? batch.key.detailID : 0);
        
        MaterialParams mp;
        mp.roughness = batch.matParams.roughness;
        mp.metallic = batch.matParams.metallic;
        mp.emissiveScale = batch.matParams.emissiveScale;
        mp.hasNormalMap = batch.normalID() ? true : false;
        mp.hasRoughnessMap = batch.roughnessID() ? true : false;
        mp.hasMetallicMap = batch.metallicID() ? true : false;
        mp.hasSpecMaskMap = batch.key.specMaskID ? true : false;
        mp.hasEmissiveMap = batch.key.emissiveID ? true : false;
        mp.hasDetail = batch.key.detailID ? true : false;
        // Paper's Lighting Model Parameters
        mp.diffuseCoefficient = 1.0f;
        mp.lightmapBrightness = 1.0f;  // Lower exposure for probe capture
        
        mp.detailScale = batch.matParams.detailScale;
        mp.detailBlendFactor = batch.matParams.detailBlendFactor;
        mp.detailBlendMode = batch.matParams.detailBlendMode;
        
        m_bspShader.UploadMaterialParams(mp);
        glDrawArrays(GL_TRIANGLES,
                    static_cast<GLint>(batch.offset),
                    static_cast<GLsizei>(batch.count));
    }
    
    glBindVertexArray(0);
    m_bspShader.Unbind();
}

// ── Probe Creation for Map ────────────────────────────────────────────────────

void Renderer::CreateProbesForMap(const BSP& map)
{
    // Get map bounds from vertices
    const auto& vertices = map.GetVertices();
    if (vertices.empty()) return;
    
    glm::vec3 minBound(FLT_MAX), maxBound(-FLT_MAX);
    for (const auto& v : vertices) {
        minBound = glm::min(minBound, v.position);
        maxBound = glm::max(maxBound, v.position);
    }
    
    // Calculate map center and size
    glm::vec3 center = (minBound + maxBound) * 0.5f;
    glm::vec3 size = maxBound - minBound;
    
    Logger::Info("[Renderer] Map bounds: min(" + 
                std::to_string(minBound.x) + ", " + std::to_string(minBound.y) + ", " + std::to_string(minBound.z) + ") " +
                "max(" + std::to_string(maxBound.x) + ", " + std::to_string(maxBound.y) + ", " + std::to_string(maxBound.z) + ")");
    
    // Create probes in a grid pattern based on map size
    // For now, create a simple arrangement: one probe at center, plus probes at key positions
    
    // Clear existing probes
    auto& probes = m_reflectionProbeSystem.GetProbes();
    while (!probes.empty()) {
        m_reflectionProbeSystem.RemoveProbe(probes.back());
    }
    
    // Create center probe
    m_reflectionProbeSystem.CreateProbe(center, size.x * 0.5f);
    Logger::Info("[Renderer] Created reflection probe at center: (" + 
                std::to_string(center.x) + ", " + std::to_string(center.y) + ", " + std::to_string(center.z) + ")");
    
    // Create additional probes for larger maps
    if (size.x > 1000.0f || size.z > 1000.0f) {
        // Create probes at quarter positions
        float quarterX = (minBound.x + center.x) * 0.5f;
        float quarterZ = (minBound.z + center.z) * 0.5f;
        float threeQuarterX = (center.x + maxBound.x) * 0.5f;
        float threeQuarterZ = (center.z + maxBound.z) * 0.5f;
        
        // Four corner probes at mid-height
        float midY = (minBound.y + maxBound.y) * 0.5f;
        
        m_reflectionProbeSystem.CreateProbe(glm::vec3(quarterX, midY, quarterZ), size.x * 0.25f);
        m_reflectionProbeSystem.CreateProbe(glm::vec3(threeQuarterX, midY, quarterZ), size.x * 0.25f);
        m_reflectionProbeSystem.CreateProbe(glm::vec3(quarterX, midY, threeQuarterZ), size.x * 0.25f);
        m_reflectionProbeSystem.CreateProbe(glm::vec3(threeQuarterX, midY, threeQuarterZ), size.x * 0.25f);
        
        Logger::Info("[Renderer] Created 4 additional reflection probes for large map");
    }
    
    // Assign faces to probes
    m_reflectionProbeSystem.AssignFacesToProbes(const_cast<BSP&>(map));
    
    Logger::Info("[Renderer] Created " + std::to_string(m_reflectionProbeSystem.GetProbeCount()) + " reflection probes for map");
}

} // namespace veex
