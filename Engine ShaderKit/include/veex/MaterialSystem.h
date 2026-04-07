#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <mutex>

namespace veex {

class GameInfo;

// ── Material ─────────────────────────────────────────────────────────────────
// All GL texture handles. 0 = not present / use scalar fallback.
struct Material {
    uint32_t textureID   = 0;  // albedo   (unit 0)
    uint32_t shaderID    = 0;  // reserved
    uint32_t roughnessID = 0;  // roughness map (unit 2) – optional
    uint32_t metallicID  = 0;  // metallic map  (unit 3) – optional
    uint32_t normalID    = 0;  // normal map    (unit 4) – optional
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

private:
    MaterialSystem() = default;

    uint32_t CreateDummyTexture();
    uint32_t LoadTexture(const std::string& path);

    // Try to find <baseName><suffix>.<ext> on disk.
    // Returns 0 if not found (caller uses scalar fallback).
    uint32_t TryLoadSuffix(const std::string& baseKey, const std::string& suffix);

    uint32_t m_fallbackTexture = 0;
    uint32_t m_defaultShader   = 0;

    // Cache stores the full Material so PBR maps are resolved once.
    std::unordered_map<std::string, Material> m_materialCache;
    std::mutex    m_cacheMutex;

    const GameInfo* m_gameInfo = nullptr;
};

} // namespace veex
