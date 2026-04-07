#pragma once
// veex/Renderer.h
// The Renderer orchestrates the GPU state via a Source 2-style Render Graph.
// It manages VAOs, VBOs, and Scene-level UBOs, delegating uniform updates
// and texture binding abstractions to ShaderKit (veex::Shader).

#include "veex/Common.h"
#include "veex/Shader.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <functional>

namespace veex {

class Camera;
class BSP;
class Skybox;
struct GameInfo;

// ── RenderPass ───────────────────────────────────────────────────────────────
// Encapsulates a single functional unit of the frame (e.g., Shadows, Opaque, UI).
struct RenderPass {
    std::string name;
    std::function<void()> execute;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Prevent accidental copying of GL resources
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool Init(const GameInfo& game);
    void UploadMap(const BSP& bsp);
    void Shutdown();

    // ── Main Pipeline ─────────────────────────────────────────────────────────
    // Recording entry point: builds and executes the render graph.
    void Render(int width, int height, const Camera& camera,
                const BSP& map, Skybox& skybox);

    // ── Debug ─────────────────────────────────────────────────────────────────
    void DrawDebugTBN(const BSP& map, const Camera& camera,
                      int width, int height);

private:
    // Orchestration
    void BuildGraph(int width, int height, const Camera& camera, 
                    const BSP& map, Skybox& skybox);
    void ExecuteGraph();

    // Internal Draw Logic
    void DrawMapInternal(const BSP& map, const Camera& camera, 
                         int width, int height);

    // ── GL objects ────────────────────────────────────────────────────────────
    uint32_t m_vao              = 0;
    uint32_t m_vbo              = 0;
    uint32_t m_sceneUBO         = 0;
    uint32_t m_fallbackAlbedo   = 0;   // 1x1 white albedo for missing textures
    uint32_t m_fallbackLightmap = 0;   // 1x1 black for missing lightmaps
    uint32_t m_fallbackNormal   = 0;   // 1x1 Flat Tangent (128, 128, 255)
    uint32_t m_roughnessDefault = 0;   // 1x1 default roughness texture
    uint32_t m_metallicDefault  = 0;   // 1x1 default metallic texture
    uint32_t m_emissiveDefault  = 0;   // 1x1 black emissive texture
    uint32_t m_normalDefault    = 0;   // 1x1 normal fallback for missing maps
    uint32_t m_specMaskDefault  = 0;   // 1x1 combined specmask fallback
    int      m_currentVertexCount = 0;

    // ── Render Graph ──────────────────────────────────────────────────────────
    std::vector<RenderPass> m_renderGraph;

    // ── ShaderKit Programs ────────────────────────────────────────────────────
    Shader m_bspShader;        // Main world / PBR shader
};

} // namespace veex