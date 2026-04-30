#include "veex/DualTextureAtlas.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include <algorithm>
#include <cstring>

namespace veex {

DualTextureAtlas::DualTextureAtlas() {}

DualTextureAtlas::~DualTextureAtlas() {
    Shutdown();
}

bool DualTextureAtlas::Initialize(int width, int height) {
    if (m_colorAtlas.IsInitialized()) {
        Logger::Warn("DualTextureAtlas::Initialize called on already initialized atlas");
        return true;
    }

    Logger::Info("Initializing DualTextureAtlas: " + std::to_string(width) + "x" + 
                 std::to_string(height));

    // Check if DXT is supported - use compressed only if supported
    bool dxtSupported = TextureAtlas::IsDXTSupported();
    
    if (dxtSupported) {
        // Initialize both atlases with DXT formats
        // Color atlas uses DXT1 for RGB data (no alpha)
        // Alpha atlas uses DXT5 for alpha channel data
        
        m_colorAtlas.SetFormat(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
        bool colorSuccess = m_colorAtlas.Initialize(width, height);
        
        m_alphaAtlas.SetFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
        bool alphaSuccess = m_alphaAtlas.Initialize(width, height);

        if (!colorSuccess || !alphaSuccess) {
            Logger::Error("Failed to initialize DualTextureAtlas with DXT");
            Shutdown();
            return false;
        }
        
        Logger::Info("DualTextureAtlas using DXT compression");
    } else {
        // On macOS or platforms without DXT support, use RGBA8
        // This uses more VRAM but works on all platforms
        m_colorAtlas.SetFormat(GL_RGBA8);
        bool colorSuccess = m_colorAtlas.Initialize(width, height);
        
        // For RGBA8, we don't need a separate alpha atlas
        m_alphaAtlas.SetFormat(GL_RGBA8);
        bool alphaSuccess = m_alphaAtlas.Initialize(width, height);

        if (!colorSuccess || !alphaSuccess) {
            Logger::Error("Failed to initialize DualTextureAtlas with RGBA8");
            Shutdown();
            return false;
        }
        
        Logger::Info("DualTextureAtlas using RGBA8 (no compression)");
    }

    m_nextID = 1;
    m_allocationIndex.clear();

    Logger::Info("DualTextureAtlas initialized successfully");
    return true;
}

void DualTextureAtlas::Shutdown() {
    m_colorAtlas.Shutdown();
    m_alphaAtlas.Shutdown();
    m_allocationIndex.clear();
    Logger::Info("DualTextureAtlas shutdown complete");
}

int DualTextureAtlas::AllocateTexture(int width, int height,
                                     const uint8_t* data, size_t dataSize,
                                     GLenum format,
                                     int inputWidth, int inputHeight) {
    if (!m_colorAtlas.IsInitialized()) {
        Logger::Error("DualTextureAtlas::AllocateTexture called on uninitialized atlas");
        return -1;
    }

    if (!data || dataSize == 0) {
        Logger::Error("DualTextureAtlas::AllocateTexture called with null or empty data");
        return -1;
    }

    // Use input dimensions if provided, otherwise use the passed width/height
    int actualInputWidth = (inputWidth > 0) ? inputWidth : width;
    int actualInputHeight = (inputHeight > 0) ? inputHeight : height;

    // Generate allocation ID
    int allocationID = m_nextID++;

    int colorAllocID = -1;
    int alphaAllocID = -1;

    // Route texture based on format
    if (format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || 
        format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
        // DXT1 textures go to color atlas
        colorAllocID = m_colorAtlas.AllocateTexture(width, height,
                                                   data, dataSize,
                                                   format,
                                                   actualInputWidth, actualInputHeight);
        Logger::Info("DualTextureAtlas: Allocated DXT1 texture " + std::to_string(allocationID) + 
                     " in color atlas only");
    } else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        // DXT5 textures go to alpha atlas
        alphaAllocID = m_alphaAtlas.AllocateTexture(width, height,
                                                   data, dataSize,
                                                   format,
                                                   actualInputWidth, actualInputHeight);
        Logger::Info("DualTextureAtlas: Allocated DXT5 texture " + std::to_string(allocationID) + 
                     " in alpha atlas only");
    } else {
        // Fallback: try both atlases for other formats
        colorAllocID = m_colorAtlas.AllocateTexture(width, height,
                                                   data, dataSize,
                                                   format,
                                                   actualInputWidth, actualInputHeight);
        alphaAllocID = m_alphaAtlas.AllocateTexture(width, height,
                                                   data, dataSize,
                                                   format,
                                                   actualInputWidth, actualInputHeight);
        Logger::Info("DualTextureAtlas: Allocated texture " + std::to_string(allocationID) + 
                     " in both color and alpha atlases (fallback)");
    }

    if (colorAllocID < 0 && alphaAllocID < 0) {
        Logger::Error("DualTextureAtlas: Failed to allocate texture in any atlas");
        return -1;
    }

    // Store mapping - use color allocation ID if available, otherwise alpha
    m_allocationIndex[allocationID] = (colorAllocID >= 0) ? colorAllocID : alphaAllocID;

    return allocationID;
}

void DualTextureAtlas::Free(int allocationID) {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        Logger::Warn("DualTextureAtlas::Free called with invalid allocation ID: " + 
                    std::to_string(allocationID));
        return;
    }

    // Free from color atlas
    m_colorAtlas.Free(it->second);
    m_allocationIndex.erase(it);

    Logger::Info("DualTextureAtlas: Freed allocation " + std::to_string(allocationID));
}

glm::vec4 DualTextureAtlas::GetColorUVCrop(int allocationID) const {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        Logger::Error("DualTextureAtlas::GetColorUVCrop called with invalid allocation ID: " + 
                     std::to_string(allocationID));
        return glm::vec4(0.0f);
    }

    return m_colorAtlas.GetUVCrop(it->second);
}

glm::vec4 DualTextureAtlas::GetAlphaUVCrop(int allocationID) const {
    // For now, return the same UV crop as color
    // A proper implementation would return alpha-specific UV coordinates
    return GetColorUVCrop(allocationID);
}

bool DualTextureAtlas::GetAllocationSize(int allocationID, int& width, int& height) const {
    auto it = m_allocationIndex.find(allocationID);
    if (it == m_allocationIndex.end()) {
        return false;
    }

    return m_colorAtlas.GetAllocationSize(it->second, width, height);
}

size_t DualTextureAtlas::GetUsedMemory() const {
    return m_colorAtlas.GetUsedMemory() + m_alphaAtlas.GetUsedMemory();
}

size_t DualTextureAtlas::GetTotalMemory() const {
    return m_colorAtlas.GetTotalMemory() + m_alphaAtlas.GetTotalMemory();
}

float DualTextureAtlas::GetUsagePercentage() const {
    size_t total = GetTotalMemory();
    if (total == 0) return 0.0f;
    return (static_cast<float>(GetUsedMemory()) / static_cast<float>(total)) * 100.0f;
}

} // namespace veex