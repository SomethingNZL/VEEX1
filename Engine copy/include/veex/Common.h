#pragma once
// veex/Common.h
// Central GPU-facing data structures shared by BSP, Renderer, and ShaderKit.
//
// Key additions over the previous version:
//   • BatchKey  — fully describes a batch's state for draw-call grouping.
//   • RenderBatch — carries a BatchKey plus a pre-baked MaterialParams so
//     ShaderKit never has to reverse-engineer per-batch material state.

#include <cstdint>
#include <vector>
#include <functional>   // std::hash
#include <glm/glm.hpp>

namespace veex {

// ── Per-vertex data uploaded to the GPU ──────────────────────────────────────
// Layout matches the glVertexAttribPointer calls in Renderer::Init exactly.
// attrib 0: position   (vec3)
// attrib 1: normal     (vec3)
// attrib 2: texCoord   (vec2) — albedo UV
// attrib 3: lmCoord0   (vec2) — lightmap UV for RNM basis 0
// attrib 4: tangent    (vec4) — xyz = tangent direction, w = bitangent sign
// attrib 5: lmCoord1   (vec2) — lightmap UV for RNM basis 1
// attrib 6: lmCoord2   (vec2) — lightmap UV for RNM basis 2
// attrib 7: faceNormal (vec3) — face normal for RNM basis orientation
//
// The tangent is computed by MikkTSpace (industry-standard) and must be
// decoded in the shader as:  bitangent = cross(normal, tangent.xyz) * tangent.w
//
// RNM (Radiosity Normal Mapping): Three lightmap coordinates are stored per vertex
// to support Source Engine's RNM technique. Each coordinate samples a different
// directional component of the radiosity lighting.
// The face normal is used to orient the RNM basis vectors correctly.
struct Vertex {
    glm::vec3 position;   // attrib 0
    glm::vec3 normal;     // attrib 1
    glm::vec2 texCoord;   // attrib 2
    glm::vec2 lmCoord0;   // attrib 3 - RNM basis 0 lightmap UV
    glm::vec4 tangent;    // attrib 4  (MikkTSpace output)
    glm::vec2 lmCoord1;   // attrib 5 - RNM basis 1 lightmap UV
    glm::vec2 lmCoord2;   // attrib 6 - RNM basis 2 lightmap UV
    glm::vec3 faceNormal; // attrib 7 - face normal for RNM basis orientation
};

// ── BatchKey ──────────────────────────────────────────────────────────────────
// All fields that must agree for two triangles to share a draw call.
// The batch system groups geometry by this key so the GPU state never
// changes mid-batch — every batch is one glDrawArrays call.
//
// Fields ordered largest → smallest to avoid padding waste.
struct BatchKey {
    uint32_t textureID    = 0;   // GL handle — albedo     (unit 0)
    uint32_t normalID     = 0;   // GL handle — normal map (unit 4);  0 = fallback
    uint32_t roughnessID  = 0;   // GL handle — roughness  (unit 2);  0 = scalar
    uint32_t metallicID   = 0;   // GL handle — metallic   (unit 3);  0 = scalar
    uint32_t emissiveID   = 0;   // GL handle — emissive   (unit 5);  0 = none
    uint32_t specMaskID   = 0;   // GL handle — Source specmask-style combined maps
    uint32_t detailID     = 0;   // GL handle — detail texture (unit 7);  0 = none
    uint32_t heightID     = 0;   // GL handle — height/displacement (unit 9); 0 = none
    uint32_t texinfoID    = 0;   // BSP texinfo index, preserves texture mapping/orientation
    uint32_t orientationID= 0;   // Encoded major face direction
    uint32_t shaderFlags  = 0;   // ShaderFeatureFlags bitmask for this material

    // Two batches may be merged only if every field is identical.
    bool operator==(const BatchKey& o) const noexcept {
        return textureID    == o.textureID
            && normalID     == o.normalID
            && roughnessID  == o.roughnessID
            && metallicID   == o.metallicID
            && specMaskID   == o.specMaskID
            && emissiveID   == o.emissiveID
            && detailID     == o.detailID
            && heightID     == o.heightID
            && texinfoID    == o.texinfoID
            && orientationID== o.orientationID
            && shaderFlags  == o.shaderFlags;
    }
};

} // namespace veex

// ── std::hash specialisation for BatchKey ────────────────────────────────────
// Required so BatchKey can be used as an unordered_map key in the batch system.
namespace std {
template<> struct hash<veex::BatchKey> {
    size_t operator()(const veex::BatchKey& k) const noexcept {
        // FNV-1a inspired mixing — cheap and low-collision for small integer sets.
        size_t h = 0xcbf29ce484222325ULL;
        auto mix = [&](uint32_t v) {
            h ^= static_cast<size_t>(v);
            h *= 0x100000001b3ULL;
        };
        mix(k.textureID);
        mix(k.normalID);
        mix(k.roughnessID);
        mix(k.metallicID);
        mix(k.specMaskID);
        mix(k.emissiveID);
        mix(k.heightID);
        mix(k.texinfoID);
        mix(k.orientationID);
        mix(k.shaderFlags);
        return h;
    }
};
} // namespace std

namespace veex {

// ── RenderBatch ───────────────────────────────────────────────────────────────
// Everything the Renderer needs to issue one draw call.
// All geometry in [offset, offset+count) shares the same GPU state.
struct RenderBatch {
    BatchKey key;             // state identity — set by BSP::BuildVertexBuffer

    // ── Geometry slice in the VBO ─────────────────────────────────────────────
    uint32_t offset = 0;     // first vertex index
    uint32_t count  = 0;     // vertex count

    // ── Per-material scalar/vector values for ShaderKit ───────────────────────
    // Baked at batch-build time so the Renderer can call
    //   shader.UploadMaterialParams(batch.matParams)
    // without touching the material system at render time.
    // Forward-declared here; defined in Shader.h.
    struct MaterialParams {
        float roughness        = 0.7f;
        float metallic         = 0.0f;
        float alphaRef         = 0.5f;
        float emissiveScale    = 1.0f;
        float refractionScale  = 0.05f;
        float envmapTint       = 1.0f;
        float lightmapBrightness = 1.0f;  // Lightmap brightness multiplier
        bool  hasNormalMap     = false;
        bool  hasRoughnessMap  = false;
        bool  hasMetallicMap   = false;
        bool  hasSpecMaskMap   = false;
        bool  hasEmissiveMap   = false;
        bool  hasDetail        = false;   // Has detail texture (from VMT)
        bool  enableRNM        = false;   // Enable Radiosity Normal Mapping
        bool  enableSilhouetteParallax = false; // Enable silhouette parallax mapping
        
        // ── VMT Detail Texture Parameters ──────────────────────────────────────
        float detailScale       = 1.0f;   // Detail texture tiling scale
        float detailBlendFactor = 1.0f;   // Detail blend intensity
        int   detailBlendMode   = 0;      // 0=multiply, 1=add, 2=lerp
        
        // ── Normal Map Depth Parameters ────────────────────────────────────────
        float bumpScale         = 1.0f;   // Normal map depth/bump scale from VMT
        
        // ── Silhouette Parallax Parameters ─────────────────────────────────────
        float parallaxScale     = 0.03f;  // Height scale for parallax (typical 0.02-0.1)
        float parallaxMinLayers = 8.0f;   // Min ray-march layers (quality/perf)
        float parallaxMaxLayers = 32.0f;  // Max ray-march layers
        
        // ── Height Map Atlas Parameters ────────────────────────────────────────────
        // When depth maps are stored in an atlas, these tell the shader where to sample
        glm::vec4 heightAtlasUVCrop = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        bool      useHeightAtlas    = false;
    } matParams;

    // ── Convenience accessors (backwards compat) ──────────────────────────────
    uint32_t textureID  () const { return key.textureID;   }
    uint32_t normalID   () const { return key.normalID;    }
    uint32_t roughnessID() const { return key.roughnessID; }
    uint32_t metallicID () const { return key.metallicID;  }
    uint32_t specMaskID () const { return key.specMaskID;  }
};

} // namespace veex
