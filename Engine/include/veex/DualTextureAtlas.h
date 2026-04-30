#pragma once

#include "veex/Common.h"
#include "veex/TextureAtlas.h"
#include <unordered_map>
#include <string>

namespace veex {

/**
 * DualTextureAtlas - MegaTexture implementation using separate Color + Alpha atlases
 * 
 * Creates two compressed texture atlases:
 * - Color Atlas: DXT1 for RGB data (no alpha)
 * - Alpha Atlas: DXT5 for alpha channel data
 * 
 * This provides optimal VRAM usage (DXT1 + DXT5) while avoiding format mixing issues.
 * The shader combines both atlases to reconstruct the final RGBA texture.
 * 
 * Key features:
 * - 4x4 block alignment for DXT compression compatibility
 * - Automatic padding to prevent texture bleeding
 * - Platform-aware format selection (DXT for Windows/Linux, RGBA8 for macOS)
 * - Simple first-fit bin packing allocator
 * - Shader combines color + alpha channels for final texture
 * 
 * Usage:
 * 1. Initialize with desired atlas size (e.g., 4096x4096)
 * 2. Allocate textures using AllocateTexture()
 * 3. Use returned allocation ID to get UV coordinates for both atlases
 * 4. Bind both atlas textures and use UV crop in shaders
 */
class DualTextureAtlas {
public:
    DualTextureAtlas();
    ~DualTextureAtlas();

    // Prevent copying
    DualTextureAtlas(const DualTextureAtlas&) = delete;
    DualTextureAtlas& operator=(const DualTextureAtlas&) = delete;

    /**
     * Initialize both atlases with specified dimensions
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
     * Allocate space for a texture in the atlases
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
     * Get the OpenGL texture IDs for both atlases
     */
    GLuint GetColorAtlasID() const { return m_colorAtlas.GetTextureID(); }
    GLuint GetAlphaAtlasID() const { return m_alphaAtlas.GetTextureID(); }

    /**
     * Get references to the atlases for extraction
     */
    TextureAtlas& GetColorAtlas() { return m_colorAtlas; }
    const TextureAtlas& GetColorAtlas() const { return m_colorAtlas; }
    TextureAtlas& GetAlphaAtlas() { return m_alphaAtlas; }
    const TextureAtlas& GetAlphaAtlas() const { return m_alphaAtlas; }

    /**
     * Get UV crop coordinates for an allocation
     * Returns vec4(x_offset, y_offset, x_scale, y_scale) for both atlases
     */
    glm::vec4 GetColorUVCrop(int allocationID) const;
    glm::vec4 GetAlphaUVCrop(int allocationID) const;

    /**
     * Get allocation dimensions
     */
    bool GetAllocationSize(int allocationID, int& width, int& height) const;

    /**
     * Check if atlases are initialized
     */
    bool IsInitialized() const { return m_colorAtlas.IsInitialized(); }

    /**
     * Get atlas dimensions
     */
    int GetWidth() const { return m_colorAtlas.GetWidth(); }
    int GetHeight() const { return m_colorAtlas.GetHeight(); }

    /**
     * Get memory usage statistics
     */
    size_t GetUsedMemory() const;
    size_t GetTotalMemory() const;
    float GetUsagePercentage() const;

private:
    // Color atlas (DXT1 for RGB)
    TextureAtlas m_colorAtlas;
    
    // Alpha atlas (DXT5 for alpha channel)
    TextureAtlas m_alphaAtlas;
    
    // Allocation tracking
    std::unordered_map<int, size_t> m_allocationIndex; // ID -> index mapping
    int m_nextID = 1;
};

} // namespace veex