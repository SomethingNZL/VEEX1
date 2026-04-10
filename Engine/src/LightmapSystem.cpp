// veex/LightmapSystem.cpp
// Enhanced lightmap handling system implementation with improved atlas packing,
// multi-resolution support, streaming, and compression.

#include "veex/LightmapSystem.h"
#include "veex/BSPParser.h"
#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace veex {

// ── LightmapAtlas Implementation ──────────────────────────────────────────────

LightmapAtlas::LightmapAtlas() = default;

LightmapAtlas::~LightmapAtlas() {
    Clear();
}

bool LightmapAtlas::Initialize(const LightmapAtlasConfig& config) {
    m_config = config;
    Clear();
    return true;
}

bool LightmapAtlas::BuildFromBSP(const BSPParser& parser) {
    const auto& faces = parser.GetFaces();
    // Note: BSPParser doesn't have GetLightingData(), we'll use a simpler approach
    // that works with the existing lightmap atlas building
    
    if (faces.empty()) {
        Logger::Warn("[LightmapAtlas] No faces available for atlas building.");
        return false;
    }
    
    // Calculate face data and total area
    std::vector<LightmapFaceData> faceDataList;
    float totalArea = 0.0f;
    
    for (int i = 0; i < (int)faces.size(); ++i) {
        const auto& face = faces[i];
        
        // Skip faces without lightmap data
        if (face.lightofs < 0 || face.styles[0] == 255) continue;
        if (face.texinfo < 0 || face.texinfo >= (int)parser.GetTexinfo().size()) continue;
        if (parser.GetTexinfo()[face.texinfo].flags & 0x0080) continue; // SURF_NODRAW
        
        int lw = face.LightmapTextureSizeInLuxels[0] + 1;
        int lh = face.LightmapTextureSizeInLuxels[1] + 1;
        
        if (lw <= 0 || lh <= 0) continue;
        
        LightmapFaceData data;
        data.faceIndex = i;
        data.luxelSize = glm::ivec2(lw, lh);
        data.area = static_cast<float>(lw * lh);
        data.hasValidLightmap = true;
        data.resolutionLevel = 0; // Start with full resolution
        
        faceDataList.push_back(data);
        totalArea += data.area;
    }
    
    if (faceDataList.empty()) {
        Logger::Warn("[LightmapAtlas] No valid faces found for lightmap atlas.");
        return false;
    }
    
    // Sort faces by area (largest first) for better packing
    std::sort(faceDataList.begin(), faceDataList.end(), 
              [](const LightmapFaceData& a, const LightmapFaceData& b) {
                  return a.area > b.area;
              });
    
    // Pack faces into atlas
    if (!PackFaces(faceDataList)) {
        Logger::Error("[LightmapAtlas] Failed to pack faces into atlas.");
        return false;
    }
    
    // Build atlas pixel data (initialized to black)
    m_atlasPixels.resize(m_atlasSize.x * m_atlasSize.y, glm::vec3(0.0f));
    
    // Store face data (actual lightmap data would be filled by BSPParser's BuildLightmapAtlas)
    for (const auto& faceData : faceDataList) {
        m_faceData[faceData.faceIndex] = faceData;
    }
    
    // Upload to GPU
    if (!UploadToGPU(m_atlasPixels)) {
        Logger::Error("[LightmapAtlas] Failed to upload atlas to GPU.");
        return false;
    }
    
    // Calculate statistics
    m_stats.totalFaces = static_cast<int>(faces.size());
    m_stats.validFaces = static_cast<int>(faceDataList.size());
    m_stats.atlasWidth = m_atlasSize.x;
    m_stats.atlasHeight = m_atlasSize.y;
    m_stats.packingEfficiency = (totalArea / (m_atlasSize.x * m_atlasSize.y)) * 100.0f;
    m_stats.memoryUsageKB = (m_atlasSize.x * m_atlasSize.y * 3) / 1024; // RGB8 format
    
    Logger::Info("[LightmapAtlas] Built atlas: " + std::to_string(m_stats.validFaces) + 
                 "/" + std::to_string(m_stats.totalFaces) + " faces, " +
                 std::to_string(m_atlasSize.x) + "x" + std::to_string(m_atlasSize.y) + 
                 ", efficiency: " + std::to_string(static_cast<int>(m_stats.packingEfficiency)) + "%");
    
    return true;
}

bool LightmapAtlas::UpdateFace(int faceIndex, const std::vector<glm::vec3>& lightmapData) {
    auto it = m_faceData.find(faceIndex);
    if (it == m_faceData.end()) {
        Logger::Warn("[LightmapAtlas] Cannot update face " + std::to_string(faceIndex) + 
                     " - not found in atlas.");
        return false;
    }
    
    const auto& faceData = it->second;
    
    if (lightmapData.size() != static_cast<size_t>(faceData.luxelSize.x * faceData.luxelSize.y)) {
        Logger::Warn("[LightmapAtlas] Lightmap data size mismatch for face " + std::to_string(faceIndex));
        return false;
    }
    
    // Update atlas pixels
    for (int y = 0; y < faceData.luxelSize.y; ++y) {
        for (int x = 0; x < faceData.luxelSize.x; ++x) {
            int atlasX = faceData.atlasOffset.x + x;
            int atlasY = faceData.atlasOffset.y + y;
            
            if (atlasX >= 0 && atlasX < m_atlasSize.x && 
                atlasY >= 0 && atlasY < m_atlasSize.y) {
                int atlasIndex = atlasY * m_atlasSize.x + atlasX;
                int dataIndex = y * faceData.luxelSize.x + x;
                m_atlasPixels[atlasIndex] = lightmapData[dataIndex];
            }
        }
    }
    
    // Update GPU texture
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    if (m_compressionFormat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT ||
        m_compressionFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ||
        m_compressionFormat == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
        m_compressionFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        // For compressed formats, we need to re-upload the entire texture
        // This is a limitation of compressed texture updates
        glTexImage2D(GL_TEXTURE_2D, 0, m_compressionFormat, 
                     m_atlasSize.x, m_atlasSize.y, 0, 
                     GL_RGB, GL_UNSIGNED_BYTE, m_atlasPixels.data());
    } else {
        // For uncompressed formats, we can use glTexSubImage2D
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        faceData.atlasOffset.x, faceData.atlasOffset.y,
                        faceData.luxelSize.x, faceData.luxelSize.y,
                        GL_RGB, GL_FLOAT, lightmapData.data());
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

const LightmapFaceData* LightmapAtlas::GetFaceData(int faceIndex) const {
    auto it = m_faceData.find(faceIndex);
    return (it != m_faceData.end()) ? &it->second : nullptr;
}

void LightmapAtlas::Clear() {
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    
    m_atlasSize = glm::ivec2(0);
    m_faceData.clear();
    m_atlasPixels.clear();
    m_stats = {};
}

bool LightmapAtlas::AllocateAtlas(int width, int height) {
    // Check if dimensions are valid
    if (width < m_config.minAtlasSize || height < m_config.minAtlasSize ||
        width > m_config.maxAtlasSize || height > m_config.maxAtlasSize) {
        Logger::Error("[LightmapAtlas] Invalid atlas dimensions: " + std::to_string(width) + 
                     "x" + std::to_string(height));
        return false;
    }
    
    m_atlasSize = glm::ivec2(width, height);
    m_atlasPixels.resize(width * height, glm::vec3(0.0f));
    
    return true;
}

bool LightmapAtlas::PackFaces(const std::vector<LightmapFaceData>& faceDataList) {
    // Simple shelf-packing algorithm
    int currentX = 0, currentY = 0, shelfHeight = 0;
    int atlasWidth = m_config.maxAtlasSize;
    int atlasHeight = m_config.maxAtlasSize;
    
    // Try different atlas sizes
    for (int size = m_config.maxAtlasSize; size >= m_config.minAtlasSize; size /= 2) {
        if (AllocateAtlas(size, size)) {
            currentX = 0; currentY = 0; shelfHeight = 0;
            
            bool allPacked = true;
            for (const auto& face : faceDataList) {
                int w = face.luxelSize.x + m_config.padding;
                int h = face.luxelSize.y + m_config.padding;
                
                if (currentX + w > atlasWidth) {
                    currentY += shelfHeight;
                    currentX = 0;
                    shelfHeight = 0;
                }
                
                if (currentY + h > atlasHeight) {
                    allPacked = false;
                    break;
                }
                
                // Assign position
                LightmapFaceData data = face;
                data.atlasOffset = glm::ivec2(currentX, currentY);
                data.atlasScale = glm::vec2(static_cast<float>(face.luxelSize.x) / size,
                                           static_cast<float>(face.luxelSize.y) / size);
                
                m_faceData[face.faceIndex] = data;
                
                currentX += w;
                shelfHeight = std::max(shelfHeight, h);
            }
            
            if (allPacked) {
                // Pad remaining space and return success
                m_atlasSize = glm::ivec2(size, size);
                return true;
            }
        }
    }
    
    return false;
}

bool LightmapAtlas::UploadToGPU(const std::vector<glm::vec3>& atlasData) {
    // Clean up existing texture
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
    
    // Check for DXT compression support
    bool useCompression = m_config.enableCompression && IsDXTSupported();
    
    if (useCompression) {
        // Try to use DXT compression
        if (IsDXTSupported()) {
            m_compressionFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        } else {
            Logger::Warn("[LightmapAtlas] DXT compression not supported, falling back to uncompressed.");
            useCompression = false;
        }
    }
    
    // Generate texture
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    if (useCompression) {
        // Convert HDR to LDR for compression
        std::vector<uint8_t> ldrData(atlasData.size() * 3);
        for (size_t i = 0; i < atlasData.size(); ++i) {
            // Simple tone mapping and gamma correction
            glm::vec3 hdr = atlasData[i];
            glm::vec3 ldr = hdr / (1.0f + hdr); // Tone mapping
            ldr = glm::pow(ldr, glm::vec3(1.0f / 2.2f)); // Gamma correction
            ldr = glm::clamp(ldr * 255.0f, 0.0f, 255.0f);
            
            ldrData[i*3+0] = static_cast<uint8_t>(ldr.x);
            ldrData[i*3+1] = static_cast<uint8_t>(ldr.y);
            ldrData[i*3+2] = static_cast<uint8_t>(ldr.z);
        }
        
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, m_compressionFormat,
                               m_atlasSize.x, m_atlasSize.y, 0,
                               static_cast<GLsizei>(ldrData.size()), ldrData.data());
        
        m_stats.memoryUsageKB = static_cast<int>(ldrData.size() / 1024);
    } else {
        // Use uncompressed RGB8
        m_compressionFormat = GL_RGB8;
        
        // Convert HDR to LDR
        std::vector<uint8_t> ldrData(atlasData.size() * 3);
        for (size_t i = 0; i < atlasData.size(); ++i) {
            glm::vec3 hdr = atlasData[i];
            glm::vec3 ldr = hdr / (1.0f + hdr);
            ldr = glm::pow(ldr, glm::vec3(1.0f / 2.2f));
            ldr = glm::clamp(ldr * 255.0f, 0.0f, 255.0f);
            
            ldrData[i*3+0] = static_cast<uint8_t>(ldr.x);
            ldrData[i*3+1] = static_cast<uint8_t>(ldr.y);
            ldrData[i*3+2] = static_cast<uint8_t>(ldr.z);
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
                     m_atlasSize.x, m_atlasSize.y, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, ldrData.data());
        
        m_stats.memoryUsageKB = static_cast<int>(ldrData.size() / 1024);
        
        // Generate mipmaps if enabled
        if (m_config.enableMipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

glm::vec3 LightmapAtlas::DecodeLightmapSample(const veex::ColorRGBExp32& sample) const {
    // Source RGBE decode: channel = byte * 2^exponent / 255
    const float scale = std::pow(2.0f, static_cast<float>(sample.exponent)) / 255.0f;
    return glm::clamp(glm::vec3(sample.r, sample.g, sample.b) * scale, 0.0f, 65504.0f);
}

// ── LightmapStreaming Implementation ──────────────────────────────────────────

LightmapStreaming::LightmapStreaming() = default;

LightmapStreaming::~LightmapStreaming() = default;

void LightmapStreaming::Initialize(const LightmapAtlasConfig& config) {
    m_config = config;
    m_loadedFaces.clear();
    m_faceInfo.clear();
    m_facePriorityList.clear();
    m_lastUpdateTime = 0.0f;
    m_stats = {};
}

void LightmapStreaming::UpdateStreaming(const BSP& map, const glm::vec3& cameraPos, 
                                       const std::vector<int>& visibleFaces) {
    const float currentTime = static_cast<float>(glfwGetTime());
    
    // Throttle updates
    if (currentTime - m_lastUpdateTime < 0.1f) {
        return;
    }
    m_lastUpdateTime = currentTime;
    
    // Update face priorities
    UpdateFacePriorities(cameraPos, visibleFaces);
    
    // Sort faces by priority
    SortFacesByPriority();
    
    // Manage memory budget
    ManageMemoryBudget();
    
    // Update statistics
    m_stats.totalFaces = static_cast<int>(map.GetParser().GetFaces().size());
    m_stats.loadedFaces = static_cast<int>(m_loadedFaces.size());
    m_stats.loadEfficiency = 0.0f; // Calculate based on visible faces
    
    int visibleLoaded = 0;
    for (int faceIndex : visibleFaces) {
        if (m_loadedFaces.count(faceIndex) > 0) {
            visibleLoaded++;
        }
    }
    
    if (!visibleFaces.empty()) {
        m_stats.loadEfficiency = static_cast<float>(visibleLoaded) / visibleFaces.size() * 100.0f;
    }
}

float LightmapStreaming::GetFacePriority(int faceIndex, const glm::vec3& cameraPos) const {
    auto it = m_faceInfo.find(faceIndex);
    if (it == m_faceInfo.end()) {
        return 0.0f;
    }
    
    const auto& info = it->second;
    float priority = info.priority;
    
    // Boost priority for recently visible faces
    float timeSinceUpdate = static_cast<float>(glfwGetTime()) - info.lastUpdateTime;
    if (timeSinceUpdate < 1.0f) {
        priority *= 2.0f;
    }
    
    return priority;
}

bool LightmapStreaming::ShouldLoadFace(int faceIndex) const {
    auto it = m_faceInfo.find(faceIndex);
    if (it == m_faceInfo.end()) {
        return false;
    }
    
    const auto& info = it->second;
    return info.priority > 0.1f && !info.isLoaded;
}

void LightmapStreaming::LoadFace(int faceIndex, const BSPParser& parser) {
    auto it = m_faceInfo.find(faceIndex);
    if (it == m_faceInfo.end()) {
        return;
    }
    
    it->second.isLoaded = true;
    m_loadedFaces.insert(faceIndex);
    
    // Update memory usage statistics
    // This would be calculated based on the actual lightmap size
    m_stats.memoryUsageKB += 100; // Placeholder
}

void LightmapStreaming::UnloadFace(int faceIndex) {
    auto it = m_faceInfo.find(faceIndex);
    if (it == m_faceInfo.end()) {
        return;
    }
    
    it->second.isLoaded = false;
    m_loadedFaces.erase(faceIndex);
    
    // Update memory usage statistics
    m_stats.memoryUsageKB -= 100; // Placeholder
}

void LightmapStreaming::UpdateFacePriorities(const glm::vec3& cameraPos, 
                                           const std::vector<int>& visibleFaces) {
    const auto& faces = m_faceInfo; // This would be populated elsewhere
    
    for (auto& [faceIndex, info] : m_faceInfo) {
        // Calculate distance to camera
        // This would use actual face center calculation
        info.distanceToCamera = glm::distance(cameraPos, glm::vec3(0.0f));
        
        // Base priority based on distance (closer = higher priority)
        float distancePriority = 1.0f / (1.0f + info.distanceToCamera * 0.01f);
        
        // Boost priority for visible faces
        bool isVisible = std::find(visibleFaces.begin(), visibleFaces.end(), faceIndex) != visibleFaces.end();
        if (isVisible) {
            distancePriority *= 5.0f;
        }
        
        // Boost priority for recently updated faces
        float timeSinceUpdate = static_cast<float>(glfwGetTime()) - info.lastUpdateTime;
        float timePriority = std::max(0.0f, 1.0f - timeSinceUpdate * 0.1f);
        
        info.priority = distancePriority * timePriority;
        info.lastUpdateTime = static_cast<float>(glfwGetTime());
    }
}

void LightmapStreaming::ManageMemoryBudget() {
    // Sort faces by priority (already done in SortFacesByPriority)
    
    // Unload low-priority faces if over budget
    int currentMemory = m_stats.memoryUsageKB;
    int targetMemory = m_memoryBudgetKB * 0.8f; // Use 80% of budget
    
    if (currentMemory > targetMemory) {
        // Unload faces starting from lowest priority
        for (auto* info : m_facePriorityList) {
            if (currentMemory <= targetMemory) break;
            
            if (info->isLoaded && info->priority < 0.5f) {
                UnloadFace(info->faceIndex);
                currentMemory -= 100; // Placeholder
            }
        }
    }
    
    // Load high-priority faces if under budget
    int maxMemory = m_memoryBudgetKB;
    if (currentMemory < maxMemory) {
        for (auto* info : m_facePriorityList) {
            if (currentMemory >= maxMemory) break;
            
            if (!info->isLoaded && info->priority > 0.8f) {
                // This would need access to the BSPParser to actually load
                // For now, just mark as loaded
                info->isLoaded = true;
                m_loadedFaces.insert(info->faceIndex);
                currentMemory += 100; // Placeholder
            }
        }
    }
}

void LightmapStreaming::SortFacesByPriority() {
    m_facePriorityList.clear();
    m_facePriorityList.reserve(m_faceInfo.size());
    
    for (auto& [faceIndex, info] : m_faceInfo) {
        m_facePriorityList.push_back(&info);
    }
    
    std::sort(m_facePriorityList.begin(), m_facePriorityList.end(),
              [](const FaceStreamingInfo* a, const FaceStreamingInfo* b) {
                  return a->priority > b->priority;
              });
}

// ── LightmapSystem Implementation ─────────────────────────────────────────────

LightmapSystem::LightmapSystem() = default;

LightmapSystem::~LightmapSystem() = default;

bool LightmapSystem::Initialize(const LightmapAtlasConfig& config) {
    m_config = config;
    
    m_atlas = std::make_unique<LightmapAtlas>();
    if (!m_atlas->Initialize(config)) {
        Logger::Error("[LightmapSystem] Failed to initialize lightmap atlas.");
        return false;
    }
    
    m_streaming = std::make_unique<LightmapStreaming>();
    m_streaming->Initialize(config);
    
    Logger::Info("[LightmapSystem] Initialized with enhanced lightmap handling.");
    return true;
}

bool LightmapSystem::BuildAtlas(const BSPParser& parser) {
    if (!m_atlas) {
        Logger::Error("[LightmapSystem] Atlas not initialized.");
        return false;
    }
    
    return m_atlas->BuildFromBSP(parser);
}

bool LightmapSystem::UpdateFaceLightmap(int faceIndex, const std::vector<glm::vec3>& lightmapData) {
    if (!m_atlas) {
        Logger::Error("[LightmapSystem] Atlas not initialized.");
        return false;
    }
    
    return m_atlas->UpdateFace(faceIndex, lightmapData);
}

void LightmapSystem::UpdateStreaming(const BSP& map, const glm::vec3& cameraPos, 
                                   const std::vector<int>& visibleFaces) {
    if (m_streaming) {
        m_streaming->UpdateStreaming(map, cameraPos, visibleFaces);
    }
}

const LightmapFaceData* LightmapSystem::GetFaceData(int faceIndex) const {
    if (!m_atlas) return nullptr;
    return m_atlas->GetFaceData(faceIndex);
}

LightmapSystem::SystemStats LightmapSystem::GetStats() const {
    SystemStats stats;
    
    if (m_atlas) {
        stats.atlasStats = m_atlas->GetStats();
    }
    
    if (m_streaming) {
        stats.streamingStats = m_streaming->GetStats();
    }
    
    stats.totalMemoryUsageMB = (stats.atlasStats.memoryUsageKB + 
                               stats.streamingStats.memoryUsageKB) / 1024.0f;
    
    stats.averagePackingEfficiency = stats.atlasStats.packingEfficiency;
    
    return stats;
}

} // namespace veex