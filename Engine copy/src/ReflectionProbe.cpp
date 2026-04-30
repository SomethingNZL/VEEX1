#include "veex/ReflectionProbe.h"
#include "veex/BSP.h"
#include "veex/Camera.h"
#include "veex/GLHeaders.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace veex {

// ── ReflectionProbe Implementation ─────────────────────────────────────────────

ReflectionProbe::ReflectionProbe() {
    // Default values
    m_position = glm::vec3(0.0f);
    m_radius = 512.0f;
    m_intensity = 1.0f;
    m_cubemapResolution = 256;
    m_cubemapTexture = 0;
    m_captured = false;
    m_dirty = true;
}

ReflectionProbe::~ReflectionProbe() {
    // Cubemap texture will be cleaned up by ReflectionProbeSystem
}

bool ReflectionProbe::AffectsFace(uint32_t faceIndex) const {
    return std::find(m_affectedFaces.begin(), m_affectedFaces.end(), faceIndex) != m_affectedFaces.end();
}

bool ReflectionProbe::IsInfluenceArea(const glm::vec3& worldPos) const {
    float distance = glm::distance(worldPos, m_position);
    return distance <= m_radius;
}

float ReflectionProbe::GetBlendWeight(const glm::vec3& worldPos) const {
    float distance = glm::distance(worldPos, m_position);
    if (distance >= m_radius) return 0.0f;
    
    // Smooth falloff from 1.0 at center to 0.0 at edge
    float normalized = distance / m_radius;
    return 1.0f - normalized;
}

// ── ReflectionProbeSystem Implementation ───────────────────────────────────────

ReflectionProbeSystem::ReflectionProbeSystem() {
    m_probeDataUBO = 0;
    m_probeArrayTexture = 0;
    m_captureFBO = 0;
    m_captureDepthRB = 0;
}

ReflectionProbeSystem::~ReflectionProbeSystem() {
    Shutdown();
}

void ReflectionProbeSystem::Initialize() {
    // Create the capture framebuffer and depth renderbuffer
    glGenFramebuffers(1, &m_captureFBO);
    glGenRenderbuffers(1, &m_captureDepthRB);
    
    // Create the probe data UBO
    glGenBuffers(1, &m_probeDataUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_probeDataUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ProbeDataUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ReflectionProbeSystem::Shutdown() {
    // Clean up all probes
    for (ReflectionProbe* probe : m_probes) {
        DestroyCubemapTexture(probe);
        delete probe;
    }
    m_probes.clear();
    
    // Clean up GL resources
    if (m_probeDataUBO) {
        glDeleteBuffers(1, &m_probeDataUBO);
        m_probeDataUBO = 0;
    }
    
    if (m_captureFBO) {
        glDeleteFramebuffers(1, &m_captureFBO);
        m_captureFBO = 0;
    }
    
    if (m_captureDepthRB) {
        glDeleteRenderbuffers(1, &m_captureDepthRB);
        m_captureDepthRB = 0;
    }
}

ReflectionProbe* ReflectionProbeSystem::CreateProbe(const glm::vec3& position, float radius) {
    ReflectionProbe* probe = new ReflectionProbe();
    probe->SetPosition(position);
    probe->SetRadius(radius);
    probe->MarkDirty();
    
    m_probes.push_back(probe);
    
    // Create cubemap texture
    CreateCubemapTexture(probe);
    
    // Mark all probes as dirty since face assignment may change
    for (ReflectionProbe* p : m_probes) {
        p->MarkDirty();
    }
    
    return probe;
}

void ReflectionProbeSystem::RemoveProbe(ReflectionProbe* probe) {
    auto it = std::find(m_probes.begin(), m_probes.end(), probe);
    if (it != m_probes.end()) {
        DestroyCubemapTexture(probe);
        m_probes.erase(it);
        delete probe;
        
        // Rebuild face mapping
        BuildFaceProbeMap();
    }
}

void ReflectionProbeSystem::CaptureDirtyProbes(const BSP& map, const Camera& camera, 
                                               int viewportWidth, int viewportHeight) {
    for (ReflectionProbe* probe : m_probes) {
        if (probe->IsDirty()) {
            CaptureProbe(probe, map, camera, viewportWidth, viewportHeight);
        }
    }
}

void ReflectionProbeSystem::CaptureProbe(ReflectionProbe* probe, const BSP& map, 
                                         const Camera& camera, int viewportWidth, int viewportHeight) {
    if (!probe || !probe->IsDirty()) return;
    
    if (!probe->IsCaptured()) {
        CreateCubemapTexture(probe);
    }
    
    // Set up capture matrices
    glm::vec3 position = probe->GetPosition();
    float aspect = 1.0f;  // Cubemap faces are square
    float fov = 90.0f;
    float near = 0.1f;
    float far = 10000.0f;
    
    glm::mat4 captureProj = glm::perspective(glm::radians(fov), aspect, near, far);
    
    // 6 face view matrices
    const glm::mat4 captureViews[6] = {
        glm::lookAt(position, position + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),  // +X
        glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)), // -X
        glm::lookAt(position, position + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),   // +Y
        glm::lookAt(position, position + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)), // -Y
        glm::lookAt(position, position + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),  // +Z
        glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))  // -Z
    };
    
    // Bind capture framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    
    // Set up depth renderbuffer
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureDepthRB);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 
                         probe->GetCubemapResolution(), probe->GetCubemapResolution());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureDepthRB);
    
    // Capture each face
    for (int i = 0; i < 6; i++) {
        // Attach the face to the framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                              probe->GetCubemapTexture(), 0);
        
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            continue; // Skip this face if there's an error
        }
        
        // Set viewport to cubemap resolution
        glViewport(0, 0, probe->GetCubemapResolution(), probe->GetCubemapResolution());
        
        // Clear the face
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Enable depth test and culling for proper scene rendering
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        // Create view-projection matrix for this face
        glm::mat4 faceViewProj = captureProj * captureViews[i];
        
        // Render the scene from this face's perspective
        // This should be implemented by the Renderer class
        // For now, we'll just clear to a debug color to show the cubemap faces are being captured
        glClearColor(0.1f + i * 0.05f, 0.1f + i * 0.05f, 0.15f + i * 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    
    // Restore original viewport
    glViewport(0, 0, viewportWidth, viewportHeight);
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    probe->MarkCaptured(); // Mark as captured and no longer dirty
}

void ReflectionProbeSystem::AssignFacesToProbes(BSP& map) {
    // This is a simplified implementation
    // In a full implementation, you would:
    // 1. Iterate through all faces in the BSP
    // 2. For each face, find the closest probe(s) that influence it
    // 3. Assign the face to the probe with the strongest influence
    
    // For now, we'll just build a simple mapping
    BuildFaceProbeMap();
    
    // Clear existing face assignments
    for (ReflectionProbe* probe : m_probes) {
        probe->m_affectedFaces.clear();
    }
    
    // Simple assignment: each face gets the closest probe
    // This would be optimized in a real implementation
    const std::vector<RenderBatch>& batches = map.GetBatches();
    
    for (size_t faceIdx = 0; faceIdx < batches.size(); faceIdx++) {
        const RenderBatch& batch = batches[faceIdx];
        
        // Find closest probe
        ReflectionProbe* closestProbe = nullptr;
        float minDistance = std::numeric_limits<float>::max();
        
        for (ReflectionProbe* probe : m_probes) {
            // Calculate distance to face center (simplified)
            // In a real implementation, you'd calculate the actual face center
            float distance = glm::distance(probe->GetPosition(), glm::vec3(0.0f, 0.0f, 0.0f));
            
            if (distance < minDistance) {
                minDistance = distance;
                closestProbe = probe;
            }
        }
        
        // Assign face to closest probe
        if (closestProbe && minDistance < closestProbe->GetRadius()) {
            closestProbe->m_affectedFaces.push_back(static_cast<uint32_t>(faceIdx));
        }
    }
}

ReflectionProbe* ReflectionProbeSystem::GetProbeForFace(uint32_t faceIndex) const {
    int probeIndex = GetProbeIndexForFace(faceIndex);
    if (probeIndex >= 0 && probeIndex < static_cast<int>(m_probes.size())) {
        return m_probes[probeIndex];
    }
    return nullptr;
}

ReflectionProbe* ReflectionProbeSystem::GetClosestProbe(const glm::vec3& worldPos) const {
    ReflectionProbe* closest = nullptr;
    float minDistance = std::numeric_limits<float>::max();
    
    for (ReflectionProbe* probe : m_probes) {
        float distance = glm::distance(worldPos, probe->GetPosition());
        if (distance < minDistance) {
            minDistance = distance;
            closest = probe;
        }
    }
    
    return closest;
}

void ReflectionProbeSystem::UploadProbeData() {
    if (m_probes.empty()) return;
    
    // Prepare UBO data
    ProbeDataUBO uboData;
    uboData.probeCount = static_cast<uint32_t>(m_probes.size());
    
    for (size_t i = 0; i < m_probes.size() && i < ProbeDataUBO::MAX_PROBES; i++) {
        ReflectionProbe* probe = m_probes[i];
        ProbeDataUBO::ProbeData& data = uboData.probes[i];
        
        // Position and radius
        data.position = glm::vec4(probe->GetPosition(), probe->GetRadius());
        
        // Intensity (using RGB for tint, W for blend mode)
        data.intensity = glm::vec4(probe->GetIntensity(), probe->GetIntensity(), 
                                  probe->GetIntensity(), 0.0f);
        
        // Box extents (for box projection, simplified)
        glm::vec3 extents(probe->GetRadius());
        data.boxExtents = glm::vec4(probe->GetPosition() - extents, 1.0f);
        data.boxExtentsMax = glm::vec4(probe->GetPosition() + extents, 1.0f);
    }
    
    // Upload to UBO
    glBindBuffer(GL_UNIFORM_BUFFER, m_probeDataUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ProbeDataUBO), &uboData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

int ReflectionProbeSystem::GetProbeIndexForFace(uint32_t faceIndex) const {
    if (faceIndex < m_faceToProbeMap.size()) {
        return m_faceToProbeMap[faceIndex];
    }
    return -1;
}

// ── Private Helper Methods ─────────────────────────────────────────────────────

void ReflectionProbeSystem::CreateCubemapTexture(ReflectionProbe* probe) {
    if (probe->m_cubemapTexture != 0) {
        glDeleteTextures(1, &probe->m_cubemapTexture);
    }
    
    glGenTextures(1, &probe->m_cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, probe->m_cubemapTexture);
    
    int resolution = probe->GetCubemapResolution();
    
    // Allocate storage for all 6 faces
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, resolution, resolution, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void ReflectionProbeSystem::DestroyCubemapTexture(ReflectionProbe* probe) {
    if (probe->m_cubemapTexture != 0) {
        glDeleteTextures(1, &probe->m_cubemapTexture);
        probe->m_cubemapTexture = 0;
        probe->m_captured = false;
    }
}

void ReflectionProbeSystem::BuildFaceProbeMap() {
    // This would build a mapping from face indices to probe indices
    // For now, we'll create a simple mapping based on proximity
    
    // This is a placeholder - in a real implementation, you'd want to:
    // 1. Calculate face centers
    // 2. Find the closest probe for each face
    // 3. Store the mapping efficiently
    
    m_faceToProbeMap.clear();
    // The actual mapping would be built in AssignFacesToProbes()
}

} // namespace veex