#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <cstdint>
#include <glm/glm.hpp>

namespace veex {

// Source BSP Constants
static constexpr int LUMP_ENTITIES              = 0;
static constexpr int LUMP_PLANES                = 1;
static constexpr int LUMP_VERTEXES              = 3;
static constexpr int LUMP_VISIBILITY            = 4;  // NEW: PVS data
static constexpr int LUMP_NODES                 = 5;  // NEW: BSP nodes (for leaf lookup)
static constexpr int LUMP_TEXINFO               = 6;
static constexpr int LUMP_FACES                 = 7;
static constexpr int LUMP_LEAVES                = 10; // NEW: BSP leaves
static constexpr int LUMP_LEAF_FACES            = 16; // NEW: leaf -> face index list
static constexpr int LUMP_TEXDATA               = 2;
static constexpr int LUMP_EDGES                 = 12;
static constexpr int LUMP_SURFEDGES             = 13;
static constexpr int LUMP_FACES_HDR             = 58;
static constexpr int LUMP_TEXDATA_STRING_DATA   = 43;
static constexpr int LUMP_TEXDATA_STRING_TABLE  = 44;

struct lump_t {
    int offset;
    int length;
    int version;
    char fourCC[4];
};

struct BSPHeader {
    char magic[4]; // VBSP
    int version;
    lump_t lumps[64];
    int mapRevision;
};

struct dplane_t {
    glm::vec3 normal;
    float dist;
    int type;
};

struct dedge_t {
    unsigned short v[2];
};

struct dface_t {
    unsigned short planenum;
    unsigned char side;
    unsigned char onNode;
    int firstEdge;
    short numEdges;
    short texinfo;
    short dispinfo;
    short surfaceFogAttrib;
    unsigned char styles[4];
    int lightofs;
    float area;
    int LightmapTextureMinsInLuxels[2];
    int LightmapTextureSizeInLuxels[2];
    int origFace;
    unsigned short numPrims;
    unsigned short firstPrimID;
    unsigned int smoothingGroups;
};

struct texinfo_t {
    float textureVecs[2][4];
    float lightmapVecs[2][4];
    int flags;
    int texdata;
};

struct texdata_t {
    glm::vec3 reflectivity;
    int nameStringTableID;
    int width, height;
    int view_width, view_height;
};

// --- NEW VIS STRUCTURES ---

// Source BSP leaf — Orange Box / Source 2007 format = exactly 56 bytes.
// The final 24 bytes are the ambient lighting cube (6 x CompressedLightCube).
// Getting this wrong corrupts the entire leaf array and breaks FindLeaf.
struct dleaf_t {
    int            contents;           //  4
    short          cluster;            //  2  PVS cluster index, -1 = no cluster
    short          area;               //  2
    short          mins[3];            //  6
    short          maxs[3];            //  6
    unsigned short firstleafface;      //  2
    unsigned short numleaffaces;       //  2
    unsigned short firstleafbrush;     //  2
    unsigned short numleafbrushes;     //  2
    short          leafWaterDataID;    //  2
    uint8_t        ambientLighting[24];//  24  (6 faces x 4 bytes)
    // Total: 56 bytes
};

// Source BSP node
struct dnode_t {
    int      planenum;
    int      children[2]; // positive = node index, negative = -(leaf+1)
    short    mins[3];
    short    maxs[3];
    unsigned short firstface;
    unsigned short numfaces;
    short    area;
    short    paddding;
};

// Header at the start of the visibility lump
struct dvis_t {
    int numclusters;
    // Followed by numclusters pairs of ints: { pvs_offset, pas_offset }
    // We read these dynamically.
};

// --------------------------

class BSPParser {
public:
    bool LoadFromFile(const std::string& path, const std::string& unused = "");

    const std::vector<dface_t>&   GetFaces()   const { return m_faces;   }
    const std::vector<texinfo_t>& GetTexinfo() const { return m_texinfo; }
    const std::string&            GetEntityData() const { return m_entityData; }

    void        GetFaceVertices(const dface_t& face, std::vector<glm::vec3>& out) const;
    glm::vec3   GetFaceNormal(const dface_t& face) const;
    std::string GetTextureName(int texinfoIndex) const;
    glm::ivec2  GetTextureDimensions(int texinfoIndex) const;

    // NEW: Returns the set of face indices visible from the given world-space position.
    // Returns an empty set if VIS data is unavailable (caller should render everything).
    std::unordered_set<int> GetVisibleFaceIndices(const glm::vec3& worldPos) const;

    bool HasVis() const { return m_visNumClusters > 0; }

private:
    template<typename T>
    void ReadLump(std::ifstream& file, int lumpIndex, std::vector<T>& out);
    void LoadEntities(std::ifstream& file);
    void LoadVisibility(std::ifstream& file);  // NEW

    // Walk the BSP node tree to find which leaf contains a point
    int FindLeaf(const glm::vec3& pos) const;

    // Decompress RLE bitset for a given cluster into a boolean array
    // out must be at least m_visNumClusters bytes
    void DecompressPVS(int cluster, std::vector<uint8_t>& out) const;

    BSPHeader               m_header;
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

    // NEW VIS members
    std::vector<dleaf_t>          m_leaves;
    std::vector<dnode_t>          m_nodes;
    std::vector<unsigned short>   m_leafFaces;   // leaf face index array
    std::vector<uint8_t>          m_visData;     // raw visibility lump bytes
    int                           m_visNumClusters = 0;
    // Per-cluster: byte offset into m_visData for the PVS bitset
    std::vector<int>              m_visPVSOffsets;
};

} // namespace veex