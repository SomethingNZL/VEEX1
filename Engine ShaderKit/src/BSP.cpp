// veex/BSP.cpp
// BSP world loader, batch builder, and MikkTSpace tangent generator.

#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "mikktspace.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <cmath>

namespace veex {

static constexpr float kWorldScale = 1.0f / 32.0f;

static inline glm::vec3 SourceToGL(const glm::vec3& v) {
    return { v.x, v.z, -v.y };
}

// ── MikkTSpace Callbacks ──────────────────────────────────────────────────────
// Restored: These are required for the Normal Mapping API to function.

struct MikkContext {
    std::vector<Vertex>* verts;
};

static int MikkNumFaces(const SMikkTSpaceContext* ctx) {
    return static_cast<int>(static_cast<const MikkContext*>(ctx->m_pUserData)->verts->size() / 3);
}
static int MikkNumVertsOfFace(const SMikkTSpaceContext*, int) { return 3; }
static void MikkGetPos(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
    const auto& v = (*static_cast<const MikkContext*>(ctx->m_pUserData)->verts)[face * 3 + vert];
    out[0] = v.position.x; out[1] = v.position.y; out[2] = v.position.z;
}
static void MikkGetNorm(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
    const auto& v = (*static_cast<const MikkContext*>(ctx->m_pUserData)->verts)[face * 3 + vert];
    out[0] = v.normal.x; out[1] = v.normal.y; out[2] = v.normal.z;
}
static void MikkGetUV(const SMikkTSpaceContext* ctx, float out[], int face, int vert) {
    const auto& v = (*static_cast<const MikkContext*>(ctx->m_pUserData)->verts)[face * 3 + vert];
    out[0] = v.texCoord.x; out[1] = v.texCoord.y;
}
static void MikkSetTSpaceBasic(const SMikkTSpaceContext* ctx, const float tangent[], float sign, int face, int vert) {
    auto& v = (*static_cast<MikkContext*>(ctx->m_pUserData)->verts)[face * 3 + vert];
    v.tangent = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

// Non-const to prevent "discards qualifiers" error
static SMikkTSpaceInterface kMikkInterface = {
    MikkNumFaces, MikkNumVertsOfFace, MikkGetPos, MikkGetNorm, MikkGetUV, MikkSetTSpaceBasic, nullptr
};

// ── BSP Implementation ────────────────────────────────────────────────────────

bool BSP::LoadFromFile(const std::string& path, const GameInfo& game) {
    const std::string fullPath = ResolveAssetPath(path, game);
    if (fullPath.empty()) return false;
    if (!m_parser.LoadFromFile(fullPath)) return false;

    ParseSun();
    m_parser.BuildLightmapAtlas();
    return BuildVertexBuffer();
}

void BSP::ParseSun() {
    EntityParser ep;
    const auto entities = ep.Parse(m_parser.GetEntityData());
    for (const auto& e : entities) {
        if (e.kv.count("classname") && e.kv.at("classname") == "light_environment") {
            float yawDeg = e.GetAngles().y;
            float pitchDeg = e.kv.count("pitch") ? e.GetFloat("pitch", -45.0f) : -45.0f;
            float pitch = glm::radians(-pitchDeg);
            float yaw = glm::radians(yawDeg);
            glm::vec3 srcDir(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw));
            m_sun.direction = glm::normalize(SourceToGL(srcDir));
            
            auto lit = e.kv.find("_light");
            if (lit != e.kv.end()) {
                std::istringstream ss(lit->second);
                float r, g, b, intensity;
                if (ss >> r >> g >> b >> intensity) {
                    m_sun.intensity = intensity / 200.0f;
                    m_sun.color = (glm::vec3(r, g, b) / 255.0f) * m_sun.intensity;
                }
            }
            return;
        }
    }
}

bool BSP::BuildVertexBuffer() {
    const auto& faces = m_parser.GetFaces();
    const auto& texinfo = m_parser.GetTexinfo();
    const auto& lmInfos = m_parser.GetFaceLightmapInfo();

    m_vertices.clear();
    m_batches.clear();

    // The grouping logic: keeps draw calls low by merging geometry by Material
    std::unordered_map<BatchKey, std::vector<Vertex>> materialVerts;
    std::unordered_map<BatchKey, RenderBatch> materialMeta;

    for (int fi = 0; fi < (int)faces.size(); ++fi) {
        const dface_t& f = faces[fi];
        if (f.texinfo < 0) continue;
        const texinfo_t& ti = texinfo[f.texinfo];
        if (ti.flags & 0x0080) continue; // SURF_NODRAW

        std::vector<glm::vec3> facePoints;
        m_parser.GetFaceVertices(f, facePoints);
        if (facePoints.size() < 3) continue;

        Material mat = MaterialSystem::Get().GetMaterial(m_parser.GetTextureName(f.texinfo));

        // RESTORED: Creating the key based on your Material struct
        BatchKey key;
        key.textureID   = mat.textureID;
        key.normalID    = mat.normalID;
        key.roughnessID = mat.roughnessID;
        key.metallicID  = mat.metallicID;

        if (materialMeta.find(key) == materialMeta.end()) {
            RenderBatch rb;
            rb.key = key;
            // API COMPATIBILITY: Mapping your material IDs to ShaderKit params
            rb.matParams.roughness = (mat.roughnessID != 0) ? 1.0f : 0.8f;
            rb.matParams.metallic  = (mat.metallicID != 0) ? 1.0f : 0.0f;
            rb.matParams.hasNormalMap = (mat.normalID != 0);
            rb.matParams.hasRoughnessMap = (mat.roughnessID != 0);
            rb.matParams.hasMetallicMap = (mat.metallicID != 0);
            materialMeta[key] = rb;
        }

        glm::vec3 N = SourceToGL(m_parser.GetFaceNormal(f));
        glm::ivec2 dim = m_parser.GetTextureDimensions(f.texinfo);
        bool hasLM = (fi < (int)lmInfos.size() && lmInfos[fi].valid);

        for (size_t i = 1; i + 1 < facePoints.size(); ++i) {
            glm::vec3 tri[3] = { facePoints[0], facePoints[i], facePoints[i+1] };
            for (int k = 0; k < 3; ++k) {
                Vertex v;
                v.position = SourceToGL(tri[k]) * kWorldScale;
                v.normal   = N;
                v.texCoord = ComputeTexCoord(tri[k], ti, (float)dim.x, (float)dim.y);
                v.lmCoord  = hasLM ? ComputeLightmapCoord(tri[k], ti, f, lmInfos[fi]) : glm::vec2(0.5f);
                materialVerts[key].push_back(v);
            }
        }
    }

    // RESTORED: MikkTSpace tangent generation for every batch
    for (auto& [key, verts] : materialVerts) {
        MikkContext mikkCtx{ &verts };
        SMikkTSpaceContext ctx{};
        ctx.m_pInterface = &kMikkInterface;
        ctx.m_pUserData  = &mikkCtx;
        genTangSpaceDefault(&ctx);

        RenderBatch batch = materialMeta[key];
        batch.offset = (uint32_t)m_vertices.size();
        batch.count  = (uint32_t)verts.size();
        m_batches.push_back(batch);
        m_vertices.insert(m_vertices.end(), verts.begin(), verts.end());
    }

    m_lastFaceCount = (int)faces.size();
    return !m_vertices.empty();
}

const std::vector<RenderBatch>& BSP::GetVisibleBatches(const glm::vec3& worldPos) const {
    if (!m_parser.HasVis()) return m_batches;

    // FIXED: Using public GetVisibleFaceIndices to avoid FindLeaf private error
    auto visFaces = m_parser.GetVisibleFaceIndices(worldPos);
    if (visFaces.empty()) return m_batches;

    // API COMPATIBILITY: Returns m_batches for now to ensure rendering works.
    return m_batches;
}

// ── Utility Functions ─────────────────────────────────────────────────────────

glm::vec2 BSP::ComputeTexCoord(const glm::vec3& pos, const texinfo_t& ti, float w, float h) const {
    float u = glm::dot(glm::vec3(ti.textureVecs[0][0], ti.textureVecs[0][1], ti.textureVecs[0][2]), pos) + ti.textureVecs[0][3];
    float v = glm::dot(glm::vec3(ti.textureVecs[1][0], ti.textureVecs[1][1], ti.textureVecs[1][2]), pos) + ti.textureVecs[1][3];
    return { u / (w > 0 ? w : 1), v / (h > 0 ? h : 1) };
}

glm::vec2 BSP::ComputeLightmapCoord(const glm::vec3& pos, const texinfo_t& ti, const dface_t& face, const FaceLightmapInfo& lm) const {
    float u = glm::dot(glm::vec3(ti.lightmapVecs[0][0], ti.lightmapVecs[0][1], ti.lightmapVecs[0][2]), pos) + ti.lightmapVecs[0][3];
    float v = glm::dot(glm::vec3(ti.lightmapVecs[1][0], ti.lightmapVecs[1][1], ti.lightmapVecs[1][2]), pos) + ti.lightmapVecs[1][3];
    u -= (float)face.LightmapTextureMinsInLuxels[0];
    v -= (float)face.LightmapTextureMinsInLuxels[1];
    return lm.atlasOffset + glm::vec2((u + 0.5f) / lm.luxelW, (v + 0.5f) / lm.luxelH) * lm.atlasScale;
}

} // namespace veex