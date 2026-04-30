#pragma once

#include "veex/Common.h"
#include "veex/TextureAtlas.h"
#include "veex/DualTextureAtlas.h"
#include <glad/gl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace veex {

class GameInfo;
class MaterialSystem;

// Forward declarations
struct TextureInfo;

/**
 * MegaTexture - Improved MegaTexture implementation for BSP geometry
 * 
 * A complete texture atlas system that packs multiple BSP material textures
 * into large GPU textures for reduced draw calls and better GPU cache utilization.
 * 
 * Key improvements over the old TextureAtlas:
 * - Skyline bin packing algorithm for optimal space usage
 * - Proper cache loading/unloading with actual data restoration
 * - Multi-atlas support for large maps
 * - Mipmap generation for LOD
 * - Thread-safe operations
 * - Comprehensive GL error checking
 * - Streaming support for on-demand texture loading
 * 
 * Architecture:
 * - MegaTexture: High-level API for texture management
 * - TextureAtlas: Low-level GPU texture with bin packing
 * - MegaTextureCache: Persistent storage to disk
 * - Shader uniforms: uAtlasCrop, uAtlasTexture for shader integration
 */
class MegaTexture {
public:
    // Configuration for the mega texture system
    struct Config {
        int atlasWidth = 4096;        // Atlas texture width
        int atlasHeight = 4096;       // Atlas texture height
        int maxAtlases = 4;           // Maximum number of atlas textures
        int padding = 2;              // Padding between textures (pixels)
        bool generateMipmaps = true;  // Generate mipmaps for LOD
        bool useCompression = true;   // Use DXT compression when available
        bool enableCaching = true;    // Enable disk cache
        bool verboseLogging = true;  // Enable detailed logging
    };
    
    // Status of the mega texture system
    enum class Status {
        UNINITIALIZED,
        INITIALIZING,
        READY,
        LOADING_CACHE,
        BUILDING,
        ERROR
    };
    
    // Individual texture slot in the atlas
    struct TextureSlot {
        int atlasIndex = 0;       // Which atlas this texture is in
        int x = 0, y = 0;         // Position in atlas
        int width = 0, height = 0; // Aligned dimensions
        int inputWidth = 0;       // Original input dimensions
        int inputHeight = 0;
        int allocationID = 0;      // ID for this slot
        std::string name;         // Texture/material name
        uint32_t checksum = 0;    // CRC32 for cache validation
    };
    
    // Statistics about atlas usage
    struct Stats {
        int atlasCount = 0;
        int textureCount = 0;
        int failedCount = 0;
        size_t usedMemory = 0;
        size_t totalMemory = 0;
        float usagePercent = 0.0f;
        float fragmentationPercent = 0.0f;
    };
    
    MegaTexture();
    ~MegaTexture();
    
    // Prevent copying (GPU resources can't be duplicated)
    MegaTexture(const MegaTexture&) = delete;
    MegaTexture& operator=(const MegaTexture&) = delete;
    
    /**
     * Initialize the mega texture system
     * @param config Configuration for the system
     * @param gameInfo Game info for cache path resolution
     * @return true if initialization succeeded
     */
    bool Initialize(const Config& config, const GameInfo& gameInfo);
    
    /**
     * Shutdown and release all resources
     */
    void Shutdown();
    
    /**
     * Get current status
     */
    Status GetStatus() const { return m_status; }
    
    /**
     * Pack a texture into the atlas
     * @param name Texture/material name
     * @param data Raw RGBA data
     * @param width Width of the texture
     * @param height Height of the texture
     * @return TextureSlot with atlas coordinates, or nullptr if failed
     */
    const TextureSlot* PackTexture(const std::string& name,
                                   const uint8_t* data,
                                   int width, int height);
    
    /**
     * Pack a pre-compressed DXT texture
     * @param name Texture/material name  
     * @param compressedData DXT compressed data
     * @param width Width of the texture
     * @param height Height of the texture
     * @param format GL compressed format (DXT1/DXT5)
     * @return TextureSlot with atlas coordinates, or nullptr if failed
     */
    const TextureSlot* PackCompressedTexture(const std::string& name,
                                            const uint8_t* compressedData,
                                            int width, int height,
                                            GLenum format);
    
    /**
     * Pack multiple textures at once (batch operation)
     * @param textures Vector of texture info to pack
     * @return Number of successfully packed textures
     */
    int PackTextures(const std::vector<TextureInfo>& textures);
    
    /**
     * Find a texture slot by name
     * @param name Texture name to find
     * @return Pointer to slot, or nullptr if not found
     */
    const TextureSlot* FindTexture(const std::string& name) const;
    
    /**
     * Get UV coordinates for a texture
     * @param name Texture name
     * @return glm::vec4(uOffset, vOffset, uScale, vScale) or zeros if not found
     */
    glm::vec4 GetUVCrop(const std::string& name) const;
    
    /**
     * Get the primary atlas texture ID for binding
     * @param atlasIndex Index of the atlas (0 for primary)
     * @return OpenGL texture ID, or 0 if invalid
     */
    GLuint GetAtlasTextureID(int atlasIndex = 0) const;
    
    /**
     * Get number of atlas textures
     */
    int GetAtlasCount() const { return static_cast<int>(m_atlases.size()); }
    
    /**
     * Bind atlas textures to shader sampler units
     * @param baseUnit Base texture unit to bind to
     * @param shaderID Shader program ID (for validation)
     */
    void BindAtlases(int baseUnit, GLuint shaderID = 0) const;
    
    /**
     * Load from cache
     * @param mapName Name of the map for cache lookup
     * @return true if cache was loaded successfully
     */
    bool LoadFromCache(const std::string& mapName);
    
    /**
     * Save to cache
     * @param mapName Name of the map for cache storage
     * @return true if cache was saved successfully
     */
    bool SaveToCache(const std::string& mapName) const;
    
    /**
     * Check if cache exists for a map
     * @param mapName Name of the map
     * @return true if valid cache exists
     */
    bool HasCache(const std::string& mapName) const;
    
    /**
     * Get statistics
     */
    Stats GetStats() const;
    
    /**
     * Get configuration
     */
    const Config& GetConfig() const { return m_config; }
    
    /**
     * Validate GL state after operations
     * Uses VEEX logging system for error reporting
     * @param operation Description of the operation for logging
     * @return true if no errors occurred
     */
    bool ValidateGLState(const char* operation) const;
    
    /**
     * Force regeneration of all mipmaps
     */
    void GenerateMipmaps();
    
    /**
     * Clear all packed textures and reset atlases
     */
    void Clear();
    
    /**
     * Check if system is ready for use
     */
    bool IsReady() const { return m_status == Status::READY; }
    
private:
    // Internal atlas management
    struct Atlas {
        GLuint textureID = 0;
        int width = 0, height = 0;
        GLenum internalFormat = 0;
        std::vector<int> skyline; // Skyline heights for bin packing
        size_t usedMemory = 0;
        int textureCount = 0;
    };
    
    bool CreateAtlas(Atlas& atlas, int width, int height, GLenum format);
    void DestroyAtlas(Atlas& atlas);
    
    bool FindSlotSkyline(int width, int height, Atlas& atlas, 
                         int& outX, int& outY, int& outAtlasIndex);
    
    bool UploadToAtlas(const Atlas& atlas, int x, int y,
                      int width, int height,
                      const uint8_t* data, size_t dataSize,
                      GLenum uploadFormat);
    
    bool FindSlotForCompressed(int width, int height, GLenum format,
                              int& outX, int& outY, int& outAtlasIndex);
    
    bool UploadCompressedToAtlas(const Atlas& atlas, int x, int y,
                                int width, int height,
                                const uint8_t* data, size_t dataSize,
                                GLenum uploadFormat);
    
    uint32_t CalculateChecksum(const uint8_t* data, size_t size);
    
    // State
    Status m_status = Status::UNINITIALIZED;
    Config m_config;
    const GameInfo* m_gameInfo = nullptr;
    
    std::vector<Atlas> m_atlases;
    std::unordered_map<std::string, TextureSlot> m_textureSlots;
    std::vector<std::string> m_textureOrder; // For consistent ordering
    
    int m_currentAtlasIndex = 0;
    int m_nextSlotID = 1;
    
    // Cache path
    std::string m_cacheDirectory;
    
    // Statistics
    Stats m_stats;
    
    // Last GL error for debugging
    mutable GLenum m_lastGLError = GL_NO_ERROR;
};

/**
 * Texture information for batch packing
 */
struct TextureInfo {
    std::string name;
    std::vector<uint8_t> data; // RGBA data
    int width = 0;
    int height = 0;
    bool isCompressed = false;
    GLenum format = 0; // GL_COMPRESSED_* format if compressed
};

/**
 * MegaTextureCache - Handles disk caching of mega textures
 * 
 * Cache format:
 * - .mtexi: Index file with texture metadata and UV coordinates
 * - .megatex: Compressed texture data
 */
class MegaTextureCache {
public:
    static constexpr uint32_t MAGIC = 0x4D5458;  // "MTX"
    static constexpr uint32_t VERSION = 2;       // Updated version
    
    struct Header {
        uint32_t magic = MAGIC;
        uint32_t version = VERSION;
        uint32_t atlasCount = 0;
        int atlasWidth = 0;
        int atlasHeight = 0;
        uint32_t textureCount = 0;
        uint32_t dataSize = 0;
        uint32_t checksum = 0;
    };
    
    struct TextureEntry {
        std::string name;
        int atlasIndex = 0;
        float uOffset = 0.0f, vOffset = 0.0f;
        float uScale = 0.0f, vScale = 0.0f;
        int inputWidth = 0, inputHeight = 0;
        uint32_t checksum = 0;
    };
    
    MegaTextureCache() = default;
    
    /**
     * Check if cache exists and is valid
     */
    static bool HasValidCache(const std::string& mapName, const GameInfo& gameInfo);
    
    /**
     * Load cache data
     * @param mapName Map name
     * @param gameInfo Game info for path resolution
     * @param outEntries Output texture entries
     * @param outAtlasData Output compressed atlas data
     * @param outHeader Output header info
     * @return true if loaded successfully
     */
    static bool LoadCache(const std::string& mapName,
                          const GameInfo& gameInfo,
                          std::vector<TextureEntry>& outEntries,
                          std::vector<uint8_t>& outAtlasData,
                          Header& outHeader);
    
    /**
     * Save cache data
     */
    static bool SaveCache(const std::string& mapName,
                          const GameInfo& gameInfo,
                          const std::vector<TextureEntry>& entries,
                          const std::vector<uint8_t>& atlasData,
                          const Header& header);
    
    /**
     * Get cache directory path
     */
    static std::string GetCacheDirectory(const GameInfo& gameInfo);
    
    /**
     * Calculate CRC32 checksum
     */
    static uint32_t CalculateCRC32(const uint8_t* data, size_t size);
};

} // namespace veex