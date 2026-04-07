#include "veex/BSPParser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace veex {

bool BSPParser::LoadFromFile(const std::string& path, const std::string&)
{
    // Binary mode is essential for Windows compatibility
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(&m_header), sizeof(BSPHeader));
    if (std::strncmp(m_header.magic, "VBSP", 4) != 0) return false;

    ReadLump(file, LUMP_VERTEXES, m_vertices);
    
    ReadLump(file, LUMP_FACES_HDR, m_faces);
    if (m_faces.empty()) ReadLump(file, LUMP_FACES, m_faces);
    
    ReadLump(file, LUMP_TEXINFO, m_texinfo);
    ReadLump(file, LUMP_TEXDATA, m_texdata);
    ReadLump(file, LUMP_PLANES, m_planes); 
    ReadLump(file, LUMP_EDGES, m_edges);
    ReadLump(file, LUMP_SURFEDGES, m_surfedges);
    ReadLump(file, LUMP_TEXDATA_STRING_TABLE, m_texdataStringTable);
    ReadLump(file, LUMP_TEXDATA_STRING_DATA, m_texdataStringData);

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