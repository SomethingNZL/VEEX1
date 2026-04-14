// veex/BSPParser.cpp
// Raw VBSP lump reader, lightmap atlas builder, geometry helpers, VIS decoder.
// All std::cout calls replaced with Logger calls — log prefix [BSPParser].

#include "veex/BSPParser.h"
#include "veex/Logger.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <glad/gl.h>

namespace veex {

// ── File loading ──────────────────────────────────────────────────────────────

bool BSPParser::LoadFromFile(const std::string& path, const std::string&)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        Logger::Error("[BSPParser] Cannot open file: " + path);
        return false;
    }

    file.read(reinterpret_cast<char*>(&m_header), sizeof(BSPHeader));
    if (std::strncmp(m_header.magic, "VBSP", 4) != 0) {
        Logger::Error("[BSPParser] Bad magic — not a VBSP file: " + path);
        return false;
    }
    Logger::Info("[BSPParser] Loading VBSP version " +
                 std::to_string(m_header.version) + ": " + path);

    ReadLump(file, LUMP_VERTEXES,             m_vertices);
    ReadLump(file, LUMP_FACES_HDR,            m_faces);
    if (m_faces.empty()) ReadLump(file, LUMP_FACES, m_faces);
    ReadLump(file, LUMP_TEXINFO,              m_texinfo);
    ReadLump(file, LUMP_TEXDATA,              m_texdata);
    ReadLump(file, LUMP_PLANES,               m_planes);
    ReadLump(file, LUMP_EDGES,                m_edges);
    ReadLump(file, LUMP_SURFEDGES,            m_surfedges);
    ReadLump(file, LUMP_TEXDATA_STRING_TABLE, m_texdataStringTable);
    ReadLump(file, LUMP_TEXDATA_STRING_DATA,  m_texdataStringData);
    ReadLump(file, LUMP_NODES,                m_nodes);
    ReadLump(file, LUMP_LEAVES,               m_leaves);
    ReadLump(file, LUMP_LEAF_FACES,           m_leafFaces);
    ReadLump(file, LUMP_LEAF_BRUSHES,         m_leafBrushes);

    // Prefer HDR lighting lump; fall back to LDR.
    ReadLump(file, LUMP_LIGHTING_HDR, m_lightingData);
    if (m_lightingData.empty()) {
        ReadLump(file, LUMP_LIGHTING, m_lightingData);
        if (!m_lightingData.empty())
            Logger::Info("[BSPParser] HDR lighting lump absent — using LDR fallback.");
    }

    // Load world lights for RNM indirect lighting
    ReadLump(file, LUMP_WORLDLIGHTS_HDR, m_worldLights);
    if (m_worldLights.empty()) {
        ReadLump(file, LUMP_WORLDLIGHTS, m_worldLights);
        if (!m_worldLights.empty())
            Logger::Info("[BSPParser] HDR world lights lump absent — using LDR fallback.");
    }

    // Load leaf ambient lighting for RNM
    ReadLump(file, LUMP_LEAF_AMBIENT_LIGHTING_HDR, m_leafAmbientLighting);
    ReadLump(file, LUMP_LEAF_AMBIENT_INDEX_HDR, m_leafAmbientIndex);
    if (m_leafAmbientLighting.empty() || m_leafAmbientIndex.empty()) {
        ReadLump(file, LUMP_LEAF_AMBIENT_LIGHTING, m_leafAmbientLighting);
        ReadLump(file, LUMP_LEAF_AMBIENT_INDEX, m_leafAmbientIndex);
        if (!m_leafAmbientLighting.empty() && !m_leafAmbientIndex.empty())
            Logger::Info("[BSPParser] HDR leaf ambient lighting lumps absent — using LDR fallback.");
    }

    LoadVisibility(file);
    LoadEntities(file);

    // ── VIS sanity check ──────────────────────────────────────────────────────
    {
        int rawBytes = m_header.lumps[LUMP_LEAVES].length;
        int remainder = rawBytes % static_cast<int>(sizeof(dleaf_t));
        if (remainder != 0)
            Logger::Warn("[BSPParser] LUMP_LEAVES size " + std::to_string(rawBytes)
                         + " is not a multiple of dleaf_t (" +
                         std::to_string(sizeof(dleaf_t)) + ") — remainder=" +
                         std::to_string(remainder) + ". VIS may be unreliable.");
        Logger::Info("[BSPParser] Lumps: faces=" + std::to_string(m_faces.size())
                     + "  vertices=" + std::to_string(m_vertices.size())
                     + "  texinfo=" + std::to_string(m_texinfo.size())
                     + "  leaves=" + std::to_string(m_leaves.size())
                     + "  nodes=" + std::to_string(m_nodes.size())
                     + "  leaffaces=" + std::to_string(m_leafFaces.size())
                     + "  vis_clusters=" + std::to_string(m_visNumClusters)
                     + "  lighting_samples=" + std::to_string(m_lightingData.size()));
    }

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
    Logger::Info("[BSPParser] Entity lump: " + std::to_string(lump.length) + " bytes.");
}

void BSPParser::LoadVisibility(std::ifstream& file)
{
    const auto& lump = m_header.lumps[LUMP_VISIBILITY];
    if (lump.length < 4) {
        Logger::Warn("[BSPParser] VIS lump missing or too small — PVS culling disabled.");
        return;
    }
    m_visData.resize(lump.length);
    file.seekg(lump.offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(m_visData.data()), lump.length);

    std::memcpy(&m_visNumClusters, m_visData.data(), sizeof(int));
    if (m_visNumClusters <= 0) {
        Logger::Warn("[BSPParser] VIS cluster count=" + std::to_string(m_visNumClusters)
                     + " — PVS culling disabled.");
        return;
    }

    const int* table = reinterpret_cast<const int*>(m_visData.data() + sizeof(int));
    m_visPVSOffsets.resize(m_visNumClusters);
    for (int i = 0; i < m_visNumClusters; ++i)
        m_visPVSOffsets[i] = table[i * 2];

    Logger::Info("[BSPParser] VIS: " + std::to_string(m_visNumClusters)
                 + " clusters, " + std::to_string(lump.length) + " bytes.");
}

// ── Lightmap atlas ────────────────────────────────────────────────────────────

glm::vec3 BSPParser::DecodeRGBExp32(const ColorRGBExp32& s)
{
    // Source RGBE decode: channel = byte * 2^exponent / 255
    // Clamped to [0, 65504] to avoid Inf/NaN from extreme exponents.
    const float scale = std::pow(2.0f, static_cast<float>(s.exponent)) / 255.0f;
    return glm::clamp(glm::vec3(s.r, s.g, s.b) * scale, 0.0f, 65504.0f);
}

// Shelf-packing: sort faces by height descending, pack left-to-right, new
// shelf when a row overflows.  Efficient enough for < 10 k faces.
static void ShelfPack(
    const std::vector<std::pair<int,int>>& sizes,
    int maxAtlasW,
    std::vector<std::pair<int,int>>& outPositions,
    int& outAtlasW, int& outAtlasH)
{
    outPositions.assign(sizes.size(), {0, 0});
    int curX = 0, curY = 0, shelfH = 0;
    outAtlasW = outAtlasH = 0;

    for (size_t i = 0; i < sizes.size(); ++i) {
        int w = sizes[i].first, h = sizes[i].second;
        if (w <= 0 || h <= 0) continue;

        if (curX + w > maxAtlasW) {
            curY  += shelfH;
            curX   = 0;
            shelfH = 0;
        }
        outPositions[i] = { curX, curY };
        curX     += w;
        shelfH    = std::max(shelfH, h);
        outAtlasW = std::max(outAtlasW, curX);
        outAtlasH = std::max(outAtlasH, curY + shelfH);
    }
}

static int NextPOT(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

uint32_t BSPParser::BuildLightmapAtlas()
{
    if (m_lightingData.empty()) {
        Logger::Warn("[BSPParser] Lightmap: no lighting data — atlas not built.");
        return 0;
    }

    constexpr int kMaxAtlasSize = 4096;
    constexpr int kPadding      = 1;    // 1-luxel border to prevent bleeding

    const int numFaces = static_cast<int>(m_faces.size());
    m_faceLightmapInfo.resize(numFaces);

    // ── Step 1: per-face luxel dimensions ────────────────────────────────────
    std::vector<std::pair<int,int>> sizes(numFaces, {0, 0});
    int validFaces = 0;

    for (int fi = 0; fi < numFaces; ++fi) {
        const dface_t& face = m_faces[fi];
        m_faceLightmapInfo[fi].valid = false;

        if (face.lightofs < 0 || face.styles[0] == 255) continue;
        if (face.texinfo < 0 || face.texinfo >= (int)m_texinfo.size()) continue;
        if (m_texinfo[face.texinfo].flags & 0x0080) continue;

        int lw = face.LightmapTextureSizeInLuxels[0] + 1;
        int lh = face.LightmapTextureSizeInLuxels[1] + 1;
        if (lw <= 0 || lh <= 0) continue;

        sizes[fi] = { lw + kPadding, lh + kPadding };
        m_faceLightmapInfo[fi].luxelW = lw;
        m_faceLightmapInfo[fi].luxelH = lh;
        ++validFaces;
    }
    Logger::Info("[BSPParser] Lightmap: " + std::to_string(validFaces)
                 + "/" + std::to_string(numFaces) + " faces have lightmap data.");

    // ── Step 2: shelf-pack ────────────────────────────────────────────────────
    std::vector<std::pair<int,int>> positions;
    int packedW, packedH;
    ShelfPack(sizes, kMaxAtlasSize, positions, packedW, packedH);

    int atlasW = std::min(NextPOT(std::max(packedW, 4)), kMaxAtlasSize);
    int atlasH = std::min(NextPOT(std::max(packedH, 4)), kMaxAtlasSize);

    Logger::Info("[BSPParser] Lightmap atlas: " + std::to_string(atlasW)
                 + "x" + std::to_string(atlasH)
                 + "  packed_area=" + std::to_string(packedW) + "x" + std::to_string(packedH)
                 + "  efficiency=" +
                 std::to_string(static_cast<int>(
                     100.f * (static_cast<float>(packedW * packedH) /
                              static_cast<float>(atlasW * atlasH)))) + "%");

    // ── Step 3: decode HDR texels into atlas float buffer ─────────────────────
    std::vector<glm::vec3> atlasPixels(atlasW * atlasH, glm::vec3(0.0f));
    const int totalSamples = static_cast<int>(m_lightingData.size());
    int outOfBoundsFaces = 0;

    for (int fi = 0; fi < numFaces; ++fi) {
        FaceLightmapInfo& info = m_faceLightmapInfo[fi];
        int lw = info.luxelW, lh = info.luxelH;
        if (lw <= 0 || lh <= 0) continue;

        const dface_t& face = m_faces[fi];
        if (face.lightofs < 0) continue;

        const int startSample = face.lightofs / static_cast<int>(sizeof(ColorRGBExp32));
        const int numSamples  = lw * lh;
        if (startSample + numSamples > totalSamples) {
            ++outOfBoundsFaces;
            continue;
        }

        const int px = positions[fi].first;
        const int py = positions[fi].second;

        for (int y = 0; y < lh; ++y) {
            for (int x = 0; x < lw; ++x) {
                glm::vec3 hdr = DecodeRGBExp32(
                    m_lightingData[startSample + y * lw + x]);
                int idx = (py + y) * atlasW + (px + x);
                if (idx >= 0 && idx < static_cast<int>(atlasPixels.size()))
                    atlasPixels[idx] = hdr;
            }
        }

        // Clamp-pad right column and bottom row to prevent bilinear bleeding.
        if (px + lw < atlasW) {
            for (int y = 0; y < lh; ++y)
                atlasPixels[(py + y) * atlasW + px + lw] =
                    atlasPixels[(py + y) * atlasW + px + (lw - 1)];
        }
        if (py + lh < atlasH) {
            for (int x = 0; x < lw; ++x)
                atlasPixels[(py + lh) * atlasW + px + x] =
                    atlasPixels[(py + lh - 1) * atlasW + px + x];
        }

        info.atlasOffset = glm::vec2(static_cast<float>(px) / atlasW,
                                     static_cast<float>(py) / atlasH);
        info.atlasScale  = glm::vec2(static_cast<float>(lw) / atlasW,
                                     static_cast<float>(lh) / atlasH);
        info.valid = true;
    }

    if (outOfBoundsFaces > 0)
        Logger::Warn("[BSPParser] Lightmap: " + std::to_string(outOfBoundsFaces)
                     + " faces skipped — lighting lump out-of-bounds.");

    // ── Step 4: upload GL_RGB8 (more compatible than GL_RGB16F) ─────────────────
    if (m_lightmapAtlasID) glDeleteTextures(1, &m_lightmapAtlasID);

    glGenTextures(1, &m_lightmapAtlasID);
    glBindTexture(GL_TEXTURE_2D, m_lightmapAtlasID);
    
    // Convert HDR values to LDR for better compatibility
    std::vector<uint8_t> ldrPixels(atlasPixels.size() * 3);
    for (size_t i = 0; i < atlasPixels.size(); ++i) {
        // Apply simple tone mapping: divide by (1 + value) to compress HDR range
        glm::vec3 hdr = atlasPixels[i];
        glm::vec3 ldr = hdr / (1.0f + hdr);
        // Gamma correction (approximate sRGB)
        ldr = glm::pow(ldr, glm::vec3(1.0f / 2.2f));
        // Clamp and convert to 8-bit
        ldr = glm::clamp(ldr * 255.0f, 0.0f, 255.0f);
        ldrPixels[i*3+0] = static_cast<uint8_t>(ldr.x);
        ldrPixels[i*3+1] = static_cast<uint8_t>(ldr.y);
        ldrPixels[i*3+2] = static_cast<uint8_t>(ldr.z);
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,
                 atlasW, atlasH, 0,
                 GL_RGB, GL_UNSIGNED_BYTE,
                 ldrPixels.data());

    // Bilinear filtering — lightmaps are low-frequency; no mipmaps needed.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    Logger::Info("[BSPParser] Lightmap atlas uploaded: GL id="
                 + std::to_string(m_lightmapAtlasID)
                 + "  size=" + std::to_string(atlasW) + "x" + std::to_string(atlasH)
                 + "  format=GL_RGB8");
    return m_lightmapAtlasID;
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

std::string BSPParser::GetTextureName(int texinfoIndex) const
{
    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) return "fallback";
    int tdIdx = m_texinfo[texinfoIndex].texdata;
    if (tdIdx < 0 || tdIdx >= (int)m_texdata.size())                return "fallback";
    int stIdx = m_texdata[tdIdx].nameStringTableID;
    if (stIdx < 0 || stIdx >= (int)m_texdataStringTable.size())     return "fallback";
    int offset = m_texdataStringTable[stIdx];
    if (offset < 0 || offset >= (int)m_texdataStringData.size())    return "fallback";
    return std::string(&m_texdataStringData[offset]);
}

glm::ivec2 BSPParser::GetTextureDimensions(int texinfoIndex) const
{
    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) return glm::ivec2(128);
    int tdIdx = m_texinfo[texinfoIndex].texdata;
    if (tdIdx < 0 || tdIdx >= (int)m_texdata.size())                return glm::ivec2(128);
    return glm::ivec2(m_texdata[tdIdx].width, m_texdata[tdIdx].height);
}

glm::vec3 BSPParser::GetFaceNormal(const dface_t& face) const
{
    if (face.planenum < 0 || face.planenum >= (int)m_planes.size())
        return glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 n = m_planes[face.planenum].normal;
    if (face.side != 0) n = -n;
    return n;
}

void BSPParser::GetFaceVertices(const dface_t& face,
                                std::vector<glm::vec3>& out) const
{
    out.clear();
    out.reserve(face.numEdges);
    for (int i = 0; i < face.numEdges; ++i) {
        int seIdx = face.firstEdge + i;
        if (seIdx < 0 || seIdx >= (int)m_surfedges.size()) continue;
        int eIdx = m_surfedges[seIdx];
        int vIdx;
        if (eIdx >= 0) {
            if (eIdx >= (int)m_edges.size()) continue;
            vIdx = m_edges[eIdx].v[0];
        } else {
            if (-eIdx >= (int)m_edges.size()) continue;
            vIdx = m_edges[-eIdx].v[1];
        }
        if (vIdx >= 0 && vIdx < (int)m_vertices.size())
            out.push_back(m_vertices[vIdx]);
    }
}

// ── VIS ───────────────────────────────────────────────────────────────────────

void BSPParser::DecompressPVS(int cluster, std::vector<uint8_t>& out) const
{
    out.assign(m_visNumClusters, 0);
    if (cluster < 0 || cluster >= m_visNumClusters) return;

    const uint8_t* src = m_visData.data() + m_visPVSOffsets[cluster];
    const uint8_t* end = m_visData.data() + m_visData.size();
    int outCluster = 0;

    while (outCluster < m_visNumClusters && src < end) {
        uint8_t b = *src++;
        if (b == 0) {
            if (src >= end) break;
            outCluster += (*src++) * 8;
        } else {
            for (int bit = 0; bit < 8 && outCluster < m_visNumClusters; ++bit, ++outCluster)
                out[outCluster] = (b >> bit) & 1;
        }
    }
}

int BSPParser::FindLeaf(const glm::vec3& pos) const
{
    if (m_nodes.empty() || m_planes.empty()) return -1;
    int nodeIdx = 0;
    while (nodeIdx >= 0) {
        const dnode_t&  node  = m_nodes[nodeIdx];
        const dplane_t& plane = m_planes[node.planenum];
        float dist = glm::dot(plane.normal, pos) - plane.dist;
        nodeIdx = node.children[dist < 0.0f ? 1 : 0];
    }
    return -(nodeIdx + 1);
}

std::unordered_set<int> BSPParser::GetVisibleFaceIndices(const glm::vec3& worldPos) const
{
    if (!HasVis()) return {};

    const int leafIdx = FindLeaf(worldPos);
    if (leafIdx < 0 || leafIdx >= (int)m_leaves.size()) return {};

    const int camCluster = m_leaves[leafIdx].cluster;
    if (camCluster < 0) return {};

    std::vector<uint8_t> pvs;
    DecompressPVS(camCluster, pvs);

    std::unordered_set<int> visFaces;
    for (int li = 0; li < (int)m_leaves.size(); ++li) {
        const dleaf_t& leaf = m_leaves[li];
        if (leaf.cluster < 0 || leaf.cluster >= (int)pvs.size()) continue;
        if (!pvs[leaf.cluster]) continue;
        for (int fi = 0; fi < leaf.numleaffaces; ++fi) {
            int lfIdx = leaf.firstleafface + fi;
            if (lfIdx >= 0 && lfIdx < (int)m_leafFaces.size())
                visFaces.insert(m_leafFaces[lfIdx]);
        }
    }
    return visFaces;
}

// ── Face Info Resolution ──────────────────────────────────────────────────────

FaceInfo BSPParser::GetFaceInfo(int faceIndex) const {
    FaceInfo info;

    if (faceIndex < 0 || faceIndex >= (int)m_faces.size()) {
        info.shouldRender = false;
        info.materialType = FaceMaterialType::NoDraw;
        return info;
    }

    const dface_t& face = m_faces[faceIndex];

    // Get texinfo flags
    if (face.texinfo >= 0 && face.texinfo < (int)m_texinfo.size()) {
        info.surfaceFlags = static_cast<uint32_t>(m_texinfo[face.texinfo].flags);
    }

    // Get content flags from the leaf this face belongs to
    // Find which leaf contains this face
    for (int li = 0; li < (int)m_leaves.size(); ++li) {
        const dleaf_t& leaf = m_leaves[li];
        for (int lfi = 0; lfi < leaf.numleaffaces; ++lfi) {
            int lfIdx = leaf.firstleafface + lfi;
            if (lfIdx >= 0 && lfIdx < (int)m_leafFaces.size() && m_leafFaces[lfIdx] == faceIndex) {
                info.contentFlags = static_cast<uint32_t>(leaf.contents);
                break;
            }
        }
    }

    // Determine face properties from flags
    info.isSky = (info.surfaceFlags & SURF_SKY) != 0 || (info.surfaceFlags & SURF_SKY2D) != 0;
    info.isWater = (info.contentFlags & CONTENTS_WATER) != 0;
    info.isTranslucent = (info.surfaceFlags & SURF_TRANS) != 0;
    info.isWarp = (info.surfaceFlags & SURF_WARP) != 0;
    info.hasLightmap = (face.lightofs >= 0 && face.styles[0] != 255);
    info.castsShadows = (info.surfaceFlags & SURF_NOSHADOWS) == 0;
    info.receivesDecals = (info.surfaceFlags & SURF_NODECALS) == 0;
    info.shouldRender = (info.surfaceFlags & SURF_NODRAW) == 0 &&
                        (info.surfaceFlags & SURF_SKIP) == 0 &&
                        (info.surfaceFlags & SURF_HINT) == 0;

    // Determine material type
    if (info.isSky) {
        info.materialType = FaceMaterialType::Sky;
    } else if (info.isWater) {
        info.materialType = FaceMaterialType::Water;
    } else if (info.isWarp) {
        info.materialType = FaceMaterialType::Warp;
    } else if (info.isTranslucent) {
        info.materialType = FaceMaterialType::Translucent;
    } else if (!info.shouldRender) {
        info.materialType = FaceMaterialType::NoDraw;
    } else {
        info.materialType = FaceMaterialType::Standard;
    }

    return info;
}

FaceMaterialType BSPParser::GetFaceMaterialType(int texinfoIndex) const {
    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) {
        return FaceMaterialType::Standard;
    }

    uint32_t flags = static_cast<uint32_t>(m_texinfo[texinfoIndex].flags);

    if (flags & SURF_SKY) return FaceMaterialType::Sky;
    if (flags & SURF_SKY2D) return FaceMaterialType::Sky;
    if (flags & SURF_WARP) return FaceMaterialType::Warp;
    if (flags & SURF_TRANS) return FaceMaterialType::Translucent;
    if (flags & SURF_NODRAW) return FaceMaterialType::NoDraw;
    if (flags & SURF_HINT) return FaceMaterialType::Hint;

    return FaceMaterialType::Standard;
}

bool BSPParser::ShouldRenderFace(int faceIndex) const {
    if (faceIndex < 0 || faceIndex >= (int)m_faces.size()) {
        return false;
    }

    const dface_t& face = m_faces[faceIndex];
    if (face.texinfo < 0 || face.texinfo >= (int)m_texinfo.size()) {
        return false;
    }

    uint32_t flags = static_cast<uint32_t>(m_texinfo[face.texinfo].flags);
    return (flags & SURF_NODRAW) == 0 &&
           (flags & SURF_SKIP) == 0 &&
           (flags & SURF_HINT) == 0;
}

const CachedTexInfo& BSPParser::GetCachedTexInfo(int texinfoIndex) const {
    // Static fallback for invalid indices
    static CachedTexInfo s_fallback;

    if (texinfoIndex < 0 || texinfoIndex >= (int)m_texinfo.size()) {
        return s_fallback;
    }

    // Check if we need to expand the cache
    if (m_cachedTexInfo.empty()) {
        // Lazy initialization - create cache on first access
        const_cast<BSPParser*>(this)->m_cachedTexInfo.resize(m_texinfo.size());
    }

    CachedTexInfo& cached = const_cast<CachedTexInfo&>(m_cachedTexInfo[texinfoIndex]);

    if (!cached.isCached) {
        // Populate the cache
        cached.name = GetTextureName(texinfoIndex);
        cached.dimensions = GetTextureDimensions(texinfoIndex);
        cached.surfaceFlags = static_cast<uint32_t>(m_texinfo[texinfoIndex].flags);

        // Get reflectivity from texdata
        int texdataIdx = m_texinfo[texinfoIndex].texdata;
        if (texdataIdx >= 0 && texdataIdx < (int)m_texdata.size()) {
            cached.reflectivity = m_texdata[texdataIdx].reflectivity;
        } else {
            cached.reflectivity = glm::vec3(1.0f);
        }

        cached.materialType = GetFaceMaterialType(texinfoIndex);
        cached.isCached = true;
    }

    return cached;
}

} // namespace veex
