#pragma once
// veex/Renderer.h
// The Renderer owns all GL objects (VAO, VBO, UBO, fallback textures) and
// orchestrates ShaderKit to draw the world.  It never calls glUniform*
// directly — all uniform uploads go through ShaderKit (veex::Shader).

#include "veex/Common.h"
#include "veex/Shader.h"
#include <glm/glm.hpp>
#include <string>

namespace veex {

class Camera;
class BSP;
class Skybox;
struct GameInfo;

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Init(const GameInfo& game);
    void UploadMap(const BSP& bsp);
    void Render(int width, int height, const Camera& camera,
                const BSP& map, Skybox& skybox);
    void Shutdown();

    // Debug visualiser — draws TBN arrows via a geometry shader (stub).
    void DrawDebugTBN(const BSP& map, const Camera& camera,
                      int width, int height);

private:
    void DrawMap(const BSP& map, const Camera& camera, int width, int height);

    // ── GL objects ────────────────────────────────────────────────────────────
    uint32_t m_vao              = 0;
    uint32_t m_vbo              = 0;
    uint32_t m_sceneUBO         = 0;
    uint32_t m_fallbackLightmap = 0;   // 1×1 white — valid lightmap sample
    uint32_t m_fallbackNormal   = 0;   // 1×1 (128,128,255) — flat normal map
    int      m_currentVertexCount = 0;

    // ── ShaderKit programs ────────────────────────────────────────────────────
    Shader m_bspShader;        // main world / PBR shader
    Shader m_debugTbnShader;   // TBN visualiser (geometry shader, future)
};

} // namespace veex
