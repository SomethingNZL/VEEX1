#pragma once
// veex/ReflectionProbe.h
// Reflection probe system for capturing and applying environment reflections.
//
// Reflection probes work by:
// 1. Capturing a cubemap at the probe's position (rendering the scene in 6 directions)
// 2. Identifying faces that are near the probe and should use its reflection
// 3. Projecting the cubemap as a reflection on those faces based on view direction

#include "veex/Common.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace veex {

class BSP;
class Camera;

// ── ReflectionProbe ────────────────────────────────────────────────────────────
// A reflection probe captures the environment at a specific point in the world
// and provides reflection data for nearby surfaces.
class ReflectionProbe {
public:
    ReflectionProbe();
    ~ReflectionProbe();

    // ── Configuration ───────────────────────────────────────────────────────────
    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    void SetRadius(float radius) { m_radius = radius; }
    void SetIntensity(float intensity) { m_intensity = intensity; }
    void SetCubemapResolution(int resolution) { m_cubemapResolution = resolution; }

    glm::vec3 GetPosition() const { return m_position; }
    float GetRadius() const { return m_radius; }
    float GetIntensity() const { return m_intensity; }
    int GetCubemapResolution() const { return m_cubemapResolution; }

    // ── Cubemap Management ──────────────────────────────────────────────────────
    // Get the OpenGL texture ID for the cubemap
    uint32_t GetCubemapTexture() const { return m_cubemapTexture; }

    // Check if the cubemap has been captured
    bool IsCaptured() const { return m_captured; }

    // Mark the probe as needing recapture
    void MarkDirty() { m_dirty = true; }
    bool IsDirty() const { return m_dirty; }
    
    // Mark the probe as captured (called after cubemap capture)
    void MarkCaptured() { m_captured = true; m_dirty = false; }

    // ── Face Association ────────────────────────────────────────────────────────
    // Get the list of face indices that this probe affects
    const std::vector<uint32_t>& GetAffectedFaces() const { return m_affectedFaces; }

    // Check if a specific face is affected by this probe
    bool AffectsFace(uint32_t faceIndex) const;

    // ── Influence Testing ───────────────────────────────────────────────────────
    // Check if a world position is within the probe's influence radius
    bool IsInfluenceArea(const glm::vec3& worldPos) const;

    // Get the blend weight for a position (1.0 at center, 0.0 at edge)
    float GetBlendWeight(const glm::vec3& worldPos) const;

private:
    friend class ReflectionProbeSystem;

    // ── Internal State ──────────────────────────────────────────────────────────
    glm::vec3 m_position = glm::vec3(0.0f);
    float m_radius = 512.0f;              // Influence radius in world units
    float m_intensity = 1.0f;             // Reflection intensity multiplier
    int m_cubemapResolution = 256;        // Cubemap face resolution

    uint32_t m_cubemapTexture = 0;        // OpenGL cubemap texture ID
    bool m_captured = false;              // Has the cubemap been captured?
    bool m_dirty = true;                  // Does the cubemap need recapturing?

    // Faces that this probe affects (populated by ReflectionProbeSystem)
    std::vector<uint32_t> m_affectedFaces;
};

// ── ReflectionProbeSystem ──────────────────────────────────────────────────────
// Manages all reflection probes in the scene, handles cubemap capture,
// face assignment, and provides reflection data to the renderer.
class ReflectionProbeSystem {
public:
    ReflectionProbeSystem();
    ~ReflectionProbeSystem();

    // ── Lifecycle ───────────────────────────────────────────────────────────────
    void Initialize();
    void Shutdown();

    // ── Probe Management ────────────────────────────────────────────────────────
    // Create a new reflection probe at the given position
    ReflectionProbe* CreateProbe(const glm::vec3& position, float radius = 512.0f);

    // Remove a probe
    void RemoveProbe(ReflectionProbe* probe);

    // Get all probes
    const std::vector<ReflectionProbe*>& GetProbes() const { return m_probes; }

    // Get the number of probes
    size_t GetProbeCount() const { return m_probes.size(); }

    // Mark all probes as dirty (forces re-capture)
    void MarkAllProbesDirty();

    // ── Cubemap Capture ─────────────────────────────────────────────────────────
    // Capture cubemaps for all dirty probes
    void CaptureDirtyProbes(const BSP& map, const Camera& camera, int viewportWidth, int viewportHeight);

    // Capture a single probe's cubemap
    void CaptureProbe(ReflectionProbe* probe, const BSP& map, const Camera& camera, 
                      int viewportWidth, int viewportHeight);

    // ── Face Assignment ─────────────────────────────────────────────────────────
    // Assign faces to probes based on proximity (called when map loads or probes change)
    void AssignFacesToProbes(BSP& map);

    // Get the primary reflection probe for a face (nullptr if none)
    ReflectionProbe* GetProbeForFace(uint32_t faceIndex) const;

    // Get reflection probe closest to a world position
    ReflectionProbe* GetClosestProbe(const glm::vec3& worldPos) const;

    // ── Renderer Integration ────────────────────────────────────────────────────
    // Upload reflection probe data to the shader
    void UploadProbeData();

    // Get the UBO handle for reflection probe data
    uint32_t GetProbeDataUBO() const { return m_probeDataUBO; }

    // ── Face-to-Probe Mapping ───────────────────────────────────────────────────
    // Get the probe index for a face (for shader lookup)
    // Returns -1 if no probe affects this face
    int GetProbeIndexForFace(uint32_t faceIndex) const;

private:
    // ── Internal Helpers ────────────────────────────────────────────────────────
    void CreateCubemapTexture(ReflectionProbe* probe);
    void DestroyCubemapTexture(ReflectionProbe* probe);
    void BuildFaceProbeMap();

    // ── Probe Storage ───────────────────────────────────────────────────────────
    std::vector<ReflectionProbe*> m_probes;

    // ── Face to Probe Mapping ───────────────────────────────────────────────────
    // Maps face index to primary probe index (-1 = no probe)
    std::vector<int> m_faceToProbeMap;

    // ── GL Resources ────────────────────────────────────────────────────────────
    uint32_t m_probeDataUBO = 0;          // UBO for probe data
    uint32_t m_probeArrayTexture = 0;     // Texture array for all probe cubemaps

    // ── Capture Resources ───────────────────────────────────────────────────────
    uint32_t m_captureFBO = 0;
    uint32_t m_captureDepthRB = 0;
};

// ── Shader Reflection Probe Data ───────────────────────────────────────────────
// This structure matches the UBO layout in the shader
struct ProbeDataUBO {
    static constexpr int MAX_PROBES = 16;

    struct ProbeData {
        glm::vec4 position;       // xyz = position, w = radius
        glm::vec4 intensity;      // xyz = intensity tint, w = blend mode (0=blend, 1=box)
        glm::vec4 boxExtents;     // AABB min for box probes
        glm::vec4 boxExtentsMax;  // AABB max for box probes
    };

    uint32_t probeCount;
    uint32_t padding[3];
    ProbeData probes[MAX_PROBES];
};

} // namespace veex