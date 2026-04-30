// veex/BSP.cpp
// BSP world loader, batch builder, and MikkTSpace tangent generator.

#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/MaterialSystem.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Shader.h"
#include "veex/BSPTexturePacker.h"
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
    
    // Pack BSP textures into atlas using GameInfo for cache paths
    BSPTexturePacker packer;
    bool atlasSuccess = packer.PackTextures(*this, MaterialSystem::Get(), game);
    
    if (atlasSuccess) {
        Logger::Info("BSP texture atlas created successfully");
        m_texturePacker = std::make_unique<BSPTexturePacker>();
        // Copy the packed data manually since copy constructor is deleted
        // For now, we'll just create a new packer and mark it as active
        m_texturePacker->PackTextures(*this, MaterialSystem::Get(), game);
    } else {
        Logger::Warn("BSP texture atlas creation failed, falling back to individual textures");
    }
    
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

    std::unordered_map<BatchKey, std::vector<Vertex>> materialVerts;
    std::unordered_map<BatchKey, RenderBatch> materialMeta;

    auto ComputeDirectionBatchID = [](const glm::vec3& normal) {
        glm::vec3 absN = glm::abs(normal);
        if (absN.x >= absN.y && absN.x >= absN.z)
            return normal.x >= 0.0f ? 1u : 2u;
        if (absN.y >= absN.x && absN.y >= absN.z)
            return normal.y >= 0.0f ? 3u : 4u;
        return normal.z >= 0.0f ? 5u : 6u;
    };

    for (int fi = 0; fi < (int)faces.size(); ++fi) {
        const dface_t& f = faces[fi];
        
        // Use new ShouldRenderFace() method for proper flag checking
        if (!m_parser.ShouldRenderFace(fi)) continue;
        if (f.texinfo < 0) continue;
        
        const texinfo_t& ti = texinfo[f.texinfo];

        std::vector<glm::vec3> facePoints;
        m_parser.GetFaceVertices(f, facePoints);
        if (facePoints.size() < 3) continue;

        Material mat = MaterialSystem::Get().GetMaterial(m_parser.GetTextureName(f.texinfo));
        glm::vec3 faceNormal = SourceToGL(m_parser.GetFaceNormal(f));
        uint32_t orientationID = ComputeDirectionBatchID(faceNormal);

        // Compute shader flags based on material properties
        uint32_t shaderFlags = 0;
        if (mat.normalID != 0) shaderFlags |= SHADER_FEATURE_NORMAL_MAP;
        if (mat.roughnessID != 0) shaderFlags |= SHADER_FEATURE_SPECULAR;
        if (mat.metallicID != 0) shaderFlags |= SHADER_FEATURE_PBR;
        if (mat.detailID != 0) shaderFlags |= SHADER_FEATURE_DETAIL;

        BatchKey key;
        key.textureID    = mat.textureID;
        key.normalID     = mat.normalID;
        key.roughnessID  = mat.roughnessID;
        key.metallicID   = mat.metallicID;
        key.specMaskID   = mat.specMaskID;
        key.detailID     = mat.detailID;
        key.texinfoID    = static_cast<uint32_t>(f.texinfo);
        key.orientationID= orientationID;
        key.shaderFlags  = shaderFlags;

        // ── Height Map Atlas ─────────────────────────────────────────────────
        // All depth maps share the same atlas texture ID.
        if (m_texturePacker && m_texturePacker->GetDepthAtlas().IsReady()) {
            glm::vec4 depthCrop = m_texturePacker->GetDepthMapUVCrop(m_parser.GetTextureName(f.texinfo));
            if (depthCrop.z > 0.0f && depthCrop.w > 0.0f) {
                key.heightID = m_texturePacker->GetDepthAtlas().GetAtlasTextureID();
            }
        }

        if (materialMeta.find(key) == materialMeta.end()) {
            RenderBatch rb;
            rb.key = key;
            // Defaulting to 0.7-0.8 roughness for non-map materials to kill the "plastic" look
            rb.matParams.roughness = (mat.roughnessID != 0) ? 1.0f : 0.7f;
            rb.matParams.metallic  = (mat.metallicID != 0) ? 1.0f : 0.0f;
            rb.matParams.hasNormalMap = (mat.normalID != 0) ? 1 : 0;
            rb.matParams.enableRNM = rb.matParams.hasNormalMap;  // Enable RNM when normal map is present
            rb.matParams.hasRoughnessMap = (mat.roughnessID != 0) ? 1 : 0;
            rb.matParams.hasMetallicMap = (mat.metallicID != 0) ? 1 : 0;
            rb.matParams.hasSpecMaskMap = (mat.specMaskID != 0) ? 1 : 0;
            rb.matParams.hasDetail = (mat.detailID != 0) ? 1 : 0;
            
            // VMT detail texture parameters
            rb.matParams.detailScale = mat.detailScale;
            rb.matParams.detailBlendFactor = mat.detailBlendFactor;
            rb.matParams.detailBlendMode = mat.detailBlendMode;
            
            // ── Height Map Atlas ───────────────────────────────────────────────
            // If the texture packer generated a depth map for this material,
            // configure the batch to use the depth atlas.
            if (m_texturePacker && m_texturePacker->GetDepthAtlas().IsReady()) {
                glm::vec4 depthCrop = m_texturePacker->GetDepthMapUVCrop(m_parser.GetTextureName(f.texinfo));
                if (depthCrop.z > 0.0f && depthCrop.w > 0.0f) {
                    rb.matParams.useHeightAtlas = true;
                    rb.matParams.heightAtlasUVCrop = depthCrop;
                    // All depth maps share the same atlas texture
                    key.heightID = m_texturePacker->GetDepthAtlas().GetAtlasTextureID();
                }
            }
            
            materialMeta[key] = rb;
        }

        glm::ivec2 dim = m_parser.GetTextureDimensions(f.texinfo);
        bool hasLM = (fi < (int)lmInfos.size() && lmInfos[fi].valid);

        for (size_t i = 1; i + 1 < facePoints.size(); ++i) {
            glm::vec3 tri[3] = { facePoints[0], facePoints[i], facePoints[i+1] };
            for (int k = 0; k < 3; ++k) {
                Vertex v;
                v.position = SourceToGL(tri[k]) * kWorldScale;
                v.normal   = faceNormal;
                v.texCoord = ComputeTexCoord(tri[k], ti, (float)dim.x, (float)dim.y);
                if (hasLM) {
                    glm::vec2 lmCoord = ComputeLightmapCoord(tri[k], ti, f, lmInfos[fi]);
                    v.lmCoord0 = lmCoord;
                    v.lmCoord1 = lmCoord;
                    v.lmCoord2 = lmCoord;
                } else {
                    v.lmCoord0 = glm::vec2(-1.0f);
                    v.lmCoord1 = glm::vec2(-1.0f);
                    v.lmCoord2 = glm::vec2(-1.0f);
                }
                materialVerts[key].push_back(v);
            }
        }
    }

    for (auto& [key, verts] : materialVerts) {
        MikkContext mikkCtx{ &verts };
        SMikkTSpaceContext ctx{};
        ctx.m_pInterface = &kMikkInterface;
        ctx.m_pUserData  = &mikkCtx;
        genTangSpaceDefault(&ctx);

        RenderBatch batch = materialMeta[key];
        
        // Finalize state flags before pushing
        batch.matParams.hasNormalMap = (key.normalID != 0) ? 1 : 0;
        
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
    auto visFaces = m_parser.GetVisibleFaceIndices(worldPos);
    if (visFaces.empty()) return m_batches;
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
    // atlasOffset and atlasScale are already normalized to [0,1]
    // u, v are in luxel coordinates (0 to luxelW-1, 0 to luxelH-1)
    // Normalize u, v to [0,1] and scale by atlasScale, then add atlasOffset
    return lm.atlasOffset + glm::vec2((u + 0.5f) / lm.luxelW, (v + 0.5f) / lm.luxelH) * lm.atlasScale;
}

bool BSP::IsOccluded(const glm::vec3& start, const glm::vec3& end) const {
    // Convert world positions back to Source Engine units if necessary
    // (kWorldScale is 1/32, so multiply by 32 to get back to BSP units)
    glm::vec3 p1 = start * (1.0f / kWorldScale);
    glm::vec3 p2 = end * (1.0f / kWorldScale);

    return TraceRay(0, 0.0f, 1.0f, p1, p2);
}

bool BSP::TraceRay(int nodeIdx, float tStart, float tEnd, const glm::vec3& p1, const glm::vec3& p2) const {
    // If we hit a leaf (negative index)
    if (nodeIdx < 0) {
        int leafIdx = -(nodeIdx + 1);
        const auto& leaf = m_parser.GetLeaves()[leafIdx];
        
        // If the leaf is solid (contents & CONTENTS_SOLID), it's occluded
        // In Source BSP, solid is usually 0x1
        return (leaf.contents & 0x1); 
    }

    const auto& node = m_parser.GetNodes()[nodeIdx];
    const auto& plane = m_parser.GetPlanes()[node.planenum];

    float d1 = glm::dot(glm::vec3(plane.normal.x, plane.normal.y, plane.normal.z), p1) - plane.dist;
    float d2 = glm::dot(glm::vec3(plane.normal.x, plane.normal.y, plane.normal.z), p2) - plane.dist;

    if (d1 >= 0 && d2 >= 0) return TraceRay(node.children[0], tStart, tEnd, p1, p2);
    if (d1 < 0 && d2 < 0) return TraceRay(node.children[1], tStart, tEnd, p1, p2);

    // Ray crosses the plane
    float t = d1 / (d1 - d2);
    
    // Check front side first
    if (TraceRay(node.children[d1 < 0], tStart, t * tEnd, p1, p2)) return true;
    // Then check back side
    return TraceRay(node.children[d1 >= 0], t * tEnd, tEnd, p1, p2);
}

} // namespace veex