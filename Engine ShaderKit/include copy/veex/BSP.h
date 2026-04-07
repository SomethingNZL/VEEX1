#pragma once
#include "veex/Common.h"
#include "veex/BSPParser.h"
#include <vector>
#include <string>

namespace veex {

class GameInfo;

class BSP {
public:
    BSP() = default;
    bool LoadFromFile(const std::string& path, const GameInfo& game);
    bool BuildVertexBuffer();

    const std::vector<Vertex>&      GetVertices() const { return m_vertices; }
    const std::vector<RenderBatch>& GetBatches()  const { return m_batches;  }
    const BSPParser&                GetParser()   const { return m_parser;   }

    // NEW: Returns only the batches (with corrected counts/offsets) that are
    // visible from worldPos, using the BSP PVS. Falls back to all batches if
    // VIS data is unavailable. Uses a scratch buffer to avoid allocation per frame.
    const std::vector<RenderBatch>& GetVisibleBatches(const glm::vec3& worldPos) const;

private:
    glm::vec2 ComputeTexCoord(const glm::vec3& pos, const texinfo_t& ti, float texW, float texH) const;

    BSPParser              m_parser;
    std::vector<Vertex>    m_vertices;
    std::vector<RenderBatch> m_batches;

    // NEW: per-face index → batch index, built in BuildVertexBuffer
    // maps global face index → which batch owns it (for VIS culling)
    struct FaceBatchEntry {
        int batchIndex;   // which batch this face contributed to
        int vertexOffset; // start vertex in m_vertices for this face's triangles
        int vertexCount;  // number of verts for this face
    };
    std::vector<FaceBatchEntry> m_faceBatchMap; // indexed by face index in parser

    // Mutable scratch buffer so GetVisibleBatches() can be const
    mutable std::vector<RenderBatch> m_visibleBatchScratch;
};

} // namespace veex
