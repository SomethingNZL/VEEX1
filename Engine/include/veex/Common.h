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
// attrib 3: lmCoord    (vec2) — lightmap UV in [0,1] atlas space
// attrib 4: tangent    (vec4) — xyz = tangent direction, w = bitangent sign
//
// The tangent is computed by MikkTSpace (industry-standard) and must be
// decoded in the shader as:  bitangent = cross(normal, tangent.xyz) * tangent.w
struct Vertex {
    glm::vec3 position;   // attrib 0
    glm::vec3 normal;     // attrib 1
    glm::vec2 texCoord;   // attrib 2
    glm::vec2 lmCoord;    // attrib 3
    glm::vec4 tangent;    // attrib 4  (MikkTSpace output)
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
        
        // ── VMT Detail Texture Parameters ──────────────────────────────────────
        float detailScale       = 1.0f;   // Detail texture tiling scale
        float detailBlendFactor = 1.0f;   // Detail blend intensity
        int   detailBlendMode   = 0;      // 0=multiply, 1=add, 2=lerp
    } matParams;

    // ── Convenience accessors (backwards compat) ──────────────────────────────
    uint32_t textureID  () const { return key.textureID;   }
    uint32_t normalID   () const { return key.normalID;    }
    uint32_t roughnessID() const { return key.roughnessID; }
    uint32_t metallicID () const { return key.metallicID;  }
    uint32_t specMaskID () const { return key.specMaskID;  }
};

} // namespace veex
