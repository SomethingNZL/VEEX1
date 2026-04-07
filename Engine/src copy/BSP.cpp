#include "veex/BSP.h"
#include "veex/FileSystem.h"
#include "veex/Logger.h"

namespace veex {

bool BSP::LoadFromFile(const std::string& path, const GameInfo& game) {
    m_vertices.clear();

    // 1. Ask the FileSystem where 'wall.png' is, based on the GameInfo
    std::string textureName = "textures/wall.png"; 
    std::string resolvedPath = ResolveAssetPath(textureName, game);

    // 2. If the FS found it, load it into the GPU
    if (!resolvedPath.empty()) {
        if (m_texture.LoadFromFile(resolvedPath)) {
            Logger::Info("BSP: Texture successfully loaded for the room.");
        }
    }

    // --- Geometry Generation ---
    float texScale = 0.5f; 
    auto addQuad = [&](glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4, glm::vec3 norm, float width, float height) {
        float uMax = width * texScale;
        float vMax = height * texScale;
        m_vertices.push_back({p1, norm, {0.0f, 0.0f}});
        m_vertices.push_back({p2, norm, {uMax, 0.0f}});
        m_vertices.push_back({p3, norm, {uMax, vMax}});
        m_vertices.push_back({p1, norm, {0.0f, 0.0f}});
        m_vertices.push_back({p3, norm, {uMax, vMax}});
        m_vertices.push_back({p4, norm, {0.0f, vMax}});
    };

    float s = 10.0f; float h = 5.0f; float fw = s * 2.0f;
    addQuad({-s, 0,  s}, { s, 0,  s}, { s, 0, -s}, {-s, 0, -s}, {0, 1, 0}, fw, fw);
    addQuad({-s, 0, -s}, { s, 0, -s}, { s, h, -s}, {-s, h, -s}, {0, 0, 1}, fw, h);  
    addQuad({-s, 0,  s}, {-s, 0, -s}, {-s, h, -s}, {-s, h,  s}, {1, 0, 0}, fw, h);  
    addQuad({ s, 0, -s}, { s, 0,  s}, { s, h,  s}, { s, h, -s}, {-1, 0, 0}, fw, h); 
    addQuad({ s, 0,  s}, {-s, 0,  s}, {-s, h,  s}, { s, h,  s}, {0, 0, -1}, fw, h); 

    Logger::Info("BSP: Generated room with " + std::to_string(m_vertices.size()) + " vertices.");
    return true;
}

} // namespace veex