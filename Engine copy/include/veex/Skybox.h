#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "veex/Shader.h"
#include "veex/GameInfo.h"

namespace veex {

class Skybox {
public:
    Skybox() : m_vao(0), m_vbo(0), m_textureID(0) {}
    ~Skybox() { Shutdown(); }
    
    /**
     * Initializes the Skybox geometry and loads the 6-face cubemap.
     * Uses ResolveAssetPath to find materials/skybox/ textures.
     */
    bool Initialize(const std::string& skyName, const GameInfo& game);
    
    /**
     * Renders the skybox at 'infinity' by stripping translation from the view matrix.
     */
    void Render(const glm::mat4& view, const glm::mat4& projection);
    
    void Shutdown();

    /**
     * Required by the RenderGraph to verify the skybox is ready for the SkyboxPass.
     * Prevents GL errors if textures failed to load.
     */
    bool IsLoaded() const { return m_textureID != 0; }

private:
    uint32_t LoadCubemap(const std::vector<std::string>& faces);

    uint32_t m_vao;
    uint32_t m_vbo;
    uint32_t m_textureID;
    Shader   m_shader; 
};

} // namespace veex