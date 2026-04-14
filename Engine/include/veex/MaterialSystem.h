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

    // ── Detail Texture Parameters ──────────────────────────────────────────────
    float detailScale       = 1.0f;
    float detailBlendFactor = 1.0f;
    int   detailBlendMode   = 0;

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

    // ── Shader Feature Flags ───────────────────────────────────────────────────
    uint32_t shaderFlags = 0;

    bool hasDiffuse() const { return textureID != 0; }
    bool hasMetallic() const { return metallicID != 0; }
    bool hasRoughness() const { return roughnessID != 0; }
    bool hasNormal() const { return normalID != 0; }
    bool hasDetail() const { return detailID != 0; }
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
