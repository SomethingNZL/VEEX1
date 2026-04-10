#pragma once
// veex/LightmapSystem.h
// Enhanced lightmap handling system with improved atlas packing, 
// multi-resolution support, streaming, and compression.

#include "veex/Common.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace veex {

class BSPParser;
class BSP;

// ── Lightmap Atlas Configuration ──────────────────────────────────────────────
struct LightmapAtlasConfig {
    int maxAtlasSize = 4096;          // Maximum atlas dimension
    int minAtlasSize = 512;           // Minimum atlas dimension
    int padding = 1;                  // Luxel padding between faces
    bool enableCompression = true;    // Use DXT compression
    bool enableMipmaps = true;        // Generate mipmaps for distant faces
    bool enableStreaming = true;      // Load/unload lightmaps based on visibility
};

// ── Lightmap Face Data ────────────────────────────────────────────────────────
struct LightmapFaceData {
    int faceIndex = -1;
    glm::ivec2 luxelSize;             // Size in luxels
    glm::ivec2 atlasOffset;           // Offset in atlas
    glm::vec2 atlasScale;             // Scale factor for UV mapping
    bool hasValidLightmap = false;
    float area = 0.0f;                // Face area for LOD decisions
    int resolutionLevel = 0;          // 0 = full res, 1 = half res, etc.
    bool needsUpdate = false;
};

// ── Lightmap Atlas ────────────────────────────────────────────────────────────
class LightmapAtlas {
public:
    LightmapAtlas();
    ~LightmapAtlas();

    // Initialize atlas with configuration
    bool Initialize(const LightmapAtlasConfig& config);
    
    // Build atlas from BSP lightmap data
    bool BuildFromBSP(const BSPParser& parser);
    
    // Update atlas with new lightmap data
    bool UpdateFace(int faceIndex, const std::vector<glm::vec3>& lightmapData);
    
    // Get atlas texture ID
    uint32_t GetTextureID() const { return m_textureID; }
    
    // Get face lightmap information
    const LightmapFaceData* GetFaceData(int faceIndex) const;
    
    // Get atlas dimensions
    glm::ivec2 GetAtlasSize() const { return m_atlasSize; }
    
    // Get compression format
    int GetCompressionFormat() const { return m_compressionFormat; }
    
    // Statistics
    struct AtlasStats {
        int totalFaces = 0;
        int validFaces = 0;
        int atlasWidth = 0;
        int atlasHeight = 0;
        float packingEfficiency = 0.0f;
        int memoryUsageKB = 0;
    };
    
    AtlasStats GetStats() const { return m_stats; }

private:
    // ── Internal Helpers ──────────────────────────────────────────────────────
    void Clear();
    bool AllocateAtlas(int width, int height);
    bool PackFaces(const BSPParser& parser);
    bool UploadToGPU(const std::vector<glm::vec3>& atlasData);
    glm::vec3 DecodeLightmapSample(const ColorRGBExp32& sample) const;
    
    // ── Members ───────────────────────────────────────────────────────────────
    LightmapAtlasConfig m_config;
    uint32_t m_textureID = 0;
    glm::ivec2 m_atlasSize = glm::ivec2(0);
    
    // Face data mapping
    std::unordered_map<int, LightmapFaceData> m_faceData;
    
    // Atlas pixel data (HDR)
    std::vector<glm::vec3> m_atlasPixels;
    
    // Compression settings
    int m_compressionFormat = GL_RGB8;  // Default to uncompressed RGB8
    bool m_useMipmaps = false;
    
    // Statistics
    AtlasStats m_stats;
};

// ── Lightmap Streaming System ─────────────────────────────────────────────────
class LightmapStreaming {
public:
    LightmapStreaming();
    ~LightmapStreaming();

    // Initialize streaming system
    void Initialize(const LightmapAtlasConfig& config);
    
    // Update streaming based on camera position and visible faces
    void UpdateStreaming(const BSP& map, const glm::vec3& cameraPos, 
                        const std::vector<int>& visibleFaces);
    
    // Get priority for a face based on distance and importance
    float GetFacePriority(int faceIndex, const glm::vec3& cameraPos) const;
    
    // Check if face lightmap should be loaded
    bool ShouldLoadFace(int faceIndex) const;
    
    // Load/unload face lightmap
    void LoadFace(int faceIndex, const BSPParser& parser);
    void UnloadFace(int faceIndex);
    
    // Get loaded faces
    const std::unordered_set<int>& GetLoadedFaces() const { return m_loadedFaces; }
    
    // Statistics
    struct StreamingStats {
        int totalFaces = 0;
        int loadedFaces = 0;
        int memoryUsageKB = 0;
        float loadEfficiency = 0.0f;  // Percentage of needed faces that are loaded
    };
    
    StreamingStats GetStats() const { return m_stats; }

private:
    // ── Internal Helpers ──────────────────────────────────────────────────────
    void UpdateFacePriorities(const glm::vec3& cameraPos, 
                             const std::vector<int>& visibleFaces);
    void ManageMemoryBudget();
    void SortFacesByPriority();
    
    // ── Members ───────────────────────────────────────────────────────────────
    LightmapAtlasConfig m_config;
    
    // Face priority and loading state
    struct FaceStreamingInfo {
        int faceIndex = -1;
        float priority = 0.0f;
        bool isLoaded = false;
        float lastUpdateTime = 0.0f;
        float distanceToCamera = 0.0f;
    };
    
    std::unordered_map<int, FaceStreamingInfo> m_faceInfo;
    std::vector<FaceStreamingInfo*> m_facePriorityList;
    
    // Loaded faces set
    std::unordered_set<int> m_loadedFaces;
    
    // Memory budget (in KB)
    int m_memoryBudgetKB = 16384;  // Default 16MB
    
    // Statistics
    StreamingStats m_stats;
    
    // Last update time
    float m_lastUpdateTime = 0.0f;
};

// ── Enhanced Lightmap System ──────────────────────────────────────────────────
class LightmapSystem {
public:
    LightmapSystem();
    ~LightmapSystem();

    // Initialize system
    bool Initialize(const LightmapAtlasConfig& config);
    
    // Build lightmap atlas from BSP
    bool BuildAtlas(const BSPParser& parser);
    
    // Update lightmap for a specific face
    bool UpdateFaceLightmap(int faceIndex, const std::vector<glm::vec3>& lightmapData);
    
    // Get lightmap atlas
    LightmapAtlas* GetAtlas() { return m_atlas.get(); }
    const LightmapAtlas* GetAtlas() const { return m_atlas.get(); }
    
    // Get streaming system
    LightmapStreaming* GetStreaming() { return m_streaming.get(); }
    const LightmapStreaming* GetStreaming() const { return m_streaming.get(); }
    
    // Update streaming based on camera and visibility
    void UpdateStreaming(const BSP& map, const glm::vec3& cameraPos, 
                        const std::vector<int>& visibleFaces);
    
    // Get face lightmap data
    const LightmapFaceData* GetFaceData(int faceIndex) const;
    
    // Statistics
    struct SystemStats {
        LightmapAtlas::AtlasStats atlasStats;
        LightmapStreaming::StreamingStats streamingStats;
        float totalMemoryUsageMB = 0.0f;
        float averagePackingEfficiency = 0.0f;
    };
    
    SystemStats GetStats() const;

private:
    // ── Members ───────────────────────────────────────────────────────────────
    std::unique_ptr<LightmapAtlas> m_atlas;
    std::unique_ptr<LightmapStreaming> m_streaming;
    LightmapAtlasConfig m_config;
    
    // Last update time for throttling
    float m_lastStreamingUpdate = 0.0f;
    static constexpr float STREAMING_UPDATE_INTERVAL = 0.1f;  // Update every 100ms
};

} // namespace veex