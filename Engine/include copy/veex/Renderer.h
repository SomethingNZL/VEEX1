#pragma once
#include "veex/Common.h"
#include "veex/Shader.h"

namespace veex {

class Camera;
class BSP;
class Skybox;
class GameInfo; // This fixes the 'unknown type name' error

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Init(const GameInfo& game);
    void UploadMap(const BSP& bsp);
    void Render(int width, int height, const Camera& camera, const BSP& map, Skybox& skybox);
    void Shutdown();

private:
    void DrawMap(const BSP& map, const Camera& camera, int width, int height);

    uint32_t m_vao, m_vbo;
    int m_currentVertexCount;
    Shader m_bspShader;
};

} // namespace veex