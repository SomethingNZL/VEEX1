// veex/TileRenderer.cpp
// Tile-based rendering system implementation for efficient face culling and lightmap processing.

#include "veex/TileRenderer.h"
#include "veex/Camera.h"
#include "veex/BSP.h"
#include "veex/Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cmath>

namespace veex {

// ── TileRenderer Implementation ───────────────────────────────────────────────

TileRenderer::TileRenderer() {
    // Default configuration
    m_config.tileSizeX = 32;
    m_config.tileSizeY = 32;
    m_config.maxTilesX = 64;
    m_config.maxTilesY = 36;
    m_config.enableTileCulling = true;
    m_config.enableLightCulling = true;
}

TileRenderer::~TileRenderer() = default;

void TileRenderer::SetConfig(const TileConfig& config) {
    m_config = config;
    // Reinitialize if already initialized
    if (m_viewportWidth > 0 && m_viewportHeight > 0) {
        Initialize(m_viewportWidth, m_viewportHeight);
    }
}

void TileRenderer::Initialize(uint32_t viewportWidth, uint32_t viewportHeight) {
    m_viewportWidth = viewportWidth;
    m_viewportHeight = viewportHeight;
    
    // Calculate number of tiles needed
    m_tilesX = (viewportWidth + m_config.tileSizeX - 1) / m_config.tileSizeX;
    m_tilesY = (viewportHeight + m_config.tileSizeY - 1) / m_config.tileSizeY;
    
    // Clamp to maximum tiles
    m_tilesX = std::min(m_tilesX, m_config.maxTilesX);
    m_tilesY = std::min(m_tilesY, m_config.maxTilesY);
    
    // Resize tiles array
    m_tiles.resize(m_tilesX * m_tilesY);
    
    // Initialize each tile
    for (uint32_t y = 0; y < m_tilesY; ++y) {
        for (uint32_t x = 0; x < m_tilesX; ++x) {
            ScreenTile& tile = m_tiles[y * m_tilesX + x];
            tile.tileX = x;
            tile.tileY = y;
            
            // Calculate screen-space bounds [0,1]
            tile.minX = static_cast<float>(x * m_config.tileSizeX) / viewportWidth;
            tile.minY = static_cast<float>(y * m_config.tileSizeY) / viewportHeight;
            tile.maxX = static_cast<float>((x + 1) * m_config.tileSizeX) / viewportWidth;
            tile.maxY = static_cast<float>((y + 1) * m_config.tileSizeY) / viewportHeight;
            
            // Clamp to viewport bounds
            tile.maxX = std::min(tile.maxX, 1.0f);
            tile.maxY = std::min(tile.maxY, 1.0f);
            
            // Clear face indices
            tile.faceIndices.clear();
            tile.isVisible = false;
            tile.needsUpdate = true;
        }
    }
    
    Logger::Info("[TileRenderer] Initialized " + std::to_string(m_tilesX) + "x" + 
                 std::to_string(m_tilesY) + " tiles for " + std::to_string(viewportWidth) + 
                 "x" + std::to_string(viewportHeight) + " viewport");
}

void TileRenderer::UpdateTiles(const Camera& camera, const BSP& map) {
    // Check if camera changed
    glm::mat4 currentViewProj = camera.GetProjectionMatrix(static_cast<float>(m_viewportWidth) / m_viewportHeight) * camera.GetViewMatrix();
    bool cameraChanged = (currentViewProj != m_lastViewProj);
    
    if (cameraChanged) {
        m_lastViewProj = currentViewProj;
        m_cameraChanged = true;
    }
    
    // Update tile frustums if camera changed
    if (m_cameraChanged && m_config.enableTileCulling) {
        for (uint32_t y = 0; y < m_tilesY; ++y) {
            for (uint32_t x = 0; x < m_tilesX; ++x) {
                ComputeTileFrustum(x, y, camera);
            }
        }
        m_cameraChanged = false;
    }
    
    // Assign faces to tiles
    AssignFacesToTiles(map, camera);
    
    // Perform culling
    if (m_config.enableTileCulling) {
        PerformTileCulling(camera);
    }
    
    // Update statistics
    m_stats.totalTiles = m_tilesX * m_tilesY;
    m_stats.visibleTiles = 0;
    m_stats.totalFaces = 0;
    m_stats.culledFaces = 0;
    
    for (const auto& tile : m_tiles) {
        if (tile.isVisible) {
            m_stats.visibleTiles++;
            m_stats.totalFaces += static_cast<uint32_t>(tile.faceIndices.size());
        } else {
            m_stats.culledFaces += static_cast<uint32_t>(tile.faceIndices.size());
        }
    }
    
    if (m_stats.totalFaces > 0) {
        m_stats.avgFacesPerTile = static_cast<float>(m_stats.totalFaces) / m_stats.visibleTiles;
        m_stats.cullEfficiency = static_cast<float>(m_stats.culledFaces) / (m_stats.totalFaces + m_stats.culledFaces) * 100.0f;
    }
}

std::vector<const ScreenTile*> TileRenderer::GetVisibleTiles() const {
    std::vector<const ScreenTile*> visibleTiles;
    visibleTiles.reserve(m_stats.visibleTiles);
    
    for (const auto& tile : m_tiles) {
        if (tile.isVisible) {
            visibleTiles.push_back(&tile);
        }
    }
    
    return visibleTiles;
}

void TileRenderer::AssignFacesToTiles(const BSP& map, const Camera& camera) {
    const auto& faces = map.GetParser().GetFaces();
    const auto& texinfo = map.GetParser().GetTexinfo();
    
    // Clear previous assignments
    for (auto& tile : m_tiles) {
        tile.faceIndices.clear();
    }
    m_faceToTiles.clear();
    
    // Calculate view-projection matrix
    float aspect = static_cast<float>(m_viewportWidth) / m_viewportHeight;
    glm::mat4 viewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    
    // Process each face
    for (uint32_t faceIdx = 0; faceIdx < faces.size(); ++faceIdx) {
        const auto& face = faces[faceIdx];
        
        // Skip faces without valid texinfo or marked as nodraw
        if (face.texinfo < 0 || face.texinfo >= texinfo.size()) continue;
        if (texinfo[face.texinfo].flags & 0x0080) continue; // SURF_NODRAW
        
        // Get face vertices
        std::vector<glm::vec3> facePoints;
        map.GetParser().GetFaceVertices(face, facePoints);
        if (facePoints.size() < 3) continue;
        
        // Calculate screen-space bounds for this face
        TileBounds bounds;
        bool hasValidProjection = false;
        
        for (const auto& vertex : facePoints) {
            // Transform to clip space
            glm::vec4 clipPos = viewProj * glm::vec4(vertex, 1.0f);
            
            // Skip vertices behind camera
            if (clipPos.w <= 0.0f) continue;
            
            // Perspective divide to NDC
            glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
            
            // Check if in view frustum
            if (ndcPos.x < -1.0f || ndcPos.x > 1.0f ||
                ndcPos.y < -1.0f || ndcPos.y > 1.0f ||
                ndcPos.z < 0.0f || ndcPos.z > 1.0f) continue;
            
            bounds.expand(ndcPos);
            hasValidProjection = true;
        }
        
        // Skip faces that are completely outside view
        if (!hasValidProjection || !bounds.isValid()) continue;
        
        // Convert bounds to tile coordinates
        int minTileX = static_cast<int>(bounds.minX * m_tilesX);
        int minTileY = static_cast<int>(bounds.minY * m_tilesY);
        int maxTileX = static_cast<int>(bounds.maxX * m_tilesX);
        int maxTileY = static_cast<int>(bounds.maxY * m_tilesY);
        
        // Clamp to tile bounds
        minTileX = std::max(0, minTileX);
        minTileY = std::max(0, minTileY);
        maxTileX = std::min(static_cast<int>(m_tilesX - 1), maxTileX);
        maxTileY = std::min(static_cast<int>(m_tilesY - 1), maxTileY);
        
        // Assign face to overlapping tiles
        std::vector<uint32_t> assignedTiles;
        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                uint32_t tileIndex = ty * m_tilesX + tx;
                if (tileIndex < m_tiles.size()) {
                    m_tiles[tileIndex].faceIndices.push_back(faceIdx);
                    assignedTiles.push_back(tileIndex);
                }
            }
        }
        
        // Store face-to-tiles mapping for quick lookup
        if (!assignedTiles.empty()) {
            m_faceToTiles[faceIdx] = assignedTiles;
        }
    }
}

const std::vector<uint32_t>& TileRenderer::GetTileFaces(uint32_t tileX, uint32_t tileY) const {
    static const std::vector<uint32_t> empty;
    
    if (tileX >= m_tilesX || tileY >= m_tilesY) {
        return empty;
    }
    
    return m_tiles[tileY * m_tilesX + tileX].faceIndices;
}

void TileRenderer::ComputeTileFrustum(uint32_t tileX, uint32_t tileY, const Camera& camera) {
    ScreenTile& tile = m_tiles[tileY * m_tilesX + tileX];
    
    // Convert tile bounds from [0,1] to NDC [-1,1]
    float ndcMinX = tile.minX * 2.0f - 1.0f;
    float ndcMinY = tile.minY * 2.0f - 1.0f;
    float ndcMaxX = tile.maxX * 2.0f - 1.0f;
    float ndcMaxY = tile.maxY * 2.0f - 1.0f;
    
    // Get view-projection matrix
    float aspect = static_cast<float>(m_viewportWidth) / m_viewportHeight;
    glm::mat4 viewProj = camera.GetProjectionMatrix(aspect) * camera.GetViewMatrix();
    glm::mat4 invViewProj = glm::inverse(viewProj);
    
    // Define 8 corners of the tile frustum in clip space
    // Near plane (z = 0 in NDC)
    glm::vec4 corners[8] = {
        glm::vec4(ndcMinX, ndcMinY, 0.0f, 1.0f),  // Bottom-left near
        glm::vec4(ndcMaxX, ndcMinY, 0.0f, 1.0f),  // Bottom-right near
        glm::vec4(ndcMaxX, ndcMaxY, 0.0f, 1.0f),  // Top-right near
        glm::vec4(ndcMinX, ndcMaxY, 0.0f, 1.0f),  // Top-left near
        // Far plane (z = 1 in NDC)
        glm::vec4(ndcMinX, ndcMinY, 1.0f, 1.0f),  // Bottom-left far
        glm::vec4(ndcMaxX, ndcMinY, 1.0f, 1.0f),  // Bottom-right far
        glm::vec4(ndcMaxX, ndcMaxY, 1.0f, 1.0f),  // Top-right far
        glm::vec4(ndcMinX, ndcMaxY, 1.0f, 1.0f)   // Top-left far
    };
    
    // Transform corners to world space
    glm::vec3 worldCorners[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 worldPos = invViewProj * corners[i];
        worldPos /= worldPos.w;
        worldCorners[i] = glm::vec3(worldPos);
    }
    
    // Calculate bounding sphere for the tile frustum
    glm::vec3 center = glm::vec3(0.0f);
    for (int i = 0; i < 8; ++i) {
        center += worldCorners[i];
    }
    center /= 8.0f;
    
    float maxRadiusSq = 0.0f;
    for (int i = 0; i < 8; ++i) {
        float distSq = glm::length(worldCorners[i] - center);
        distSq = distSq * distSq; // Square it to get length^2
        maxRadiusSq = std::max(maxRadiusSq, distSq);
    }
    tile.center = center;
    tile.radius = std::sqrt(maxRadiusSq);
    
    // Extract frustum planes from the 8 corners
    // Left plane: points 0, 3, 7, 4 (counter-clockwise when viewed from inside)
    tile.frustumPlanes[0] = glm::vec4(glm::normalize(glm::cross(worldCorners[3] - worldCorners[0], 
                                                               worldCorners[4] - worldCorners[0])), 0.0f);
    // Right plane: points 1, 2, 6, 5
    tile.frustumPlanes[1] = glm::vec4(glm::normalize(glm::cross(worldCorners[5] - worldCorners[1], 
                                                               worldCorners[2] - worldCorners[1])), 0.0f);
    // Bottom plane: points 0, 1, 5, 4
    tile.frustumPlanes[2] = glm::vec4(glm::normalize(glm::cross(worldCorners[4] - worldCorners[0], 
                                                               worldCorners[1] - worldCorners[0])), 0.0f);
    // Top plane: points 3, 2, 6, 7
    tile.frustumPlanes[3] = glm::vec4(glm::normalize(glm::cross(worldCorners[7] - worldCorners[3], 
                                                               worldCorners[2] - worldCorners[3])), 0.0f);
    // Near plane: points 0, 1, 2, 3
    tile.frustumPlanes[4] = glm::vec4(glm::normalize(glm::cross(worldCorners[1] - worldCorners[0], 
                                                               worldCorners[3] - worldCorners[0])), 0.0f);
    // Far plane: points 4, 7, 6, 5
    tile.frustumPlanes[5] = glm::vec4(glm::normalize(glm::cross(worldCorners[5] - worldCorners[4], 
                                                               worldCorners[7] - worldCorners[4])), 0.0f);
    
    // Set plane distances (d = -dot(normal, point_on_plane))
    tile.frustumPlanes[0].w = -glm::dot(glm::vec3(tile.frustumPlanes[0]), worldCorners[0]);
    tile.frustumPlanes[1].w = -glm::dot(glm::vec3(tile.frustumPlanes[1]), worldCorners[1]);
    tile.frustumPlanes[2].w = -glm::dot(glm::vec3(tile.frustumPlanes[2]), worldCorners[0]);
    tile.frustumPlanes[3].w = -glm::dot(glm::vec3(tile.frustumPlanes[3]), worldCorners[3]);
    tile.frustumPlanes[4].w = -glm::dot(glm::vec3(tile.frustumPlanes[4]), worldCorners[0]);
    tile.frustumPlanes[5].w = -glm::dot(glm::vec3(tile.frustumPlanes[5]), worldCorners[4]);
}

void TileRenderer::PerformTileCulling(const Camera& camera) {
    const glm::vec3 cameraPos = camera.GetPosition();
    
    for (auto& tile : m_tiles) {
        // Quick sphere culling against camera frustum
        bool isVisible = true;
        
        // Check against each frustum plane
        for (int i = 0; i < 6; ++i) {
            float dist = glm::dot(glm::vec3(tile.frustumPlanes[i]), tile.center) + tile.frustumPlanes[i].w;
            if (dist < -tile.radius) {
                isVisible = false;
                break;
            }
        }
        
        // Additional check: ensure tile is in front of camera (use a reasonable far plane)
        if (isVisible) {
            float distToCamera = glm::distance(tile.center, cameraPos);
            if (distToCamera > 10000.0f) { // Default far plane
                isVisible = false;
            }
        }
        
        tile.isVisible = isVisible;
    }
}

bool TileRenderer::IsPointVisible(const glm::vec3& worldPos) const {
    for (const auto& tile : m_tiles) {
        if (!tile.isVisible) continue;
        
        // Check against each frustum plane
        bool inside = true;
        for (int i = 0; i < 6; ++i) {
            float dist = glm::dot(glm::vec3(tile.frustumPlanes[i]), worldPos) + tile.frustumPlanes[i].w;
            if (dist < 0.0f) {
                inside = false;
                break;
            }
        }
        
        if (inside) return true;
    }
    return false;
}

bool TileRenderer::IsSphereVisible(const glm::vec3& center, float radius) const {
    for (const auto& tile : m_tiles) {
        if (!tile.isVisible) continue;
        
        // Check against each frustum plane
        bool intersects = true;
        for (int i = 0; i < 6; ++i) {
            float dist = glm::dot(glm::vec3(tile.frustumPlanes[i]), center) + tile.frustumPlanes[i].w;
            if (dist < -radius) {
                intersects = false;
                break;
            }
        }
        
        if (intersects) return true;
    }
    return false;
}

void TileRenderer::DrawDebugTiles() const {
    // This would be implemented with OpenGL drawing calls
    // For now, just log the tile information
    Logger::Info("[TileRenderer] Debug: " + std::to_string(m_stats.visibleTiles) + 
                 "/" + std::to_string(m_stats.totalTiles) + " tiles visible");
}

void TileRenderer::DrawDebugFaceAssignments(const BSP& map) const {
    // Log face assignment statistics
    Logger::Info("[TileRenderer] Debug face assignments:");
    for (uint32_t y = 0; y < m_tilesY; ++y) {
        for (uint32_t x = 0; x < m_tilesX; ++x) {
            const auto& faces = GetTileFaces(x, y);
            if (!faces.empty()) {
                Logger::Info("  Tile (" + std::to_string(x) + "," + std::to_string(y) + 
                             "): " + std::to_string(faces.size()) + " faces");
            }
        }
    }
}

} // namespace veex