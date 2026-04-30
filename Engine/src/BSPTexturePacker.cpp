#include "veex/BSPTexturePacker.h"
#include "veex/BSP.h"
#include "veex/VTFLoader.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include "veex/DXTCompressor.h"
#include "veex/MegaTexture.h"
#include "veex/DepthMapGenerator.h"
#include "stb/stb_image.h"
#include <algorithm>
#include <fstream>

namespace veex {

BSPTexturePacker::BSPTexturePacker() {}

BSPTexturePacker::~BSPTexturePacker() {}

bool BSPTexturePacker::PackTextures(const BSP& bsp, MaterialSystem& materialSystem,
                                   const GameInfo& gameInfo,
                                   int atlasWidth, int atlasHeight) {
    Logger::Info("Starting BSP texture packing process...");

    // Get map name for caching
    std::string mapName = "background01"; // Default fallback

    // Collect all unique material names from BSP faces
    std::vector<std::string> materialNames = CollectMaterialNames(bsp);
    m_totalTextures = materialNames.size();

    Logger::Info("Found " + std::to_string(m_totalTextures) + " unique materials");

    if (m_totalTextures == 0) {
        Logger::Info("No materials to pack, skipping atlas creation");
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // STEP 1: Check for existing cache using GameInfo paths
    // ═══════════════════════════════════════════════════════════════════
    
    if (MegaTextureCache::HasValidCache(mapName, gameInfo)) {
        Logger::Info("Loading megatexture from cache...");
        std::vector<MegaTextureCache::TextureEntry> entries;
        std::vector<uint8_t> atlasData;
        MegaTextureCache::Header header;
        if (MegaTextureCache::LoadCache(mapName, gameInfo, entries, atlasData, header)) {
            Logger::Info("Successfully loaded megatexture cache");
            return true;
        } else {
            Logger::Warn("Cache load failed, rebuilding...");
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // STEP 2: Build RGBA8 atlas (works on all platforms including macOS)
    // ═══════════════════════════════════════════════════════════════════
    
    if (!m_dualAtlas.Initialize(atlasWidth, atlasHeight)) {
        Logger::Error("Failed to initialize DualTextureAtlas");
        return false;
    }

    // Initialize depth map atlas for parallax mapping
    if (!m_depthAtlas.Initialize(gameInfo, atlasWidth)) {
        Logger::Warn("Failed to initialize DepthMapAtlas, parallax mapping will be disabled");
    }

    // Pack each material texture
    m_packedTextures = 0;
    m_failedTextures = 0;

    for (const auto& materialName : materialNames) {
        if (PackMaterialTexture(materialName, materialSystem)) {
            m_packedTextures++;
        } else {
            m_failedTextures++;
        }
    }

    Logger::Info("Texture packing completed:");
    Logger::Info("  Packed: " + std::to_string(m_packedTextures));
    Logger::Info("  Failed: " + std::to_string(m_failedTextures));
    Logger::Info("  Atlas usage: " + std::to_string(GetStats().atlasUsagePercent) + "%");

    // ═══════════════════════════════════════════════════════════════════
    // STEP 3: Extract RGBA8 data and compress to DXT1 for caching
    // ═══════════════════════════════════════════════════════════════════
    
    // Extract color atlas RGBA8 data
    TextureAtlas& colorAtlas = m_dualAtlas.GetColorAtlas();
    if (colorAtlas.IsInitialized()) {
        int width = colorAtlas.GetWidth();
        int height = colorAtlas.GetHeight();
        
        Logger::Info("Extracting atlas data for caching: " + 
                     std::to_string(width) + "x" + std::to_string(height));
        
        std::vector<uint8_t> rgba8Data(width * height * 4);
        colorAtlas.ExtractRGBA8Data(rgba8Data.data());
        
        // Compress to DXT1
        size_t dxt1Size = DXTCompressor::GetDXT1Size(width, height);
        std::vector<uint8_t> compressedData(dxt1Size);
        DXTCompressor::CompressRGBA8ToDXT1(rgba8Data.data(), width, height, compressedData.data());
        
        Logger::Info("Compressed atlas: " + std::to_string(dxt1Size) + " bytes (from " + 
                     std::to_string(rgba8Data.size()) + " bytes)");
        
        // Build texture entries with UV coordinates for cache
        std::vector<MegaTextureCache::TextureEntry> entries;
        entries.reserve(m_materialAllocations.size());
        
        for (const auto& [materialName, allocID] : m_materialAllocations) {
            MegaTextureCache::TextureEntry entry;
            entry.name = materialName;
            entry.atlasIndex = 0;
            // Get UV crop and convert to offset/scale
            glm::vec4 uv = m_dualAtlas.GetColorUVCrop(allocID);
            entry.uOffset = uv.x;
            entry.vOffset = uv.y;
            entry.uScale = uv.z;
            entry.vScale = uv.w;
            entry.inputWidth = width;
            entry.inputHeight = height;
            entry.checksum = 0;
            entries.push_back(entry);
        }
        
        // Save cache using GameInfo paths
        MegaTextureCache::Header header;
        header.atlasWidth = width;
        header.atlasHeight = height;
        header.textureCount = entries.size();
        header.dataSize = compressedData.size();
        header.atlasCount = 1;
        
        if (MegaTextureCache::SaveCache(mapName, gameInfo, entries, compressedData, header)) {
            Logger::Info("Megatexture cache saved successfully");
        } else {
            Logger::Warn("Failed to save megatexture cache");
        }
    }

    return m_packedTextures > 0;
}

std::vector<std::string> BSPTexturePacker::CollectMaterialNames(const BSP& bsp) const {
    std::unordered_set<std::string> uniqueMaterials;
    
    // Collect from BSP parser faces
    const auto& faces = bsp.GetParser().GetFaces();
    const auto& texinfo = bsp.GetParser().GetTexinfo();
    
    for (const auto& face : faces) {
        if (face.texinfo >= 0 && face.texinfo < (int)texinfo.size()) {
            const auto& ti = texinfo[face.texinfo];
            std::string materialName = bsp.GetParser().GetTextureName(face.texinfo);
            if (!materialName.empty()) {
                uniqueMaterials.insert(materialName);
            }
        }
    }

    // Convert to vector
    std::vector<std::string> result;
    result.reserve(uniqueMaterials.size());
    for (const auto& name : uniqueMaterials) {
        result.push_back(name);
    }

    // Sort for consistent processing
    std::sort(result.begin(), result.end());

    return result;
}

bool BSPTexturePacker::PackMaterialTexture(const std::string& materialName, 
                                          MaterialSystem& materialSystem) {
    Logger::Info("Packing material: " + materialName);

    // Get the material from the system
    Material material = materialSystem.GetMaterial(materialName);
    
    if (!material.hasDiffuse()) {
        Logger::Warn("Material " + materialName + " has no diffuse texture");
        return false;
    }

    // Get texture data from existing GL texture
    std::vector<uint8_t> textureData;
    int width = 0, height = 0;
    GLenum format = 0;

    if (!materialSystem.GetTextureData(material.textureID, textureData, width, height, format)) {
        Logger::Error("Failed to get texture data for " + materialName);
        return false;
    }

    Logger::Info("Material " + materialName + " texture data: ID=" + std::to_string(material.textureID) + 
                 ", size=" + std::to_string(textureData.size()) + ", format=" + std::to_string(format) +
                 ", dims=" + std::to_string(width) + "x" + std::to_string(height));

    // Check if we have valid data
    if (textureData.empty()) {
        Logger::Error("Empty texture data for " + materialName);
        return false;
    }

    // Determine the actual format based on the data we received
    GLenum uploadFormat = format;
    
    // Check if this is an uncompressed format (PNG/JPG loaded via stb_image)
    bool isUncompressed = (format == GL_SRGB8_ALPHA8 || format == GL_RGBA8 ||
                          format == GL_SRGB8 || format == GL_RGB8 ||
                          format == GL_RGBA || format == GL_RGB ||
                          format == 35907); // GL_RGBA from some sources
    
    if (isUncompressed) {
        Logger::Info("Texture " + materialName + " is uncompressed (format=" + std::to_string(format) + ")");
        uploadFormat = GL_RGBA;
    }
    
    // Check if this is a compressed format
    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) {
        Logger::Info("Texture " + materialName + " is RGB DXT1 compressed");
        uploadFormat = format;
    } else if (format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
        Logger::Info("Texture " + materialName + " is RGBA DXT1 compressed");
        uploadFormat = format;
    } else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        Logger::Info("Texture " + materialName + " is DXT5 compressed");
        uploadFormat = format;
    } else if (isUncompressed) {
        Logger::Info("Texture " + materialName + " is uncompressed, converting to RGBA for atlas");
        uploadFormat = GL_RGBA;
    } else {
        Logger::Warn("Unknown texture format " + std::to_string(format) + " for " + materialName);
        return false;
    }

    // Allocate in dual atlas (handles both DXT1 and DXT5)
    int allocationID = m_dualAtlas.AllocateTexture(width, height,
                                                  textureData.data(), textureData.size(),
                                                  uploadFormat, width, height);

    if (allocationID < 0) {
        Logger::Error("Failed to allocate texture in dual atlas for " + materialName);
        return false;
    }

    // Store mapping
    m_materialAllocations[materialName] = allocationID;

    // Update the material to use atlas
    material.useAtlas = true;
    material.atlasAllocationID = allocationID;
    material.atlasUVCrop = m_dualAtlas.GetColorUVCrop(allocationID);

    // Clear the individual texture ID since we're using atlas
    material.textureID = 0;
    material.diffuseID = 0;

    // ── Generate depth map for parallax mapping ───────────────────────────────
    if (m_depthAtlas.IsReady() && material.normalID != 0) {
        std::vector<uint8_t> normalData;
        int normalW = 0, normalH = 0;
        GLenum normalFormat = 0;

        if (materialSystem.GetTextureData(material.normalID, normalData, normalW, normalH, normalFormat)) {
            std::vector<uint8_t> diffuseData;
            int diffuseW = 0, diffuseH = 0;
            GLenum diffuseFormat = 0;

            // Try to get diffuse data for low-frequency shape bias
            if (material.textureID != 0) {
                materialSystem.GetTextureData(material.textureID, diffuseData, diffuseW, diffuseH, diffuseFormat);
            }

            const uint8_t* diffPtr = diffuseData.empty() ? nullptr : diffuseData.data();
            if (m_depthAtlas.GenerateAndPack(materialName, normalData.data(), normalW, normalH,
                                              diffPtr, diffuseW, diffuseH)) {
                Logger::Info("Depth map generated for " + materialName);
            }
        }
    }

    Logger::Info("Successfully packed " + materialName + " as allocation " + 
                 std::to_string(allocationID));
    return true;
}

std::string BSPTexturePacker::GetMaterialTexturePath(const std::string& materialName) const {
    std::string texturePath = materialName;
    
    size_t pos = texturePath.find("_diffuse");
    if (pos != std::string::npos) {
        texturePath = texturePath.substr(0, pos);
    }
    
    return texturePath;
}

bool BSPTexturePacker::LoadTextureData(const std::string& path, 
                                      std::vector<uint8_t>& outData,
                                      int& outWidth, int& outHeight,
                                      GLenum& outFormat) const {
    // Try VTF first
    VTFLoader vtfLoader;
    if (vtfLoader.IsValidVTF(path)) {
        if (vtfLoader.LoadFromFile(path)) {
            if (vtfLoader.IsCompressed()) {
                const uint8_t* compressedData = nullptr;
                int width = 0, height = 0;
                uint32_t glFormat = 0;
                size_t dataSize = 0;

                compressedData = vtfLoader.GetCompressedData(&width, &height, &glFormat, &dataSize);

                if (compressedData && dataSize > 0 && glFormat != 0) {
                    outData.assign(compressedData, compressedData + dataSize);
                    outWidth = width;
                    outHeight = height;
                    outFormat = static_cast<GLenum>(glFormat);
                    return true;
                }
            } else {
                const uint8_t* rgbaData = vtfLoader.GetRGBAData(&outWidth, &outHeight);
                if (rgbaData) {
                    size_t dataSize = outWidth * outHeight * 4;
                    outData.assign(rgbaData, rgbaData + dataSize);
                    outFormat = GL_RGBA;
                    return true;
                }
            }
        }
    }

    // Fall back to standard image loading (PNG, JPG, etc.)
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (data) {
        outData.assign(data, data + (width * height * 4));
        stbi_image_free(data);
        outWidth = width;
        outHeight = height;
        outFormat = GL_RGBA;
        return true;
    }

    Logger::Error("Failed to load texture: " + path);
    return false;
}

int BSPTexturePacker::GetMaterialAllocationID(const std::string& materialName) const {
    auto it = m_materialAllocations.find(materialName);
    if (it != m_materialAllocations.end()) {
        return it->second;
    }
    return -1;
}

glm::vec4 BSPTexturePacker::GetMaterialUVCrop(const std::string& materialName) const {
    int allocationID = GetMaterialAllocationID(materialName);
    if (allocationID >= 0) {
        return m_dualAtlas.GetColorUVCrop(allocationID);
    }
    return glm::vec4(0.0f);
}

glm::vec4 BSPTexturePacker::GetDepthMapUVCrop(const std::string& materialName) const {
    return m_depthAtlas.GetUVCrop(materialName);
}

BSPTexturePacker::PackingStats BSPTexturePacker::GetStats() const {
    PackingStats stats;
    stats.totalTextures = m_totalTextures;
    stats.packedTextures = m_packedTextures;
    stats.failedTextures = m_failedTextures;
    stats.atlasUsagePercent = m_dualAtlas.GetUsagePercentage();
    stats.atlasMemoryUsed = m_dualAtlas.GetUsedMemory();
    stats.atlasMemoryTotal = m_dualAtlas.GetTotalMemory();
    return stats;
}

} // namespace veex
