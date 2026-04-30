#pragma once

#include "veex/Common.h"
#include <glad/gl.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace veex {

/**
 * TextureAtlas - MegaTexture implementation for BSP geometry textures
 * 
 * Creates compressed texture atlases that multiple materials can share,
 * reducing texture binding overhead and improving rendering performance.
 * 
 * Key features:
 * - 4x4 block alignment for DXT compression compatibility
 * - Automatic padding to prevent texture bleeding
 * - Dual-atlas support: Color (DXT1) + Alpha (DXT5) for optimal VRAM usage
 * - Platform-aware format selection (DXT for Windows/Linux, RGBA8 for macOS)
 * - Simple first-fit bin packing allocator
 * 
 * Usage:
 * 1. Initialize with desired atlas size (e.g., 4096x4096)
 * 2. Allocate textures using AllocateTexture()
 * 3. Use returned allocation ID to get UV coordinates
 * 4. Bind atlas texture and use UV crop in shaders
 */
class TextureAtlas {
public:
    TextureAtlas();
    ~TextureAtlas();

    // Prevent copying
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    /**
     * Initialize the atlas with specified dimensions
     * @param width Atlas width (must be multiple of 4, recommended 4096)
     * @param height Atlas height (must be multiple of 4, recommended 4096)
     * @return true if initialization succeeded
     */
    bool Initialize(int width = 4096, int height = 4096);

    /**
     * Shutdown and release all resources
     */
    void Shutdown();

    /**
     * Allocate space for a texture in the atlas
     * @param width Texture width (will be aligned to 4px)
     * @param height Texture height (will be aligned to 4px)
     * @param data Pointer to compressed/uncompressed texture data
     * @param dataSize Size of data in bytes
     * @param format OpenGL format of the input data
     * @param inputWidth Actual width of input data (before alignment)
     * @param inputHeight Actual height of input data (before alignment)
     * @return Allocation ID (>=0) or -1 if allocation failed
     */
    int AllocateTexture(int width, int height, 
                       const uint8_t* data, size_t dataSize,
                       GLenum format,
                       int inputWidth = 0, int inputHeight = 0);

    /**
     * Free an allocation
     * @param allocationID ID returned from AllocateTexture
     */
    void Free(int allocationID);

    /**
     * Get the OpenGL texture ID for the atlas
     */
    GLuint GetTextureID() const { return m_atlasTexture; }

    /**
     * Get UV crop coordinates for an allocation
     * Returns vec4(x_offset, y_offset, x_scale, y_scale)
     * where x_offset/y_offset are normalized [0,1] coordinates
     * and x_scale/y_scale are the UV scaling factors
     */
    glm::vec4 GetUVCrop(int allocationID) const;

    /**
     * Get allocation dimensions
     */
    bool GetAllocationSize(int allocationID, int& width, int& height) const;

    /**
     * Check if atlas is initialized
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * Get atlas dimensions
     */
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    /**
     * Get memory usage statistics
     */
    size_t GetUsedMemory() const;
    size_t GetTotalMemory() const;
    float GetUsagePercentage() const;

    /**
     * Check if DXT compression is supported on this platform
     */
    static bool IsDXTSupported();

    /**
     * Set the requested format for the atlas
     */
    void SetFormat(GLenum format) { m_requestedFormat = format; }

    /**
     * Extract the full atlas data as RGBA8 for compression/saving
     * @param outData Output buffer (must be at least width * height * 4 bytes)
     */
    void ExtractRGBA8Data(uint8_t* outData) const;

    /**
     * Get the internal OpenGL format
     */
    GLenum GetInternalFormat() const { return m_internalFormat; }

private:
    struct Allocation {
        int x, y;              // Position in atlas (pixels)
        int width, height;     // Allocated size (aligned to 4px)
        int inputWidth, inputHeight; // Original input size (before alignment)
        bool free;
        int id;
    };

    // Create the OpenGL texture with appropriate format
    bool CreateGLTexture();

    // Upload data to a specific region of the atlas
    void UploadData(int x, int y, int width, int height,
                   const uint8_t* data, size_t dataSize, GLenum format);

    // Find best fit allocation spot (first-fit algorithm)
    bool FindAllocationSpot(int width, int height, int& outX, int& outY);

    // Generate next allocation ID
    int GenerateAllocationID();

    // Constants
    static constexpr int PADDING_PIXELS = 8;  // Padding between textures
    static constexpr int BLOCK_SIZE = 4;       // DXT block size

    // State
    bool m_initialized = false;
    int m_width = 0;
    int m_height = 0;
    GLuint m_atlasTexture = 0;
    GLenum m_internalFormat = 0;
    GLenum m_requestedFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // Default to DXT5
    
    std::vector<Allocation> m_allocations;
    std::unordered_map<int, size_t> m_allocationIndex; // ID -> index mapping
    int m_nextID = 1;

    // Platform format selection
    GLenum DetermineFormat();
    GLenum GetUploadFormat(GLenum sourceFormat) const;
};

} // namespace veex