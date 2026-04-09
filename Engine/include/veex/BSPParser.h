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
static constexpr int LUMP_LIGHTING              =  8;
static constexpr int LUMP_LEAVES                = 10;
static constexpr int LUMP_EDGES                 = 12;
static constexpr int LUMP_SURFEDGES             = 13;
static constexpr int LUMP_LEAF_FACES            = 16;
static constexpr int LUMP_LEAF_BRUSHES          = 17;
static constexpr int LUMP_TEXDATA_STRING_DATA   = 43;
static constexpr int LUMP_TEXDATA_STRING_TABLE  = 44;
static constexpr int LUMP_FACES_HDR             = 58;
static constexpr int LUMP_LIGHTING_HDR          = 53;
static constexpr int LUMP_WORLDLIGHTS           = 15;
static constexpr int LUMP_WORLDLIGHTS_HDR       = 54;
static constexpr int LUMP_LEAF_AMBIENT_INDEX    = 52;
static constexpr int LUMP_LEAF_AMBIENT_LIGHTING = 56;
static constexpr int LUMP_LEAF_AMBIENT_INDEX_HDR = 51;
static constexpr int LUMP_LEAF_AMBIENT_LIGHTING_HDR = 55;

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
    int            lightofs;
    float          area;
    int            LightmapTextureMinsInLuxels[2];
    int            LightmapTextureSizeInLuxels[2];
    int            origFace;
    unsigned short numPrims, firstPrimID;
    unsigned int   smoothingGroups;
};

struct texinfo_t {
    float textureVecs[2][4];
    float lightmapVecs[2][4];
    int   flags, texdata;
};

struct texdata_t {
    glm::vec3 reflectivity;
    int       nameStringTableID;
    int       width, height, view_width, view_height;
};

// ── World Lights (for RNM indirect lighting) ──────────────────────────────────
enum class EmitType : int {
    Surface    = 0,
    Point      = 1,
    Spotlight  = 2,
    Skylight   = 3,
    QuakeLight = 4,
    SkyAmbient = 5,
};

struct dworldlight_t {
    glm::vec3   origin;
    glm::vec3   intensity;
    glm::vec3   normal;
    int         cluster;
    EmitType    type;
    int         style;
    float       stopdot;
    float       stopdot2;
    float       exponent;
    float       radius;
    float       constant_attn;
    float       linear_attn;
    float       quadratic_attn;
    int         flags;
    int         texinfo;
    int         owner;
};

// ── Leaf Ambient Lighting (per-leaf ambient cube samples) ─────────────────────
struct dleafambientlighting_t {
    uint8_t cube[24];   // 6 faces * 4 channels (RGBA) compressed ambient cube
    uint8_t x, y, z;    // fixed-point position within leaf bounds
    uint8_t pad;
};

struct dleafambientindex_t {
    unsigned short ambientSampleCount;
    unsigned short firstAmbientSample;
};

// ── VIS / BSP Tree ────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct dleaf_t {
    int            contents;        // Essential for IsOccluded (Solid vs Air)
    short          cluster;
    short          area;
    short          mins[3], maxs[3];
    unsigned short firstleafface, numleaffaces;
    unsigned short firstleafbrush, numleafbrushes;
    short          leafWaterDataID;
    uint8_t        ambientLighting[24];
};

struct dnode_t {
    int            planenum;
    int            children[2];     // Negative values are (~leafIndex)
    short          mins[3], maxs[3];
    unsigned short firstface, numfaces;
    short          area, padding;
};
#pragma pack(pop)

// ── Lightmap Types ────────────────────────────────────────────────────────────
struct ColorRGBExp32 {
    uint8_t r, g, b;
    int8_t  exponent;
};

struct FaceLightmapInfo {
    glm::vec2 atlasOffset;
    glm::vec2 atlasScale;
    int       luxelW;
    int       luxelH;
    bool      valid = false;
};

// ── RNM (Radiosity Normal Map) Data ──────────────────────────────────────────
// Three orthogonal basis vectors for RNM lighting computation.
// These are the "bump frame" vectors used to project lighting into tangent space.
struct RNMVectors {
    glm::vec3 tangent;   // U direction
    glm::vec3 biTangent; // V direction
    glm::vec3 normal;    // N direction (face normal)
};

// Per-face RNM lighting data — stores the average radiosity for each basis direction
struct FaceRNMData {
    glm::vec3 radiosityU;    // Average lighting along tangent
    glm::vec3 radiosityV;    // Average lighting along bi-tangent
    glm::vec3 radiosityN;    // Average lighting along normal
    bool      valid = false;
};

// ── BSPParser ─────────────────────────────────────────────────────────────────
class BSPParser {
public:
    bool LoadFromFile(const std::string& path, const std::string& unused = "");

    // Geometry Getters
    const std::vector<dface_t>&   GetFaces()      const { return m_faces;   }
    const std::vector<texinfo_t>& GetTexinfo()    const { return m_texinfo; }
    const std::vector<dplane_t>&  GetPlanes()     const { return m_planes;  }
    const std::string&            GetEntityData() const { return m_entityData; }

    // Traversal Getters (Required for IsOccluded / TraceRay)
    const std::vector<dnode_t>&   GetNodes()      const { return m_nodes;   }
    const std::vector<dleaf_t>&   GetLeaves()     const { return m_leaves;  }

    void        GetFaceVertices(const dface_t& face, std::vector<glm::vec3>& out) const;
    glm::vec3   GetFaceNormal(const dface_t& face) const;
    std::string GetTextureName(int texinfoIndex) const;
    glm::ivec2  GetTextureDimensions(int texinfoIndex) const;

    // VIS
    std::unordered_set<int> GetVisibleFaceIndices(const glm::vec3& worldPos) const;
    bool HasVis() const { return m_visNumClusters > 0; }

    // Lightmap
    uint32_t BuildLightmapAtlas();
    uint32_t                             GetLightmapAtlasID()    const { return m_lightmapAtlasID; }
    const std::vector<FaceLightmapInfo>& GetFaceLightmapInfo()   const { return m_faceLightmapInfo; }

    // RNM (Radiosity Normal Maps)
    void ComputeRNMData();
    const std::vector<FaceRNMData>& GetFaceRNMData() const { return m_faceRNMData; }
    const std::vector<dworldlight_t>& GetWorldLights() const { return m_worldLights; }

    // Leaf ambient lighting
    const std::vector<dleafambientlighting_t>& GetLeafAmbientLighting() const { return m_leafAmbientLighting; }
    const std::vector<dleafambientindex_t>& GetLeafAmbientIndex() const { return m_leafAmbientIndex; }

    // Public utility for decoding RGBE format (used by RNM and lightmap systems)
    static glm::vec3 DecodeRGBExp32(const ColorRGBExp32& s);

private:
    template<typename T>
    void ReadLump(std::ifstream& file, int lumpIndex, std::vector<T>& out);
    void LoadEntities(std::ifstream& file);
    void LoadVisibility(std::ifstream& file);

    int  FindLeaf(const glm::vec3& pos) const;
    void DecompressPVS(int cluster, std::vector<uint8_t>& out) const;

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

    std::vector<dleaf_t>        m_leaves;
    std::vector<dnode_t>        m_nodes;
    std::vector<unsigned short> m_leafFaces;
    std::vector<unsigned short> m_leafBrushes;
    std::vector<uint8_t>        m_visData;
    int                         m_visNumClusters = 0;
    std::vector<int>            m_visPVSOffsets;

    std::vector<ColorRGBExp32>    m_lightingData;
    std::vector<FaceLightmapInfo> m_faceLightmapInfo;
    uint32_t                      m_lightmapAtlasID = 0;

    // RNM data
    std::vector<dworldlight_t>         m_worldLights;
    std::vector<dleafambientlighting_t> m_leafAmbientLighting;
    std::vector<dleafambientindex_t>   m_leafAmbientIndex;
    std::vector<FaceRNMData>           m_faceRNMData;
};

} // namespace veex