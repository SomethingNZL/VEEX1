#pragma once

#include "veex/Common.h"
#include "veex/TextureAtlas.h"
#include "veex/DualTextureAtlas.h"
#include "veex/MegaTexture.h"
#include "veex/DepthMapAtlas.h"
#include "veex/MaterialSystem.h"
#include "veex/GameInfo.h"
#include <unordered_map>
#include <string>

namespace veex {

class BSP;
class BSPFace;

/**
 * BSPTexturePacker - Handles texture atlas creation for BSP geometry
 * 
 * This class manages the process of:
 * 1. Collecting all material textures used by a BSP map
 * 2. Packing them into a TextureAtlas
 * 3. Updating material references to use atlas coordinates
 * 
 * Usage:
 * - Create instance during BSP loading
 * - Call PackTextures() with the BSP, MaterialSystem, and GameInfo
 * - Materials will automatically use atlas textures
 */
class BSPTexturePacker {
public:
    BSPTexturePacker();
    ~BSPTexturePacker();

    // Prevent copying
    BSPTexturePacker(const BSPTexturePacker&) = delete;
    BSPTexturePacker& operator=(const BSPTexturePacker&) = delete;

    /**
     * Pack all BSP material textures into an atlas
     * @param bsp The BSP map to process
     * @param materialSystem The material system to update
     * @param gameInfo GameInfo for cache path resolution (uses search paths)
     * @param atlasWidth Atlas width (default 4096)
     * @param atlasHeight Atlas height (default 4096)
     * @return true if packing succeeded
     */
    bool PackTextures(const BSP& bsp, MaterialSystem& materialSystem,
                     const GameInfo& gameInfo,
                     int atlasWidth = 4096, int atlasHeight = 4096);

    /**
     * Get the dual atlas instance
     */
    DualTextureAtlas& GetAtlas() { return m_dualAtlas; }
    const DualTextureAtlas& GetAtlas() const { return m_dualAtlas; }

    /**
     * Check if atlas is active
     */
    bool IsAtlasActive() const { return m_dualAtlas.IsInitialized(); }

    /**
     * Get texture allocation ID for a material name
     */
    int GetMaterialAllocationID(const std::string& materialName) const;

    /**
     * Get UV crop for a material
     */
    glm::vec4 GetMaterialUVCrop(const std::string& materialName) const;

    /**
     * Get the depth map atlas
     */
    DepthMapAtlas& GetDepthAtlas() { return m_depthAtlas; }
    const DepthMapAtlas& GetDepthAtlas() const { return m_depthAtlas; }

    /**
     * Get depth map UV crop for a material
     */
    glm::vec4 GetDepthMapUVCrop(const std::string& materialName) const;

    /**
     * Get statistics about the packing process
     */
    struct PackingStats {
        size_t totalTextures;
        size_t packedTextures;
        size_t failedTextures;
        float atlasUsagePercent;
        size_t atlasMemoryUsed;
        size_t atlasMemoryTotal;
    };

    PackingStats GetStats() const;

private:
    /**
     * Collect all unique material names from BSP faces
     */
    std::vector<std::string> CollectMaterialNames(const BSP& bsp) const;

    /**
     * Load and pack a single material texture
     */
    bool PackMaterialTexture(const std::string& materialName, 
                            MaterialSystem& materialSystem);

    /**
     * Get the base texture path for a material
     */
    std::string GetMaterialTexturePath(const std::string& materialName) const;

    /**
     * Load texture data for packing (handles VTF and standard images)
     */
    bool LoadTextureData(const std::string& path, 
                        std::vector<uint8_t>& outData,
                        int& outWidth, int& outHeight,
                        GLenum& outFormat) const;

    // Dual atlas instance (separate DXT1 and DXT5 atlases)
    DualTextureAtlas m_dualAtlas;

    // Depth map atlas (generated from normal maps for parallax mapping)
    DepthMapAtlas m_depthAtlas;

    // Material name -> allocation ID mapping
    std::unordered_map<std::string, int> m_materialAllocations;

    // Statistics
    size_t m_totalTextures = 0;
    size_t m_packedTextures = 0;
    size_t m_failedTextures = 0;
};

} // namespace veex
