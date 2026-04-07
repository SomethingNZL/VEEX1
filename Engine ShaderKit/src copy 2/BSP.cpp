#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include <algorithm>
#include <map> // Added for efficient batching

namespace veex {

static inline glm::vec3 SourceToGL(const glm::vec3& v)
{
    return glm::vec3(v.x, v.z, -v.y);
}

bool BSP::LoadFromFile(const std::string& path, const GameInfo& game)
{
    std::string mapPath = path;
    if (!game.mapDir.empty() && path.find(game.mapDir) == std::string::npos) {
        mapPath = game.mapDir + "/" + path;
    }
    
    std::string fullPath = ResolveAssetPath(mapPath, game);
    if (fullPath.empty())
    {
        Logger::Error("BSP: Failed to resolve path: " + mapPath);
        return false;
    }

    if (!m_parser.LoadFromFile(fullPath))
    {
        Logger::Error("BSP: Failed to load BSP file: " + fullPath);
        return false;
    }

    return true;
}

bool BSP::BuildVertexBuffer()
{
    const auto& faces   = m_parser.GetFaces();
    const auto& texinfo = m_parser.GetTexinfo();

    constexpr float kWorldScale = 0.03125f;
    // Reduced flags to ensure we don't accidentally skip valid world geometry
    constexpr int kSkipFlags = 0x0080; 

    m_vertices.clear();
    m_batches.clear();

    // Use a map to group vertices by Texture ID automatically
    std::map<int, std::vector<Vertex>> batchGroups;

    for (const auto& face : faces)
    {
        if (face.texinfo < 0 || face.texinfo >= (int)texinfo.size())
            continue;

        const texinfo_t& ti = texinfo[face.texinfo];
        if (ti.flags & kSkipFlags) continue;

        std::vector<glm::vec3> faceVerts;
        m_parser.GetFaceVertices(face, faceVerts);

        if (faceVerts.size() < 3) continue;

        glm::ivec2 dims = m_parser.GetTextureDimensions(face.texinfo);
        std::string texName = m_parser.GetTextureName(face.texinfo);
        auto mat = MaterialSystem::Get().GetMaterial(texName);
        int currentTexID = (int)mat.textureID;

        glm::vec3 normal = SourceToGL(m_parser.GetFaceNormal(face));

        // Triangulate the face and add to the texture's specific group
        for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
        {
            glm::vec3 p0 = SourceToGL(faceVerts[0]);
            glm::vec3 p1 = SourceToGL(faceVerts[i]);
            glm::vec3 p2 = SourceToGL(faceVerts[i + 1]);

            batchGroups[currentTexID].push_back({ p0 * kWorldScale, normal, ComputeTexCoord(faceVerts[0], ti, (float)dims.x, (float)dims.y) });
            batchGroups[currentTexID].push_back({ p1 * kWorldScale, normal, ComputeTexCoord(faceVerts[i], ti, (float)dims.x, (float)dims.y) });
            batchGroups[currentTexID].push_back({ p2 * kWorldScale, normal, ComputeTexCoord(faceVerts[i + 1], ti, (float)dims.x, (float)dims.y) });
        }
    }

    // Flatten groups into the final vertex buffer and create batches
    for (auto& item : batchGroups)
    {
        RenderBatch batch;
        batch.textureID = item.first;
        batch.offset = (uint32_t)m_vertices.size();
        batch.count = (uint32_t)item.second.size();
        
        m_batches.push_back(batch);
        m_vertices.insert(m_vertices.end(), item.second.begin(), item.second.end());
    }

    Logger::Info("BSP: Build Complete. Verts: " + std::to_string(m_vertices.size()) + " Batches: " + std::to_string(m_batches.size()));
    return !m_vertices.empty();
}

glm::vec2 BSP::ComputeTexCoord(const glm::vec3& pos, const texinfo_t& ti, float texW, float texH) const
{
    float u = ti.textureVecs[0][0] * pos.x + ti.textureVecs[0][1] * pos.y + ti.textureVecs[0][2] * pos.z + ti.textureVecs[0][3];
    float v = ti.textureVecs[1][0] * pos.x + ti.textureVecs[1][1] * pos.y + ti.textureVecs[1][2] * pos.z + ti.textureVecs[1][3];
    return { u / texW, v / texH };
}

} // namespace veex