// veex/RenderGraph.cpp
// Dynamic render state management for face-centric BSP rendering.
// Implements SDK2013-compatible render passes and state sorting.

#include "veex/RenderGraph.h"
#include "veex/Logger.h"
#include "veex/GLHeaders.h"
#include <algorithm>

namespace veex {

// ── RenderPass Implementation ──────────────────────────────────────────────────

void RenderPass::SortBatches(const glm::vec3& cameraPos) {
    if (type == RenderPassType::Translucent || type == RenderPassType::Water) {
        // Sort back-to-front for transparency
        std::sort(batches.begin(), batches.end(), 
            [&cameraPos](const FaceBatch& a, const FaceBatch& b) {
                // Calculate batch centers
                glm::vec3 centerA(0), centerB(0);
                for (const auto& v : a.vertices) {
                    centerA += v.position;
                }
                for (const auto& v : b.vertices) {
                    centerB += v.position;
                }
                centerA /= (float)a.vertexCount;
                centerB /= (float)b.vertexCount;
                
                float distA = glm::distance2(cameraPos, centerA);
                float distB = glm::distance2(cameraPos, centerB);
                return distA > distB;  // Farthest first
            });
    } else {
        // Sort front-to-back for opaque passes (early Z optimization)
        std::sort(batches.begin(), batches.end(), 
            [&cameraPos](const FaceBatch& a, const FaceBatch& b) {
                glm::vec3 centerA(0), centerB(0);
                for (const auto& v : a.vertices) {
                    centerA += v.position;
                }
                for (const auto& v : b.vertices) {
                    centerB += v.position;
                }
                centerA /= (float)a.vertexCount;
                centerB /= (float)b.vertexCount;
                
                float distA = glm::distance2(cameraPos, centerA);
                float distB = glm::distance2(cameraPos, centerB);
                return distA < distB;  // Closest first
            });
    }
}

void RenderPass::Execute() {
    // Setup OpenGL state
    if (setupState) setupState();
    
    // Render all batches in this pass
    for (auto& batch : batches) {
        // Setup shader and textures
        if (batch.key.albedoID != 0) {
            // Bind textures
            // In a real implementation, you'd bind the actual texture IDs
        }
        
        // Draw indexed geometry
        if (!batch.indices.empty()) {
            glDrawElements(GL_TRIANGLES, (GLsizei)batch.indices.size(), GL_UNSIGNED_INT, 0);
        }
    }
    
    // Cleanup OpenGL state
    if (cleanupState) cleanupState();
}

// ── RenderGraph Implementation ────────────────────────────────────────────────

void RenderGraph::Build(const std::vector<FaceBatch>& batches, const glm::vec3& cameraPos) {
    Clear();
    
    // Create passes from batches
    CreatePasses(batches);
    
    // Sort passes
    SortPasses(cameraPos);
    
    Logger::Info("[RenderGraph] Built render graph with " + std::to_string(m_passes.size()) + " passes.");
}

void RenderGraph::Execute() {
    for (auto& pass : m_passes) {
        pass.Execute();
    }
}

void RenderGraph::Clear() {
    m_passes.clear();
}

RenderGraph::Statistics RenderGraph::GetStatistics() const {
    Statistics stats;
    for (const auto& pass : m_passes) {
        stats.totalPasses++;
        for (const auto& batch : pass.batches) {
            stats.totalBatches++;
            stats.totalVertices += batch.vertexCount;
            stats.totalIndices += batch.indexCount;
        }
    }
    return stats;
}

void RenderGraph::CreatePasses(const std::vector<FaceBatch>& batches) {
    // Group batches by render pass type
    std::unordered_map<RenderPassType, std::vector<FaceBatch>> passGroups;
    
    for (const auto& batch : batches) {
        RenderPassType passType = RenderPassType::Opaque;
        
        // Determine pass type from batch key
        if (batch.key.isSky) {
            passType = RenderPassType::Sky;
        } else if (batch.key.isWater) {
            passType = RenderPassType::Water;
        } else if (batch.key.isDisplacement) {
            passType = RenderPassType::Displacement;
        } else if (batch.key.blendMode != 0) {  // Has blending enabled
            passType = RenderPassType::Translucent;
        }
        
        passGroups[passType].push_back(batch);
    }
    
    // Create render passes
    for (auto& [passType, passBatches] : passGroups) {
        if (passBatches.empty()) continue;
        
        RenderPass pass;
        pass.type = passType;
        pass.batches = passBatches;
        
        // Create render state for this pass
        pass.state = CreateRenderState(passBatches[0]);
        
        // Setup state functions
        pass.setupState = [this, &pass]() {
            SetupRenderState(pass.state);
        };
        
        pass.cleanupState = [this, &pass]() {
            CleanupRenderState(pass.state);
        };
        
        m_passes.push_back(pass);
    }
}

void RenderGraph::SortPasses(const glm::vec3& cameraPos) {
    // Sort passes by type (opaque first, then translucent, then sky)
    std::sort(m_passes.begin(), m_passes.end(), 
        [](const RenderPass& a, const RenderPass& b) {
            // Define pass order
            auto priority = [](RenderPassType type) {
                switch (type) {
                    case RenderPassType::Opaque: return 0;
                    case RenderPassType::Shadow: return 1;
                    case RenderPassType::Displacement: return 2;
                    case RenderPassType::Water: return 3;
                    case RenderPassType::Translucent: return 4;
                    case RenderPassType::Sky: return 5;
                    case RenderPassType::Debug: return 6;
                    default: return 3;
                }
            };
            
            return priority(a.type) < priority(b.type);
        });
    
    // Sort batches within each pass
    for (auto& pass : m_passes) {
        pass.SortBatches(cameraPos);
    }
}

RenderState RenderGraph::CreateRenderState(const FaceBatch& batch) {
    RenderState state;
    
    // Set blend state
    if (batch.key.blendMode != 0) {
        state.blendEnabled = true;
        state.srcBlend = GL_SRC_ALPHA;
        state.dstBlend = GL_ONE_MINUS_SRC_ALPHA;
    }
    
    // Set depth state
    state.depthTest = true;
    state.depthWrite = !batch.key.isWater && !batch.key.isTranslucent;  // Don't write depth for transparent surfaces
    state.depthFunc = GL_LESS;
    
    // Set cull state
    state.cullEnabled = true;
    state.cullFace = GL_BACK;
    
    // Set two-sided for certain materials
    state.twoSided = batch.key.surfaceFlags & SURF_SKY;
    
    // Copy material parameters
    state.matParams = batch.matParams;
    
    return state;
}

void RenderGraph::SetupRenderState(const RenderState& state) {
    // Setup blending
    if (state.blendEnabled) {
        glEnable(GL_BLEND);
        glBlendFunc(state.srcBlend, state.dstBlend);
    } else {
        glDisable(GL_BLEND);
    }
    
    // Setup depth test
    if (state.depthTest) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(state.depthFunc);
        glDepthMask(state.depthWrite ? GL_TRUE : GL_FALSE);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    
    // Setup culling
    if (state.cullEnabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(state.cullFace);
    } else {
        glDisable(GL_CULL_FACE);
    }
    
    // Setup two-sided lighting if needed
    if (state.twoSided) {
        glDisable(GL_CULL_FACE);
    }
}

void RenderGraph::CleanupRenderState(const RenderState& state) {
    // Reset to default state
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

// ── RenderManager Implementation ──────────────────────────────────────────────

bool RenderManager::Initialize() {
    if (!InitializeShaders()) {
        Logger::Error("[RenderManager] Failed to initialize shaders.");
        return false;
    }
    
    if (!InitializeTextures()) {
        Logger::Error("[RenderManager] Failed to initialize textures.");
        return false;
    }
    
    Logger::Info("[RenderManager] Initialized successfully.");
    return true;
}

void RenderManager::Shutdown() {
    CleanupShaders();
    CleanupTextures();
    m_renderGraph.Clear();
    
    Logger::Info("[RenderManager] Shutdown complete.");
}

void RenderManager::Render(const std::vector<FaceBatch>& batches, const glm::vec3& cameraPos) {
    m_renderGraph.Build(batches, cameraPos);
    m_renderGraph.Execute();
    
    m_statistics = m_renderGraph.GetStatistics();
}

bool RenderManager::InitializeShaders() {
    // In a real implementation, you'd load and compile shaders here
    // For now, we'll just return true
    return true;
}

bool RenderManager::InitializeTextures() {
    // In a real implementation, you'd create texture objects here
    // For now, we'll just return true
    return true;
}

void RenderManager::CleanupShaders() {
    // Cleanup shader resources
}

void RenderManager::CleanupTextures() {
    // Cleanup texture resources
}

} // namespace veex