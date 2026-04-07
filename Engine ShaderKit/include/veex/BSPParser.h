#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <cstdint>
#include <glm/glm.hpp>

namespace veex {

// ── Source BSP Lump Indices ───────────────────────────────────────────────────
static constexpr int LUMP_ENTITIES              =  0;
static constexpr int LUMP_PLANES                =  1;
static constexpr int LUMP_TEXDATA               =  2;
static constexpr int LUMP_VERTEXES              =  3;
static constexpr int LUMP_VISIBILITY            =  4;
static constexpr int LUMP_NODES                 =  5;
static constexpr int LUMP_TEXINFO               =  6;
static constexpr int LUMP_FACES                 =  7;
static constexpr int LUMP_LIGHTING              =  8;  // LDR lightmap samples
static constexpr int LUMP_LEAVES                = 10;
static constexpr int LUMP_EDGES                 = 12;
static constexpr int LUMP_SURFEDGES             = 13;
static constexpr int LUMP_LEAF_FACES            = 16;
static constexpr int LUMP_TEXDATA_STRING_DATA   = 43;
static constexpr int LUMP_TEXDATA_STRING_TABLE  = 44;
static constexpr int LUMP_FACES_HDR             = 58;
static constexpr int LUMP_LIGHTING_HDR          = 53;  // HDR lightmap (preferred)

// ── BSP Header ────────────────────────────────────────────────────────────────
struct lump_t {
    int  offset, length, version;
    char fourCC[4];
};

struct BSPHeader {
    char   magic[4];
    int    version;
    lump_t lumps[64];
    int    mapRevision;
};

// ── BSP Geometry Structs ──────────────────────────────────────────────────────
struct dplane_t  { glm::vec3 normal; float dist; int type; };
struct dedge_t   { unsigned short v[2]; };

struct dface_t {
    unsigned short planenum;
    unsigned char  side, onNode;
    int            firstEdge;
    short          numEdges, texinfo, dispinfo, surfaceFogAttrib;
    unsigned char  styles[4];
    int            lightofs;                        // byte offset into LIGHTING lump
    float          area;
    int            LightmapTextureMinsInLuxels[2];  // luxel-space origin
    int            LightmapTextureSizeInLuxels[2];  // luxel-space size (w-1, h-1)
    int            origFace;
    unsigned short numPrims, firstPrimID;
    unsigned int   smoothingGroups;
};

struct texinfo_t {
    float textureVecs[2][4];   // albedo UV axes
    float lightmapVecs[2][4];  // lightmap UV axes (luxel space)
    int   flags, texdata;
};

struct texdata_t {
    glm::vec3 reflectivity;
    int       nameStringTableID;
    int       width, height, view_width, view_height;
};

// ── VIS / BSP Tree ────────────────────────────────────────────────────────────
struct dleaf_t {                      // 56 bytes — Orange Box format
    int            contents;
    short          cluster;
    short          area;
    short          mins[3], maxs[3];
    unsigned short firstleafface, numleaffaces;
    unsigned short firstleafbrush, numleafbrushes;
    short          leafWaterDataID;
    uint8_t        ambientLighting[24]; // CompressedLightCube
};

struct dnode_t {
    int            planenum;
    int            children[2];
    short          mins[3], maxs[3];
    unsigned short firstface, numfaces;
    short          area, paddding;
};

// ── Lightmap Types ────────────────────────────────────────────────────────────

// Source HDR lightmap sample: 3 bytes colour + 1 byte shared base-2 exponent.
// Decodes to linear HDR float via: linear = byte / 255.0 * 2^exponent
struct ColorRGBExp32 {
    uint8_t r, g, b;
    int8_t  exponent;
};

// Per-face lightmap rectangle in the packed atlas (filled by BuildLightmapAtlas).
struct FaceLightmapInfo {
    glm::vec2 atlasOffset;  // top-left UV in [0,1] atlas space
    glm::vec2 atlasScale;   // (luxelW / atlasW, luxelH / atlasH)
    int       luxelW;       // face lightmap width  in luxels
    int       luxelH;       // face lightmap height in luxels
    bool      valid = false;
};

// ── BSPParser ─────────────────────────────────────────────────────────────────
class BSPParser {
public:
    bool LoadFromFile(const std::string& path, const std::string& unused = "");

    // Geometry
    const std::vector<dface_t>&   GetFaces()      const { return m_faces;   }
    const std::vector<texinfo_t>& GetTexinfo()    const { return m_texinfo; }
    const std::string&            GetEntityData() const { return m_entityData; }

    void        GetFaceVertices(const dface_t& face, std::vector<glm::vec3>& out) const;
    glm::vec3   GetFaceNormal(const dface_t& face) const;
    std::string GetTextureName(int texinfoIndex) const;
    glm::ivec2  GetTextureDimensions(int texinfoIndex) const;

    // VIS
    std::unordered_set<int> GetVisibleFaceIndices(const glm::vec3& worldPos) const;
    bool HasVis() const { return m_visNumClusters > 0; }

    // ── Lightmap ──────────────────────────────────────────────────────────────
    // Call once after LoadFromFile (before BuildVertexBuffer).
    // Decodes every face's HDR lightmap samples, packs them shelf-first into a
    // power-of-two GL_RGB16F atlas, uploads it to OpenGL.
    // Returns the GL texture ID, or 0 on failure.
    uint32_t BuildLightmapAtlas();

    uint32_t                             GetLightmapAtlasID()    const { return m_lightmapAtlasID; }
    const std::vector<FaceLightmapInfo>& GetFaceLightmapInfo()   const { return m_faceLightmapInfo; }
    // ─────────────────────────────────────────────────────────────────────────

private:
    template<typename T>
    void ReadLump(std::ifstream& file, int lumpIndex, std::vector<T>& out);
    void LoadEntities(std::ifstream& file);
    void LoadVisibility(std::ifstream& file);

    int  FindLeaf(const glm::vec3& pos) const;
    void DecompressPVS(int cluster, std::vector<uint8_t>& out) const;

    // Decode one ColorRGBExp32 → linear HDR float RGB
    static glm::vec3 DecodeRGBExp32(const ColorRGBExp32& s);

    // ── BSP Data ──────────────────────────────────────────────────────────────
    BSPHeader               m_header{};
    std::vector<glm::vec3>  m_vertices;
    std::vector<dface_t>    m_faces;
    std::vector<dplane_t>   m_planes;
    std::vector<dedge_t>    m_edges;
    std::vector<int>        m_surfedges;
    std::vector<texinfo_t>  m_texinfo;
    std::vector<texdata_t>  m_texdata;
    std::vector<int>        m_texdataStringTable;
    std::vector<char>       m_texdataStringData;
    std::string             m_entityData;

    // VIS
    std::vector<dleaf_t>        m_leaves;
    std::vector<dnode_t>        m_nodes;
    std::vector<unsigned short> m_leafFaces;
    std::vector<uint8_t>        m_visData;
    int                         m_visNumClusters = 0;
    std::vector<int>            m_visPVSOffsets;

    // Lightmap
    std::vector<ColorRGBExp32>    m_lightingData;       // raw HDR samples
    std::vector<FaceLightmapInfo> m_faceLightmapInfo;   // per face, indexed by face index
    uint32_t                      m_lightmapAtlasID = 0;
};

} // namespace veex
