#pragma once
// veex/BSP.h
// BSP world geometry loader & vertex-buffer builder.
//
// BSP exposes everything ShaderKit needs:
//   • GetBatches()  — sorted, grouped render batches (one draw call each)
//   • GetVertices() — flat vertex array uploaded to the VBO
//   • GetSun()      — directional light parsed from light_environment entity
//   • GetParser()   — access to lightmap atlas and raw BSP lumps

#include "veex/Common.h"
#include "veex/BSPParser.h"
#include "veex/EntityParser.h"
#include "veex/BSPTexturePacker.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

namespace veex {

struct GameInfo;

// ── SunData ───────────────────────────────────────────────────────────────────
// Directional light derived from the map's light_environment entity.
// Forwarded to SceneParams::sunDirection / sunColor by the Renderer.
struct SunData {
    glm::vec3 direction = glm::normalize(glm::vec3(0.1f, -1.0f, 0.1f));
    glm::vec3 color     = glm::vec3(1.0f);
    float     intensity = 1.0f;   // combined from _light intensity channel
};

// ── BSP ───────────────────────────────────────────────────────────────────────
class BSP {
public:
    BSP()  = default;
    ~BSP() = default;

    // ── Loading ───────────────────────────────────────────────────────────────
    // Loads the BSP file, parses sun data, builds the lightmap atlas, and
    // calls BuildVertexBuffer() automatically.
    bool LoadFromFile(const std::string& path, const GameInfo& game);

    // Rebuilds m_vertices and m_batches from the parsed BSP data.
    // Called automatically by LoadFromFile; exposed for hot-reload scenarios.
    bool BuildVertexBuffer();

    // ── Geometry accessors (for Renderer / ShaderKit) ─────────────────────────
    const std::vector<Vertex>&      GetVertices() const { return m_vertices; }
    const std::vector<RenderBatch>& GetBatches()  const { return m_batches;  }

    // Returns only the batches visible from worldPos using BSP PVS.
    // Falls back to all batches when VIS data is unavailable.
    const std::vector<RenderBatch>& GetVisibleBatches(const glm::vec3& worldPos) const;

    // ── Scene data accessors (ShaderKit / SceneParams) ────────────────────────
    const SunData&   GetSun()    const { return m_sun;    }
    const BSPParser& GetParser() const { return m_parser; }

    // ── Batch statistics ──────────────────────────────────────────────────────
    // Exposed so Logger / debug UI can report savings from draw-call batching.
    struct BatchStats {
        int totalFaces    = 0;   // raw face count from BSP
        int totalBatches  = 0;   // actual glDrawArrays calls issued
        int totalVertices = 0;
    };

    // ── Traversal & Occlusion (SoundKit / Gameplay) ───────────────────────────
    // Fixed: These declarations must be present for BSP.cpp to compile.
    bool IsOccluded(const glm::vec3& start, const glm::vec3& end) const;
    bool TraceRay(int nodeIdx, float tStart, float tEnd, const glm::vec3& p1, const glm::vec3& p2) const;

    BatchStats GetBatchStats() const {
        return { m_lastFaceCount,
                 static_cast<int>(m_batches.size()),
                 static_cast<int>(m_vertices.size()) };
    }

private:
    // ── Internal helpers ──────────────────────────────────────────────────────
    void       ParseSun();
    glm::vec2  ComputeTexCoord    (const glm::vec3& pos, const texinfo_t& ti,
                                   float texW, float texH) const;
    glm::vec2  ComputeLightmapCoord(const glm::vec3& pos, const texinfo_t& ti,
                                    const dface_t& face,
                                    const FaceLightmapInfo& lmInfo) const;

    BSPParser            m_parser;
    SunData              m_sun;
    std::vector<Vertex>      m_vertices;
    std::vector<RenderBatch> m_batches;

    // Cached visible-batch list (rebuilt each GetVisibleBatches call when VIS
    // cluster changes; otherwise returned as-is to avoid per-frame allocation).
    mutable std::vector<RenderBatch> m_visibleBatchCache;
    mutable int m_lastVisCluster = -2;   // -2 = uninitialised

    // Texture atlas support
    std::unique_ptr<BSPTexturePacker> m_texturePacker;

    int m_lastFaceCount = 0;
};

} // namespace veex