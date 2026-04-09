#pragma once
#include <glad/gl.h>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iostream>

namespace veex {

class GameInfo;

// ── Material ─────────────────────────────────────────────────────────────────
// All GL texture handles. 0 = not present / use scalar fallback.
struct Material {
    std::string diffusePath;
    std::string metallicPath;
    std::string roughnessPath;
    std::string normalPath;

    GLuint textureID  = 0;   // Albedo / main texture
    GLuint diffuseID  = 0;   // Legacy alias for textureID
    GLuint metallicID = 0;
    GLuint roughnessID= 0;
    GLuint normalID   = 0;
    GLuint specMaskID = 0;
    GLuint shaderID   = 0;

    bool hasDiffuse() const { return textureID != 0; }
    bool hasMetallic() const { return metallicID != 0; }
    bool hasRoughness() const { return roughnessID != 0; }
    bool hasNormal() const { return normalID != 0; }
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
