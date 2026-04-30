#pragma once
#include <glad/gl.h>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>

namespace veex {

class GameInfo;

// Forward declaration for VMT support
struct VMTMaterial;

// ── Material ─────────────────────────────────────────────────────────────────
// All GL texture handles. 0 = not present / use scalar fallback.
struct Material {
    std::string diffusePath;
    std::string metallicPath;
    std::string roughnessPath;
    std::string normalPath;
    std::string detailPath;    // Detail texture for $detail blending

    GLuint textureID    = 0;   // Albedo / main texture
    GLuint diffuseID    = 0;   // Legacy alias for textureID
    GLuint metallicID   = 0;
    GLuint roughnessID  = 0;
    GLuint normalID     = 0;
    GLuint specMaskID   = 0;
    GLuint detailID     = 0;   // Detail texture for $detail blending
    GLuint shaderID     = 0;

    // ── VMT Material Data ──────────────────────────────────────────────────────
    // When loaded from a .vmt file, these fields contain the parsed data.
    std::shared_ptr<VMTMaterial> vmtData;

    // ── UV Transform (from VMT $texscale, $bumpscale, offsets) ─────────────────
    glm::vec2 baseTextureScale  = glm::vec2(1.0f);
    glm::vec2 baseTextureOffset = glm::vec2(0.0f);
    glm::vec2 bumpTextureScale  = glm::vec2(1.0f);
    glm::vec2 bumpTextureOffset = glm::vec2(0.0f);
    
    // ── Normal Map Parameters ────────────────────────────────────────────────────
    float normalMapStrength = 1.0f;  // Normal map intensity (0.0 = flat, 1.0 = full)

    // ── Detail Texture Parameters ──────────────────────────────────────────────
    float detailScale       = 1.0f;
    float detailBlendFactor = 1.0f;
    int   detailBlendMode   = 0;
    float bumpScale         = 1.0f;  // Normal map depth/bump scale from VMT

    // ── Environment Map Parameters ─────────────────────────────────────────────
    glm::vec3 envMapTint     = glm::vec3(1.0f);
    float     envMapSaturation = 1.0f;
    float     envMapContrast   = 1.0f;

    // ── Material Flags ─────────────────────────────────────────────────────────
    bool isTranslucent = false;
    bool isAlphaTested = false;
    bool isTwoSided    = false;
    bool isNoCull      = false;
    bool hasEnvMap     = false;
    bool enableRNM     = false;  // Enable Radiosity Normal Mapping

    // ── Shader Feature Flags ───────────────────────────────────────────────────
    uint32_t shaderFlags = 0;

    // ── Atlas Support ──────────────────────────────────────────────────────────
    // When using texture atlases, these fields store atlas-specific information
    bool useAtlas = false;           // Whether this material uses atlas textures
    int atlasAllocationID = -1;      // Allocation ID in the atlas
    glm::vec4 atlasUVCrop = glm::vec4(0.0f); // UV crop coordinates for atlas

    // ── Depth Map Atlas Support ───────────────────────────────────────────────
    // Generated height maps for parallax mapping, stored in a MegaTexture atlas
    bool hasDepthMap = false;        // Whether a depth map was generated
    glm::vec4 depthAtlasUVCrop = glm::vec4(0.0f); // UV crop for depth atlas
    GLuint depthAtlasTextureID = 0;  // Atlas texture ID (shared across materials)

    bool hasDiffuse() const { return textureID != 0; }
    bool hasMetallic() const { return metallicID != 0; }
    bool hasRoughness() const { return roughnessID != 0; }
    bool hasNormal() const { return normalID != 0; }
    bool hasDetail() const { return detailID != 0; }
    bool hasRNM() const { return enableRNM && hasNormal(); }
};

// ── MaterialSystem ───────────────────────────────────────────────────────────
class MaterialSystem {
public:
    static MaterialSystem& Get() {
        static MaterialSystem instance;
        return instance;
    }

    void     Initialize(const GameInfo& game);
    void     Shutdown();
    Material GetMaterial(const std::string& name);

    Material LoadMaterial(const std::string& basePath);

    // Get texture data from existing GL texture for atlas packing
    bool GetTextureData(GLuint textureID, std::vector<uint8_t>& outData,
                       int& outWidth, int& outHeight, GLenum& outFormat) const;

private:
    MaterialSystem() = default;

    uint32_t CreateDummyTexture();

    // Try to find <baseName><suffix>.<ext> on disk.
    // Returns 0 if not found (caller uses scalar fallback).
    uint32_t TryLoadSuffix(const std::string& baseKey, const std::string& suffix);

    uint32_t m_fallbackTexture = 0;
    uint32_t m_defaultShader   = 0;

    // Cache stores the full Material so PBR maps are resolved once.
    std::unordered_map<std::string, Material> m_materialCache;
    std::mutex    m_cacheMutex;

    const GameInfo* m_gameInfo = nullptr;

    GLuint LoadTexture(const std::string& path, bool srgb = false);
};

} // namespace veex
