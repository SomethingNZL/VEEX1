#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <glm/glm.hpp>

namespace veex {

// Forward declarations
struct GameInfo;
class VPKManager;

// ── Texture Info for Cache ─────────────────────────────────────────────────
struct TextureInfo {
    std::string name;
    glm::vec4 uvCrop;  // x, y, width, height in normalized coords
};

// ── MegaTexture Cache Manager ────────────────────────────────────────────────
//
// This class manages the MegaTexture cache files (.mtexi and .megatex).
// Cache files are stored in regular filesystem directories (NOT in VPKs,
// since VPKs are read-only archives).
//
// The cache uses the first writable search path from GameInfo for cache storage.
class MegaTextureCache {
public:
    static constexpr uint32_t MAGIC = 0x4D5458;  // "MTX"
    static constexpr uint32_t VERSION = 1;
    
    struct Header {
        uint32_t magic;
        uint32_t version;
        uint32_t atlasWidth;
        uint32_t atlasHeight;
        uint32_t textureCount;
        uint32_t dataSize;     // Size of compressed texture data
        uint32_t checksum;     // Checksum of all texture data
    };
    
    MegaTextureCache() = default;
    
    // Calculate CRC32 (public for external use)
    uint32_t CalculateCRC32(const uint8_t* data, size_t size);
    
    // Check if cache exists and is valid for given map
    // Uses GameInfo to find the correct cache directory (first writable path)
    static bool HasValidCache(const std::string& mapName, const GameInfo& gameInfo);
    
    // Load cached megatexture - returns true if loaded successfully
    // For now, just validates that cache exists
    static bool LoadCache(const std::string& mapName, const GameInfo& gameInfo);
    
    // Build and save new cache
    // Writes to the first writable directory from GameInfo search paths
    static bool BuildAndSaveCache(const std::string& mapName,
                                 int atlasWidth, int atlasHeight,
                                 const std::vector<TextureInfo>& textures,
                                 const std::vector<uint8_t>& compressedData,
                                 const GameInfo& gameInfo);
    
    // Get the cache directory path from GameInfo
    static std::string GetCacheDirectory(const GameInfo& gameInfo);
};

} // namespace veex
