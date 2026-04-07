#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "veex/Shader.h"
#include "veex/GameInfo.h" // Added for path resolution

namespace veex {

class Skybox {
public:
    Skybox() = default;
    
    // Now accepts GameInfo to use ResolveAssetPath
    bool Initialize(const std::string& skyName, const GameInfo& game);
    
    void Render(const glm::mat4& view, const glm::mat4& projection);
    void Shutdown();

private:
    uint32_t LoadCubemap(const std::vector<std::string>& faces);

    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_textureID = 0;
    Shader   m_shader; 
};

} // namespace veex