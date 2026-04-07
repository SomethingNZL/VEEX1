#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include <algorithm>
#include <map>

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
    if (fullPath.empty()) {
        Logger::Error("BSP: Failed to resolve path: " + mapPath);
        return false;
    }

    if (!m_parser.LoadFromFile(fullPath)) {
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
    constexpr int   kSkipFlags  = 0x0080;

    m_vertices.clear();
    m_batches.clear();
    m_faceBatchMap.clear();
    m_faceBatchMap.resize(faces.size(), { -1, 0, 0 });

    // Group vertices by texture, same as before.
    // We also record which face contributed which verts so VIS can cull per-face.
    std::map<int, std::vector<Vertex>> batchGroups;
    // Per-face: {textureID, vertStart within that texture's group, vertCount}
    struct FaceStagingEntry { int texID; int localOffset; int count; };
    std::vector<FaceStagingEntry> faceStaging(faces.size(), { -1, 0, 0 });

    for (int fi = 0; fi < (int)faces.size(); ++fi)
    {
        const auto& face = faces[fi];
        if (face.texinfo < 0 || face.texinfo >= (int)texinfo.size()) continue;

        const texinfo_t& ti = texinfo[face.texinfo];
        if (ti.flags & kSkipFlags) continue;

        std::vector<glm::vec3> faceVerts;
        m_parser.GetFaceVertices(face, faceVerts);
        if (faceVerts.size() < 3) continue;

        glm::ivec2 dims    = m_parser.GetTextureDimensions(face.texinfo);
        std::string texName = m_parser.GetTextureName(face.texinfo);
        auto mat           = MaterialSystem::Get().GetMaterial(texName);
        int currentTexID   = (int)mat.textureID;

        glm::vec3 normal = SourceToGL(m_parser.GetFaceNormal(face));

        auto& group = batchGroups[currentTexID];
        int localStart = (int)group.size();
        int addedVerts = 0;

        for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
        {
            glm::vec3 p0 = SourceToGL(faceVerts[0]);
            glm::vec3 p1 = SourceToGL(faceVerts[i]);
            glm::vec3 p2 = SourceToGL(faceVerts[i + 1]);

            group.push_back({ p0 * kWorldScale, normal, ComputeTexCoord(faceVerts[0],     ti, (float)dims.x, (float)dims.y) });
            group.push_back({ p1 * kWorldScale, normal, ComputeTexCoord(faceVerts[i],     ti, (float)dims.x, (float)dims.y) });
            group.push_back({ p2 * kWorldScale, normal, ComputeTexCoord(faceVerts[i + 1], ti, (float)dims.x, (float)dims.y) });
            addedVerts += 3;
        }

        faceStaging[fi] = { currentTexID, localStart, addedVerts };
    }

    // Flatten groups into the final vertex buffer, track each texture's global base offset
    std::map<int, uint32_t> texGlobalBase; // texID -> offset in m_vertices where its group starts

    for (auto& [texID, verts] : batchGroups)
    {
        RenderBatch batch;
        batch.textureID = texID;
        batch.offset    = (uint32_t)m_vertices.size();
        batch.count     = (uint32_t)verts.size();

        texGlobalBase[texID] = batch.offset;

        m_batches.push_back(batch);
        m_vertices.insert(m_vertices.end(), verts.begin(), verts.end());
    }

    // Now build m_faceBatchMap so VIS culling knows each face's location in the GPU buffer
    // We need batchIndex per texID
    std::map<int, int> texToBatchIdx;
    for (int bi = 0; bi < (int)m_batches.size(); ++bi)
        texToBatchIdx[m_batches[bi].textureID] = bi;

    for (int fi = 0; fi < (int)faces.size(); ++fi)
    {
        const auto& fs = faceStaging[fi];
        if (fs.texID < 0 || fs.count == 0) continue;

        auto it = texGlobalBase.find(fs.texID);
        if (it == texGlobalBase.end()) continue;

        m_faceBatchMap[fi].batchIndex   = texToBatchIdx[fs.texID];
        m_faceBatchMap[fi].vertexOffset = (int)(it->second) + fs.localOffset;
        m_faceBatchMap[fi].vertexCount  = fs.count;
    }

    Logger::Info("BSP: Build Complete. Verts: " + std::to_string(m_vertices.size()) +
                 " Batches: " + std::to_string(m_batches.size()) +
                 " VIS: " + (m_parser.HasVis() ? "YES" : "NO"));
    return !m_vertices.empty();
}

// NEW -------------------------------------------------------------------------
// Returns a (possibly culled) list of RenderBatches.
// When VIS data is present we rebuild compact batches containing only visible
// geometry. The vertex data on the GPU is unchanged — we just issue smaller
// (and fewer) draw calls via adjusted offset/count.
//
// Implementation note: because faces in a batch are not necessarily contiguous
// in the vertex buffer, we can't express "skip this face" as a single offset+count.
// We therefore produce one RenderBatch *per visible face* (same textureID, tiny
// offset+count). The driver overhead of many small draw calls is still far cheaper
// than shading every occluded pixel, especially for large maps.
// If you want to collapse them you can sort by texID and merge contiguous ranges.
// -----------------------------------------------------------------------------
const std::vector<RenderBatch>& BSP::GetVisibleBatches(const glm::vec3& worldPos) const
{
    if (!m_parser.HasVis())
        return m_batches; // No VIS data — render everything as before

    auto visibleFaceIndices = m_parser.GetVisibleFaceIndices(worldPos);

    if (visibleFaceIndices.empty())
        return m_batches; // Couldn't determine location — render everything

    m_visibleBatchScratch.clear();

    for (int fi : visibleFaceIndices)
    {
        if (fi < 0 || fi >= (int)m_faceBatchMap.size()) continue;
        const auto& entry = m_faceBatchMap[fi];
        if (entry.batchIndex < 0 || entry.vertexCount == 0) continue;

        RenderBatch b;
        b.textureID = m_batches[entry.batchIndex].textureID;
        b.offset    = (uint32_t)entry.vertexOffset;
        b.count     = (uint32_t)entry.vertexCount;
        m_visibleBatchScratch.push_back(b);
    }

    return m_visibleBatchScratch;
}
// -----------------------------------------------------------------------------

glm::vec2 BSP::ComputeTexCoord(const glm::vec3& pos, const texinfo_t& ti, float texW, float texH) const
{
    float u = ti.textureVecs[0][0] * pos.x + ti.textureVecs[0][1] * pos.y + ti.textureVecs[0][2] * pos.z + ti.textureVecs[0][3];
    float v = ti.textureVecs[1][0] * pos.x + ti.textureVecs[1][1] * pos.y + ti.textureVecs[1][2] * pos.z + ti.textureVecs[1][3];
    return { u / texW, v / texH };
}

} // namespace veex
