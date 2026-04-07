#ifndef VEEX_MATERIALSYSTEM_H
#define VEEX_MATERIALSYSTEM_H

#include <string>
#include <cstdint>
#include <unordered_map>

namespace veex {

class GameInfo; // Forward declaration

struct Material {
    uint32_t textureID = 0;
    uint32_t shaderID  = 0;
};

class MaterialSystem {
public:
    static MaterialSystem& Get() {
        static MaterialSystem instance;
        return instance;
    }

    // Must be called once after OpenGL context is ready.
    // Stores the GameInfo so GetMaterial() can resolve search paths later.
    void Initialize(const GameInfo& game);
    void Shutdown();

    // Returns a Material for the given texture name (e.g. "BRICK/BRICKWALL001A").
    // Loads on first use; subsequent calls return from cache.
    // Falls back to magenta dummy texture on failure.
    Material GetMaterial(const std::string& name);

private:
    MaterialSystem() = default;

    uint32_t CreateDummyTexture();
    uint32_t LoadTexture(const std::string& path);

    uint32_t m_fallbackTexture = 0;
    uint32_t m_defaultShader   = 0;

    // Texture cache: name -> GL texture ID
    std::unordered_map<std::string, uint32_t> m_textureCache;

    // Kept so GetMaterial can call ResolveAssetPath without needing a GameInfo arg
    const GameInfo* m_gameInfo = nullptr;
};

} // namespace veex

#endif
