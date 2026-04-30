#pragma once
// veex/Face.h
// Face-centric BSP data structures for SDK2013 compatibility.
// Preserves Source Engine face structure and enables proper tangent calculation.

#include "veex/Common.h"
#include "veex/BSPParser.h"
#include <vector>
#include <glm/glm.hpp>

namespace veex {

// ── Face Material Properties ────────────────────────────────────────────────────
// Derived from VMT parsing and texinfo flags
struct FaceMaterialProperties {
    std::string shaderType;         // "LightmappedGeneric", "VertexLitGeneric", etc.
    bool hasNormalMap;              // $bumpmap
    bool hasSpecularMap;            // $specular
    bool hasDetailTexture;          // $detail
    bool hasEmissiveMap;            // $selfillum
    float detailScale;              // $detailscale
    float detailBlendFactor;        // $detailblendfactor
    int detailBlendMode;            // $detailblendmode (0=multiply, 1=add, 2=lerp)
    float bumpScale;                // $bumpscale (normal map intensity)
    float roughness;                // PBR roughness value
    float metallic;                 // PBR metallic value
    bool receivesShadows;           // !SURF_NOSHADOWS
    bool castsShadows;              // !SURF_NOSHADOWS
    bool receivesDecals;            // !SURF_NODECALS
    bool isTranslucent;             // SURF_TRANS
    bool isSky;                     // SURF_SKY or SURF_SKY2D
    bool isWater;                   // CONTENTS_WATER
    bool isWarp;                    // SURF_WARP
    bool isNoDraw;                  // SURF_NODRAW
    bool isHint;                    // SURF_HINT
};

// ── Face Information ────────────────────────────────────────────────────────────
// Core face data that preserves Source Engine structure
struct FaceInfo {
    int texinfoIndex;               // Index into BSP texinfo array
    int lightmapIndex;              // Index into lightmap atlas
    int dispinfoIndex;              // -1 if not displacement
    int surfaceFogVolumeID;         // For water surfaces
    uint32_t surfaceFlags;          // From texinfo flags
    uint32_t contentFlags;          // From leaf contents
    bool hasLightmap;               // Derived from lightmap data
    bool isDisplacement;            // Derived from dispinfo
    bool castsShadows;              // !SURF_NOSHADOWS
    bool receivesDecals;            // !SURF_NODECALS
    bool shouldRender;              // !SURF_NODRAW && !SURF_SKIP && !SURF_HINT
    
    // Face properties for rendering
    FaceMaterialType materialType;  // Standard, Sky, Water, Translucent, Warp, NoDraw, Hint
};

// ── Face Polygon ────────────────────────────────────────────────────────────────
// Preserves original face polygon structure for proper tangent calculation
struct FacePolygon {
    std::vector<glm::vec3> vertices;    // Original polygon vertices (in world space)
    FaceInfo info;                      // Face properties
    glm::vec3 normal;                   // Face normal (normalized)
    glm::vec3 planeNormal;              // Plane normal (from BSP plane)
    int leafIndex;                      // Which leaf this face belongs to
    int faceIndex;                      // Original face index in BSP
    
    // Texture coordinate generation
    glm::vec4 texVecs[2];               // Texture vectors from texinfo
    glm::vec4 lightmapVecs[2];          // Lightmap vectors from texinfo
    
    // Lightmap coordinate bounds
    glm::ivec2 lightmapMins;            // LightmapTextureMinsInLuxels
    glm::ivec2 lightmapSize;            // LightmapTextureSizeInLuxels
    
    // Displacement data (if applicable)
    struct DisplacementData {
        int power;                      // Displacement power (2^power + 1 vertices)
        std::vector<CDispVert> verts;   // Displacement vertices
        std::vector<CDispTri> tris;     // Displacement triangles
        CDispNeighbor edgeNeighbors[4]; // Edge neighbors
        CDispCornerNeighbors cornerNeighbors[4]; // Corner neighbors
    };
    
    DisplacementData* displacement;     // NULL if not displacement
    
    // Constructor
    FacePolygon() : displacement(nullptr), leafIndex(-1), faceIndex(-1) {}
    ~FacePolygon() { delete displacement; }
};

// ── Face Vertex ─────────────────────────────────────────────────────────────────
// Vertex data for rendering with proper tangent space
struct FaceVertex {
    glm::vec3 position;     // World position
    glm::vec3 normal;       // Face normal (for flat shading)
    glm::vec3 tangent;      // Tangent vector (normalized)
    glm::vec3 bitangent;    // Bitangent vector (normalized)
    glm::vec2 texCoord;     // Texture coordinates
    glm::vec2 lmCoord0;     // Lightmap UV for RNM basis 0
    glm::vec2 lmCoord1;     // Lightmap UV for RNM basis 1
    glm::vec2 lmCoord2;     // Lightmap UV for RNM basis 2
    glm::vec3 faceNormal;   // Face normal for RNM basis orientation
    
    // Tangent space matrix for normal mapping
    glm::mat3 tangentSpace;
    
    // Constructor
    FaceVertex() : position(0), normal(0, 0, 1), tangent(1, 0, 0), 
                   bitangent(0, 1, 0), texCoord(0), lmCoord0(-1), 
                   lmCoord1(-1), lmCoord2(-1), faceNormal(0, 0, 1) {}
};

// ── Face Batch ──────────────────────────────────────────────────────────────────
// Dynamic batch for rendering faces with similar properties
struct FaceBatch {
    std::vector<FaceVertex> vertices;   // Vertices for this batch
    std::vector<uint32_t> indices;      // Index buffer for this batch
    
    // Batch key for sorting
    struct BatchKey {
        uint32_t texinfoIndex;          // Primary material identifier
        uint32_t lightmapStyle;         // Lightmap style
        uint32_t albedoID;              // Texture ID
        uint32_t normalID;              // Normal map ID
        uint32_t specularID;            // Specular map ID
        uint32_t emissiveID;            // Emissive map ID
        uint32_t detailID;              // Detail texture ID
        uint32_t blendMode;             // Blend mode
        uint32_t cullMode;              // Cull mode
        bool hasLightmap;               // Has lightmap data
        bool isDisplacement;            // Is displacement face
        bool isWater;                   // Is water surface
        bool isSky;                     // Is sky surface
        uint32_t surfaceFlags;          // Surface flags
        uint32_t contentFlags;          // Content flags
        
        bool operator==(const BatchKey& other) const {
            return texinfoIndex == other.texinfoIndex &&
                   lightmapStyle == other.lightmapStyle &&
                   albedoID == other.albedoID &&
                   normalID == other.normalID &&
                   specularID == other.specularID &&
                   emissiveID == other.emissiveID &&
                   detailID == other.detailID &&
                   blendMode == other.blendMode &&
                   cullMode == other.cullMode &&
                   hasLightmap == other.hasLightmap &&
                   isDisplacement == other.isDisplacement &&
                   isWater == other.isWater &&
                   isSky == other.isSky &&
                   surfaceFlags == other.surfaceFlags &&
                   contentFlags == other.contentFlags;
        }
    } key;
    
    // Material parameters for shader
    MaterialParams matParams;
    
    // Statistics
    int faceCount;                      // Number of faces in this batch
    int vertexCount;                    // Number of vertices
    int indexCount;                     // Number of indices
    
    FaceBatch() : faceCount(0), vertexCount(0), indexCount(0) {}
};

// ── Tangent Space Calculator ────────────────────────────────────────────────────
// Proper tangent space calculation for Source Engine faces
class TangentSpaceCalculator {
public:
    // Calculate tangent space for a face polygon
    static void CalculateFaceTangentSpace(const FacePolygon& face, 
                                         std::vector<FaceVertex>& vertices);
    
    // Calculate tangent space for a triangle
    static void CalculateTriangleTangentSpace(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                             const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                             glm::vec3& tangent, glm::vec3& bitangent, glm::vec3& normal);
    
    // Orthonormalize tangent space (Gram-Schmidt)
    static void OrthonormalizeTangentSpace(glm::vec3& tangent, glm::vec3& bitangent, const glm::vec3& normal);
    
    // Generate tangent space matrix
    static glm::mat3 GenerateTangentSpaceMatrix(const glm::vec3& tangent, 
                                               const glm::vec3& bitangent, 
                                               const glm::vec3& normal);
    
private:
    // Helper to compute edge vectors
    static void ComputeEdgeVectors(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                  const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                  glm::vec3& edge1, glm::vec3& edge2,
                                  glm::vec2& deltaUV1, glm::vec2& deltaUV2);
};

// ── Face Processor ──────────────────────────────────────────────────────────────
// Processes BSP faces into renderable format
class FaceProcessor {
public:
    // Process all faces from BSP parser
    static bool ProcessFaces(const BSPParser& parser, 
                            std::vector<FacePolygon>& faces,
                            std::vector<FaceMaterialProperties>& materials);
    
    // Triangulate face polygon
    static void TriangulateFace(const FacePolygon& face, 
                               std::vector<FaceVertex>& vertices,
                               std::vector<uint32_t>& indices);
    
    // Generate batches from processed faces
    static void GenerateBatches(const std::vector<FacePolygon>& faces,
                               const std::vector<FaceMaterialProperties>& materials,
                               std::vector<FaceBatch>& batches);
    
    // Sort faces by render order (important for transparency)
    static void SortFaces(std::vector<FacePolygon>& faces, const glm::vec3& cameraPos);
    
private:
    // Helper to compute face normal
    static glm::vec3 ComputeFaceNormal(const std::vector<glm::vec3>& vertices);
    
    // Helper to compute texture coordinates
    static glm::vec2 ComputeTextureCoord(const glm::vec3& position, 
                                        const glm::vec4 texVecs[2]);
    
    // Helper to compute lightmap coordinates
    static glm::vec2 ComputeLightmapCoord(const glm::vec3& position,
                                         const glm::vec4 lightmapVecs[2],
                                         const glm::ivec2& mins,
                                         const glm::ivec2& size,
                                         uint32_t atlasWidth, uint32_t atlasHeight);
};

} // namespace veex