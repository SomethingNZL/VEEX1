#pragma once

#include "veex/MegaTexture.h"
#include "veex/GameInfo.h"
#include <glad/gl.h>
#include <string>
#include <unordered_map>

namespace veex {

/**
 * DepthMapAtlas — Stores generated depth/height maps in a MegaTexture atlas.
 *
 * Each material with a normal map gets its depth map generated and packed
 * into a shared atlas texture.  The atlas texture is bound once per frame
 * (or per atlas switch) and per-batch UV crops tell the shader where to
 * sample.
 *
 * Usage:
 *   1. Initialize with GameInfo (for cache path resolution)
 *   2. GenerateAndPack() for each material that has a normal map
 *   3. Bind() before rendering
 *   4. GetUVCrop() per material for the shader uniform
 */
class DepthMapAtlas {
public:
    DepthMapAtlas();
    ~DepthMapAtlas();

    // Non-copyable (GPU resources)
    DepthMapAtlas(const DepthMapAtlas&) = delete;
    DepthMapAtlas& operator=(const DepthMapAtlas&) = delete;

    /**
     * Initialize the depth map atlas.
     * @param gameInfo   Used for cache directory resolution.
     * @param atlasSize  Width/height of the atlas texture (default 4096).
     * @return true if initialization succeeded.
     */
    bool Initialize(const GameInfo& gameInfo, int atlasSize = 4096);

    /**
     * Shutdown and release GPU resources.
     */
    void Shutdown();

    /**
     * Generate a depth map from normal (+ optional diffuse) data and pack it
     * into the atlas.  If the material was already packed, returns true
     * immediately without re-generation.
     *
     * @param materialName  Unique name used as the lookup key.
     * @param normalData    RGBA8 tangent-space normal map.
     * @param normalW       Normal map width.
     * @param normalH       Normal map height.
     * @param diffuseData   Optional RGBA8 diffuse for low-frequency bias.
     * @param diffuseW      Diffuse width (ignored if diffuseData is null).
     * @param diffuseH      Diffuse height (ignored if diffuseData is null).
     * @return true if the depth map was generated and packed.
     */
    bool GenerateAndPack(const std::string& materialName,
                         const uint8_t* normalData, int normalW, int normalH,
                         const uint8_t* diffuseData = nullptr,
                         int diffuseW = 0, int diffuseH = 0);

    /**
     * Manually pack pre-baked depth map data.
     */
    bool PackDepthMap(const std::string& materialName,
                      const uint8_t* data, int width, int height);

    /**
     * Look up a packed depth map.
     * @return Pointer to TextureSlot, or nullptr if not found.
     */
    const MegaTexture::TextureSlot* FindDepthMap(const std::string& materialName) const;

    /**
     * Get the atlas UV crop for a material.
     * Returns vec4(x_offset, y_offset, x_scale, y_scale) in normalized [0,1].
     */
    glm::vec4 GetUVCrop(const std::string& materialName) const;

    /**
     * Get the OpenGL texture handle of the atlas.
     */
    GLuint GetAtlasTextureID() const;

    /**
     * Bind the atlas to a texture unit.
     */
    void Bind(int textureUnit) const;

    /**
     * Check if the atlas is ready for use.
     */
    bool IsReady() const;

    /**
     * Get statistics about the atlas.
     */
    MegaTexture::Stats GetStats() const;

private:
    MegaTexture m_megaTexture;
    bool m_initialized = false;
};

} // namespace veex
