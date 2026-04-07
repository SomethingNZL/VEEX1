#include "veex/BSPParser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace veex {

bool BSPParser::LoadFromFile(const std::string& path, const std::string&)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(&m_header), sizeof(BSPHeader));
    if (std::strncmp(m_header.magic, "VBSP", 4) != 0) return false;

    ReadLump(file, LUMP_VERTEXES, m_vertices);

    ReadLump(file, LUMP_FACES_HDR, m_faces);
    if (m_faces.empty()) ReadLump(file, LUMP_FACES, m_faces);

    ReadLump(file, LUMP_TEXINFO,   m_texinfo);
    ReadLump(file, LUMP_TEXDATA,   m_texdata);
    ReadLump(file, LUMP_PLANES,    m_planes);
    ReadLump(file, LUMP_EDGES,     m_edges);
    ReadLump(file, LUMP_SURFEDGES, m_surfedges);
    ReadLump(file, LUMP_TEXDATA_STRING_TABLE, m_texdataStringTable);
    ReadLump(file, LUMP_TEXDATA_STRING_DATA,  m_texdataStringData);

    // NEW: load VIS-related lumps
    ReadLump(file, LUMP_NODES,      m_nodes);
    ReadLump(file, LUMP_LEAVES,     m_leaves);
    ReadLump(file, LUMP_LEAF_FACES, m_leafFaces);
    LoadVisibility(file);

    // DEBUG: verify dleaf_t stride matches what the BSP actually contains.
    // If raw_bytes % sizeof(dleaf_t) != 0 your struct is the wrong size for
    // this BSP version and FindLeaf will read garbage — tell us the numbers.
    {
        int rawBytes = m_header.lumps[LUMP_LEAVES].length;
        std::cout << "[VIS DEBUG] LUMP_LEAVES: raw_bytes=" << rawBytes
                  << "  sizeof(dleaf_t)=" << sizeof(dleaf_t)
                  << "  remainder=" << (rawBytes % (int)sizeof(dleaf_t))
                  << "  leaf_count=" << m_leaves.size()
                  << "  node_count=" << m_nodes.size()
                  << "  leafface_count=" << m_leafFaces.size()
                  << "  vis_clusters=" << m_visNumClusters
                  << std::endl;
    }

    LoadEntities(file);
    return true;
}

template<typename T>
void BSPParser::ReadLump(std::ifstream& file, int lumpIndex, std::vector<T>& out)
{
    const auto& lump = m_header.lumps[lumpIndex];
    if (lump.length <= 0) return;
    out.resize(lump.length / sizeof(T));
    file.seekg(lump.offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), lump.length);
}

void BSPParser::LoadEntities(std::ifstream& file)
{
    const auto& lump = m_header.lumps[LUMP_ENTITIES];
    if (lump.length <= 0) return;
    m_entityData.resize(lump.length);
    file.seekg(lump.offset, std::ios::beg);
    file.read(m_entityData.data(), lump.length);
}

// NEW: Load and parse the visibility lump
void BSPParser::LoadVisibility(std::ifstream& file)
{
    const auto& lump = m_header.lumps[LUMP_VISIBILITY];
    if (lump.length < 4) return;

    // Read the raw vis lump into m_visData
    m_visData.resize(lump.length);
    file.seekg(lump.offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(m_visData.data()), lump.length);

    // The first int is numclusters
    std::memcpy(&m_visNumClusters, m_visData.data(), sizeof(int));
    if (m_visNumClusters <= 0) return;

    // After numclusters there is an array of numclusters * 2 ints:
    //   { pvs_offset, pas_offset } — offsets are relative to start of vis lump
    m_visPVSOffsets.resize(m_visNumClusters);
    const int* table = reinterpret_cast<const int*>(m_visData.data() + sizeof(int));
    for (int i = 0; i < m_visNumClusters; ++i)
    {
        m_visPVSOffsets[i] = table[i * 2]; // PVS offset (skip PAS at [i*2+1])
    }
}

// NEW: RLE decompress a PVS bitset for 'cluster' into out[]
// Each byte covers 8 clusters; a zero byte is followed by a run-length skip count.
void BSPParser::DecompressPVS(int cluster, std::vector<uint8_t>& out) const
{
    out.assign(m_visNumClusters, 0);

    if (cluster < 0 || cluster >= m_visNumClusters) return;

    int offset = m_visPVSOffsets[cluster];
    int byteLen = (m_visNumClusters + 7) / 8;

    const uint8_t* src = m_visData.data() + offset;
    const uint8_t* end = m_visData.data() + m_visData.size();

    int outCluster = 0;
    while (outCluster < m_visNumClusters && src < end)
    {
        uint8_t b = *src++;
        if (b == 0)
        {
            // RLE: next byte is count of zero bytes to skip
            if (src >= end) break;
            int skip = *src++;
            outCluster += skip * 8;
        }
        else
        {
            // Expand 8 bits → 8 cluster visibility entries
            for (int bit = 0; bit < 8 && outCluster < m_visNumClusters; ++bit, ++outCluster)
            {
                out[outCluster] = (b >> bit) & 1;
            }
        }
    }
}

// NEW: Walk BSP node tree to find the leaf index containing 'pos'
int BSPParser::FindLeaf(const glm::vec3& pos) const
{
    if (m_nodes.empty() || m_planes.empty()) return -1;

    int nodeIdx = 0; // root node
    while (nodeIdx >= 0)
    {
        const dnode_t& node = m_nodes[nodeIdx];
        const dplane_t& plane = m_planes[node.planenum];

        float dist = glm::dot(plane.normal, pos) - plane.dist;
        nodeIdx = node.children[dist < 0.0f ? 1 : 0];
    }
    // Negative index encodes leaf: leaf = -(nodeIdx + 1)
    return -(nodeIdx + 1);
}

// NEW: Build the set of visible face indices from a given world-space position
std::unordered_set<int> BSPParser::GetVisibleFaceIndices(const glm::vec3& worldPos) const
{
    // No VIS data — caller will render everything
    if (!HasVis()) return {};

    int leafIdx = FindLeaf(worldPos);
    if (leafIdx < 0 || leafIdx >= (int)m_leaves.size()) return {};

    int cameraCluster = m_leaves[leafIdx].cluster;
    if (cameraCluster < 0) return {}; // solid leaf / no cluster

    // Decompress PVS for the camera's cluster
    std::vector<uint8_t> pvs;
    DecompressPVS(cameraCluster, pvs);

    // Walk every leaf; if its cluster is visible, add all its faces
    std::unordered_set<int> visibleFaces;
    for (int li = 0; li < (int)m_leaves.size(); ++li)
    {
        const dleaf_t& leaf = m_leaves[li];
        if (leaf.cluster < 0 || leaf.cluster >= (int)pvs.size()) continue;
        if (!pvs[leaf.cluster]) continue;

        for (int fi = 0; fi < leaf.numleaffaces; ++fi)
        {
            int lfIdx = leaf.firstleafface + fi;
            if (lfIdx >= 0 && lfIdx < (int)m_leafFaces.size())
                visibleFaces.insert(m_leafFaces[lfIdx]);
        }
    }
    return visibleFaces;
}

// ---- Unchanged methods below ----

std::string BSPParser::GetTextureName(int texinfoIndex) const
{
    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) return "fallback";
    int texdataIdx = m_texinfo[texinfoIndex].texdata;
    if (texdataIdx < 0 || texdataIdx >= (int)m_texdata.size()) return "fallback";
    int stringTableIdx = m_texdata[texdataIdx].nameStringTableID;
    if (stringTableIdx < 0 || stringTableIdx >= (int)m_texdataStringTable.size()) return "fallback";
    int stringOffset = m_texdataStringTable[stringTableIdx];
    if (stringOffset < 0 || stringOffset >= (int)m_texdataStringData.size()) return "fallback";
    return std::string(&m_texdataStringData[stringOffset]);
}

glm::ivec2 BSPParser::GetTextureDimensions(int texinfoIndex) const
{
    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) return glm::ivec2(128);
    int texdataIdx = m_texinfo[texinfoIndex].texdata;
    if (texdataIdx < 0 || texdataIdx >= (int)m_texdata.size()) return glm::ivec2(128);
    return glm::ivec2(m_texdata[texdataIdx].width, m_texdata[texdataIdx].height);
}

glm::vec3 BSPParser::GetFaceNormal(const dface_t& face) const
{
    if (face.planenum < 0 || face.planenum >= (int)m_planes.size())
        return glm::vec3(0, 1, 0);
    glm::vec3 n = m_planes[face.planenum].normal;
    if (face.side != 0) n = -n;
    return n;
}

void BSPParser::GetFaceVertices(const dface_t& face, std::vector<glm::vec3>& out) const
{
    out.clear();
    for (int i = 0; i < face.numEdges; i++) {
        int surfEdgeIdx = face.firstEdge + i;
        if (surfEdgeIdx < 0 || surfEdgeIdx >= (int)m_surfedges.size()) continue;
        int edgeIdx = m_surfedges[surfEdgeIdx];
        if (edgeIdx >= 0) {
            if (edgeIdx < (int)m_edges.size()) {
                int vIdx = m_edges[edgeIdx].v[0];
                if (vIdx >= 0 && vIdx < (int)m_vertices.size()) out.push_back(m_vertices[vIdx]);
            }
        } else {
            if (-edgeIdx < (int)m_edges.size()) {
                int vIdx = m_edges[-edgeIdx].v[1];
                if (vIdx >= 0 && vIdx < (int)m_vertices.size()) out.push_back(m_vertices[vIdx]);
            }
        }
    }
}

} // namespace veex