#pragma once
// veex/RenderGraph.h
// Dynamic render state management for face-centric BSP rendering.
// Implements SDK2013-compatible render passes and state sorting.

#include "veex/Face.h"
#include "veex/Shader.h"
#include "veex/Texture.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace veex {

// ── Render Pass Types ─────────────────────────────────────────────────────────
enum class RenderPassType {
    Opaque,         // Standard opaque surfaces
    Translucent,    // Transparent surfaces (back-to-front)
    Water,          // Water surfaces with refraction
    Sky,            // Sky surfaces
    Displacement,   // Displacement surfaces
    Shadow,         // Shadow casting surfaces
    Debug           // Debug visualization
};

// ── Render State ──────────────────────────────────────────────────────────────
struct RenderState {
    // Shader state
    Shader* shader = nullptr;
    std::vector<Texture*> textures;
    
    // Blend state
    bool blendEnabled = false;
    GLenum srcBlend = GL_ONE;
    GLenum dstBlend = GL_ZERO;
    
    // Depth state
    bool depthTest = true;
    bool depthWrite = true;
    GLenum depthFunc = GL_LESS;
    
    // Cull state
    bool cullEnabled = true;
    GLenum cullFace = GL_BACK;
    
    // Face culling for two-sided materials
    bool twoSided = false;
    
    // Material parameters
    MaterialParams matParams;
    
    // Comparison operators for sorting
    bool operator==(const RenderState& other) const {
        return shader == other.shader &&
               textures == other.textures &&
               blendEnabled == other.blendEnabled &&
               srcBlend == other.srcBlend &&
               dstBlend == other.dstBlend &&
               depthTest == other.depthTest &&
               depthWrite == other.depthWrite &&
               depthFunc == other.depthFunc &&
               cullEnabled == other.cullEnabled &&
               cullFace == other.cullFace &&
               twoSided == other.twoSided &&
               matParams == other.matParams;
    }
};

// Hash function for RenderState
struct RenderStateHash {
    std::size_t operator()(const RenderState& state) const {
        std::size_t h = 0;
        auto hash_combine = [&h](std::size_t value) {
            h ^= value + 0x9e3779b9 + (h << 6) + (h >> 2);
        };
        
        hash_combine(std::hash<void*>{}(state.shader));
        for (auto* tex : state.textures) {
            hash_combine(std::hash<void*>{}(tex));
        }
        hash_combine(std::hash<bool>{}(state.blendEnabled));
        hash_combine(std::hash<int>{}(state.srcBlend));
        hash_combine(std::hash<int>{}(state.dstBlend));
        hash_combine(std::hash<bool>{}(state.depthTest));
        hash_combine(std::hash<bool>{}(state.depthWrite));
        hash_combine(std::hash<int>{}(state.depthFunc));
        hash_combine(std::hash<bool>{}(state.cullEnabled));
        hash_combine(std::hash<int>{}(state.cullFace));
        hash_combine(std::hash<bool>{}(state.twoSided));
        
        return h;
    }
};

// ── Render Pass ───────────────────────────────────────────────────────────────
struct RenderPass {
    RenderPassType type;
    std::vector<FaceBatch> batches;
    RenderState state;
    
    // Setup and cleanup functions
    std::function<void()> setupState;
    std::function<void()> cleanupState;
    
    // Sort batches within this pass
    void SortBatches(const glm::vec3& cameraPos);
    
    // Execute this render pass
    void Execute();
};

// ── Render Graph ──────────────────────────────────────────────────────────────
class RenderGraph {
public:
    // Build render graph from face batches
    void Build(const std::vector<FaceBatch>& batches, const glm::vec3& cameraPos);
    
    // Execute the render graph
    void Execute();
    
    // Clear the render graph
    void Clear();
    
    // Get statistics
    struct Statistics {
        int totalBatches = 0;
        int totalPasses = 0;
        int totalVertices = 0;
        int totalIndices = 0;
    };
    
    Statistics GetStatistics() const;
    
private:
    std::vector<RenderPass> m_passes;
    
    // Create render passes from batches
    void CreatePasses(const std::vector<FaceBatch>& batches);
    
    // Sort passes by type and distance
    void SortPasses(const glm::vec3& cameraPos);
    
    // Create render state for a batch
    RenderState CreateRenderState(const FaceBatch& batch);
    
    // Setup OpenGL state for a render state
    void SetupRenderState(const RenderState& state);
    
    // Cleanup OpenGL state
    void CleanupRenderState(const RenderState& state);
};

// ── Render Manager ────────────────────────────────────────────────────────────
class RenderManager {
public:
    // Initialize render manager
    bool Initialize();
    
    // Shutdown render manager
    void Shutdown();
    
    // Render face batches
    void Render(const std::vector<FaceBatch>& batches, const glm::vec3& cameraPos);
    
    // Get render statistics
    const RenderGraph::Statistics& GetStatistics() const { return m_statistics; }
    
private:
    RenderGraph m_renderGraph;
    RenderGraph::Statistics m_statistics;
    
    // Initialize shaders and textures
    bool InitializeShaders();
    bool InitializeTextures();
    
    // Cleanup resources
    void CleanupShaders();
    void CleanupTextures();
};

} // namespace veex