#pragma once
// veex/TileRenderer.h
// Tile-based rendering system for efficient face culling and lightmap processing.
//
// This system divides the screen into tiles and assigns faces to tiles based on
// their screen-space bounds. This enables:
//   - Efficient frustum culling per tile
//   - Tile-based light culling for lightmap calculations
//   - Reduced overdraw through better draw call ordering
//   - Compute shader dispatch for parallel tile processing

#include "veex/Common.h"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace veex {

class Camera;
class BSP;

// ── Tile Configuration ────────────────────────────────────────────────────────
struct TileConfig {
    uint32_t tileSizeX = 32;        // Tile width in pixels
    uint32_t tileSizeY = 32;        // Tile height in pixels
    uint32_t maxTilesX = 64;        // Maximum tiles in X direction
    uint32_t maxTilesY = 36;        // Maximum tiles in Y direction (for 1080p with 32px tiles)
    bool     enableTileCulling = true;
    bool     enableLightCulling = true;
};

// ── Screen Tile ───────────────────────────────────────────────────────────────
// Represents a single tile in screen space with its associated faces
struct ScreenTile {
    uint32_t tileX = 0;
    uint32_t tileY = 0;
    
    // Screen-space bounds of this tile
    float minX = 0.0f, minY = 0.0f;
    float maxX = 1.0f, maxY = 1.0f;
    
    // Frustum planes for this tile (in world space)
    glm::vec4 frustumPlanes[6];  // Left, Right, Bottom, Top, Near, Far
    
    // Face indices that overlap this tile
    std::vector<uint32_t> faceIndices;
    
    // Bounding sphere for quick rejection
    glm::vec3 center = glm::vec3(0.0f);
    float radius = 0.0f;
    
    // Visibility flags
    bool isVisible = false;
    bool needsUpdate = true;
};

// ── Tile Bounds ───────────────────────────────────────────────────────────────
// Screen-space AABB for a face, used for tile assignment
struct TileBounds {
    float minX = 1.0f, minY = 1.0f;
    float maxX = 0.0f, maxY = 0.0f;
    
    bool isValid() const { return minX <= maxX && minY <= maxY; }
    
    void expand(float x, float y) {
        minX = fmin(minX, x);
        minY = fmin(minY, y);
        maxX = fmax(maxX, x);
        maxY = fmax(maxY, y);
    }
    
    void expand(const glm::vec3& ndcPos) {
        // Convert from NDC [-1,1] to [0,1]
        float x = (ndcPos.x + 1.0f) * 0.5f;
        float y = (ndcPos.y + 1.0f) * 0.5f;
        expand(x, y);
    }
};

// ── Tile Renderer ─────────────────────────────────────────────────────────────
// Manages tile-based rendering for efficient face processing
class TileRenderer {
public:
    TileRenderer();
    ~TileRenderer();
    
    // Prevent copying
    TileRenderer(const TileRenderer&) = delete;
    TileRenderer& operator=(const TileRenderer&) = delete;
    
    // ── Configuration ─────────────────────────────────────────────────────────
    void SetConfig(const TileConfig& config);
    const TileConfig& GetConfig() const { return m_config; }
    
    // ── Tile Management ───────────────────────────────────────────────────────
    // Initialize tiles for a given viewport size
    void Initialize(uint32_t viewportWidth, uint32_t viewportHeight);
    
    // Update tiles based on camera view
    void UpdateTiles(const Camera& camera, const BSP& map);
    
    // Get tiles for current frame
    const std::vector<ScreenTile>& GetTiles() const { return m_tiles; }
    
    // Get visible tiles only
    std::vector<const ScreenTile*> GetVisibleTiles() const;
    
    // ── Face Assignment ───────────────────────────────────────────────────────
    // Assign faces to tiles based on their screen-space bounds
    void AssignFacesToTiles(const BSP& map, const Camera& camera);
    
    // Get faces visible in a specific tile
    const std::vector<uint32_t>& GetTileFaces(uint32_t tileX, uint32_t tileY) const;
    
    // ── Culling ───────────────────────────────────────────────────────────────
    // Perform frustum culling per tile
    void PerformTileCulling(const Camera& camera);
    
    // Check if a point is visible in any tile
    bool IsPointVisible(const glm::vec3& worldPos) const;
    
    // Check if a sphere is visible in any tile
    bool IsSphereVisible(const glm::vec3& center, float radius) const;
    
    // ── Statistics ────────────────────────────────────────────────────────────
    struct TileStats {
        uint32_t totalTiles = 0;
        uint32_t visibleTiles = 0;
        uint32_t totalFaces = 0;
        uint32_t culledFaces = 0;
        float avgFacesPerTile = 0.0f;
        float cullEfficiency = 0.0f;  // Percentage of faces culled
    };
    
    TileStats GetStats() const { return m_stats; }
    
    // ── Debug Visualization ───────────────────────────────────────────────────
    // Draw tile boundaries for debugging
    void DrawDebugTiles() const;
    
    // Draw face-to-tile assignments
    void DrawDebugFaceAssignments(const BSP& map) const;

private:
    // ── Internal Helpers ──────────────────────────────────────────────────────
    void ComputeTileFrustum(uint32_t tileX, uint32_t tileY, const Camera& camera);
    void BuildTileHierarchy();
    void UpdateTileBounds(const Camera& camera);
    
    // ── Members ───────────────────────────────────────────────────────────────
    TileConfig m_config;
    std::vector<ScreenTile> m_tiles;
    
    // Tile dimensions
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    uint32_t m_tilesX = 0;
    uint32_t m_tilesY = 0;
    
    // Face to tile mapping (for quick lookup)
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_faceToTiles;
    
    // Statistics
    TileStats m_stats;
    
    // Cached camera for frame coherence
    glm::mat4 m_lastViewProj;
    bool m_cameraChanged = true;
};

} // namespace veex