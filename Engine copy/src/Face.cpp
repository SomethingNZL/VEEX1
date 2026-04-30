// veex/Face.cpp
// Face-centric BSP processing and tangent space calculation.
// Implements SDK2013-compatible face handling with proper tangent math.

#include "veex/Face.h"
#include "veex/Logger.h"
#include "veex/BSPParser.h"
#include "veex/MaterialSystem.h"
#include "veex/VMTLoader.h"
#include <algorithm>
#include <cmath>

namespace veex {

// ── Tangent Space Calculator Implementation ──────────────────────────────────────

void TangentSpaceCalculator::CalculateFaceTangentSpace(const FacePolygon& face, 
                                                       std::vector<FaceVertex>& vertices)
{
    if (face.vertices.size() < 3) return;
    
    // For flat faces, we can use a consistent tangent space
    glm::vec3 faceNormal = face.normal;
    
    // Generate consistent tangent and bitangent vectors
    glm::vec3 tangent, bitangent;
    
    // Choose a vector that's not parallel to the normal
    glm::vec3 up = glm::abs(faceNormal.z) < 0.9f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    
    // Generate orthogonal basis
    tangent = glm::normalize(glm::cross(faceNormal, up));
    bitangent = glm::normalize(glm::cross(faceNormal, tangent));
    
    // For each vertex, set the tangent space
    for (auto& vertex : vertices) {
        vertex.normal = faceNormal;
        vertex.tangent = tangent;
        vertex.bitangent = bitangent;
        vertex.faceNormal = faceNormal;
        vertex.tangentSpace = GenerateTangentSpaceMatrix(tangent, bitangent, faceNormal);
    }
}

void TangentSpaceCalculator::CalculateTriangleTangentSpace(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                                           const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                                           glm::vec3& tangent, glm::vec3& bitangent, glm::vec3& normal)
{
    // Compute edge vectors
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    
    glm::vec2 deltaUV1 = uv1 - uv0;
    glm::vec2 deltaUV2 = uv2 - uv0;
    
    // Calculate tangent and bitangent
    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    
    tangent = glm::normalize((edge1 * deltaUV2.y - edge2 * deltaUV1.y) * f);
    bitangent = glm::normalize((edge2 * deltaUV1.x - edge1 * deltaUV2.x) * f);
    
    // Calculate normal
    normal = glm::normalize(glm::cross(edge1, edge2));
    
    // Ensure tangent space is right-handed
    if (glm::dot(glm::cross(tangent, bitangent), normal) < 0.0f) {
        bitangent = -bitangent;
    }
    
    // Orthonormalize
    OrthonormalizeTangentSpace(tangent, bitangent, normal);
}

void TangentSpaceCalculator::OrthonormalizeTangentSpace(glm::vec3& tangent, glm::vec3& bitangent, const glm::vec3& normal)
{
    // Gram-Schmidt orthogonalization
    tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
    bitangent = glm::normalize(bitangent - normal * glm::dot(normal, bitangent) - tangent * glm::dot(tangent, bitangent));
}

glm::mat3 TangentSpaceCalculator::GenerateTangentSpaceMatrix(const glm::vec3& tangent, 
                                                           const glm::vec3& bitangent, 
                                                           const glm::vec3& normal)
{
    return glm::mat3(tangent, bitangent, normal);
}

void TangentSpaceCalculator::ComputeEdgeVectors(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                               const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2,
                                               glm::vec3& edge1, glm::vec3& edge2,
                                               glm::vec2& deltaUV1, glm::vec2& deltaUV2)
{
    edge1 = v1 - v0;
    edge2 = v2 - v0;
    deltaUV1 = uv1 - uv0;
    deltaUV2 = uv2 - uv0;
}

// ── Face Processor Implementation ───────────────────────────────────────────────

bool FaceProcessor::ProcessFaces(const BSPParser& parser, 
                                std::vector<FacePolygon>& faces,
                                std::vector<FaceMaterialProperties>& materials)
{
    const auto& bspFaces = parser.GetFaces();
    const auto& texinfo = parser.GetTexinfo();
    const auto& planes = parser.GetPlanes();
    const auto& lightmapInfo = parser.GetFaceLightmapInfo();
    const auto& leaves = parser.GetLeaves();
    
    faces.clear();
    materials.clear();
    
    // Resize materials array to match texinfo count
    materials.resize(texinfo.size());
    
    // Process each face
    for (int i = 0; i < (int)bspFaces.size(); ++i) {
        const dface_t& bspFace = bspFaces[i];
        
        // Skip faces that shouldn't be rendered
        if (!parser.ShouldRenderFace(i)) continue;
        
        // Get face info
        FaceInfo faceInfo = parser.GetFaceInfo(i);
        if (!faceInfo.shouldRender) continue;
        
        // Create face polygon
        FacePolygon face;
        face.info = faceInfo;
        face.faceIndex = i;
        face.texinfoIndex = bspFace.texinfo;
        
        // Get vertices for this face
        parser.GetFaceVertices(bspFace, face.vertices);
        if (face.vertices.size() < 3) continue;
        
        // Get plane normal
        if (bspFace.planenum >= 0 && bspFace.planenum < (int)planes.size()) {
            const dplane_t& plane = planes[bspFace.planenum];
            face.planeNormal = glm::vec3(plane.normal.x, plane.normal.y, plane.normal.z);
            if (bspFace.side != 0) face.planeNormal = -face.planeNormal;
        }
        
        // Compute face normal
        face.normal = ComputeFaceNormal(face.vertices);
        
        // Get texinfo data
        if (bspFace.texinfo >= 0 && bspFace.texinfo < (int)texinfo.size()) {
            const texinfo_t& ti = texinfo[bspFace.texinfo];
            face.texVecs[0] = glm::vec4(ti.textureVecs[0][0], ti.textureVecs[0][1], ti.textureVecs[0][2], ti.textureVecs[0][3]);
            face.texVecs[1] = glm::vec4(ti.textureVecs[1][0], ti.textureVecs[1][1], ti.textureVecs[1][2], ti.textureVecs[1][3]);
            face.lightmapVecs[0] = glm::vec4(ti.lightmapVecs[0][0], ti.lightmapVecs[0][1], ti.lightmapVecs[0][2], ti.lightmapVecs[0][3]);
            face.lightmapVecs[1] = glm::vec4(ti.lightmapVecs[1][0], ti.lightmapVecs[1][1], ti.lightmapVecs[1][2], ti.lightmapVecs[1][3]);
        }
        
        // Get lightmap info
        if (i < (int)lightmapInfo.size() && lightmapInfo[i].valid) {
            face.lightmapMins = glm::ivec2(bspFace.LightmapTextureMinsInLuxels[0], bspFace.LightmapTextureMinsInLuxels[1]);
            face.lightmapSize = glm::ivec2(bspFace.LightmapTextureSizeInLuxels[0], bspFace.LightmapTextureSizeInLuxels[1]);
            face.lightmapIndex = i;
            face.hasLightmap = true;
        } else {
            face.hasLightmap = false;
        }
        
        // Find which leaf this face belongs to (simplified approach)
        // In a real implementation, this would use the BSP tree traversal
        face.leafIndex = -1;
        
        // Check for displacement
        if (bspFace.dispinfo >= 0) {
            face.info.isDisplacement = true;
            // Displacement data would be loaded here
        }
        
        faces.push_back(face);
    }
    
    Logger::Info("[FaceProcessor] Processed " + std::to_string(faces.size()) + " faces.");
    return !faces.empty();
}

void FaceProcessor::TriangulateFace(const FacePolygon& face, 
                                   std::vector<FaceVertex>& vertices,
                                   std::vector<uint32_t>& indices)
{
    if (face.vertices.size() < 3) return;
    
    // For convex polygons, use simple fan triangulation
    // For more complex polygons, a proper triangulation algorithm would be needed
    
    int baseVertexIndex = (int)vertices.size();
    
    // Create vertices for the face
    for (size_t i = 0; i < face.vertices.size(); ++i) {
        FaceVertex vertex;
        vertex.position = face.vertices[i];
        vertex.normal = face.normal;
        vertex.faceNormal = face.normal;
        
        // Compute texture coordinates
        vertex.texCoord = ComputeTextureCoord(vertex.position, face.texVecs);
        
        // Compute lightmap coordinates if available
        if (face.hasLightmap) {
            uint32_t atlasWidth = 4096;  // Would come from lightmap atlas
            uint32_t atlasHeight = 4096;
            vertex.lmCoord0 = ComputeLightmapCoord(vertex.position, face.lightmapVecs, 
                                                  face.lightmapMins, face.lightmapSize,
                                                  atlasWidth, atlasHeight);
            vertex.lmCoord1 = vertex.lmCoord0;
            vertex.lmCoord2 = vertex.lmCoord0;
        } else {
            vertex.lmCoord0 = glm::vec2(-1.0f);
            vertex.lmCoord1 = glm::vec2(-1.0f);
            vertex.lmCoord2 = glm::vec2(-1.0f);
        }
        
        vertices.push_back(vertex);
    }
    
    // Create triangle indices (fan triangulation)
    for (size_t i = 1; i < face.vertices.size() - 1; ++i) {
        indices.push_back(baseVertexIndex);
        indices.push_back(baseVertexIndex + i);
        indices.push_back(baseVertexIndex + i + 1);
    }
}

void FaceProcessor::GenerateBatches(const std::vector<FacePolygon>& faces,
                                   const std::vector<FaceMaterialProperties>& materials,
                                   std::vector<FaceBatch>& batches)
{
    batches.clear();
    
    // Group faces by material properties for efficient batching
    std::unordered_map<FaceBatch::BatchKey, std::vector<FacePolygon>, BatchKeyHash> faceGroups;
    
    for (const auto& face : faces) {
        FaceBatch::BatchKey key;
        key.texinfoIndex = face.texinfoIndex;
        key.lightmapStyle = 0;  // Simplified for now
        key.surfaceFlags = face.info.surfaceFlags;
        key.contentFlags = face.info.contentFlags;
        key.hasLightmap = face.hasLightmap;
        key.isDisplacement = face.info.isDisplacement;
        key.isWater = face.info.isWater;
        key.isSky = face.info.isSky;
        
        // Set material properties based on face info
        if (face.texinfoIndex >= 0 && face.texinfoIndex < (int)materials.size()) {
            const auto& mat = materials[face.texinfoIndex];
            key.albedoID = mat.hasNormalMap ? 1 : 0;  // Simplified texture ID assignment
            key.normalID = mat.hasNormalMap ? 1 : 0;
            key.specularID = mat.hasSpecularMap ? 1 : 0;
            key.emissiveID = mat.hasEmissiveMap ? 1 : 0;
            key.detailID = mat.hasDetailTexture ? 1 : 0;
            key.blendMode = mat.isTranslucent ? 1 : 0;  // GL_BLEND
            key.cullMode = 0;  // GL_BACK
        }
        
        faceGroups[key].push_back(face);
    }
    
    // Create batches from grouped faces
    for (auto& [key, faceGroup] : faceGroups) {
        FaceBatch batch;
        batch.key = key;
        
        // Process faces in this group
        for (const auto& face : faceGroup) {
            std::vector<FaceVertex> faceVertices;
            std::vector<uint32_t> faceIndices;
            
            TriangulateFace(face, faceVertices, faceIndices);
            
            // Calculate tangent space for this face
            TangentSpaceCalculator::CalculateFaceTangentSpace(face, faceVertices);
            
            // Add vertices and indices to batch
            int baseIndex = (int)batch.vertices.size();
            batch.vertices.insert(batch.vertices.end(), faceVertices.begin(), faceVertices.end());
            
            for (uint32_t index : faceIndices) {
                batch.indices.push_back(baseIndex + index);
            }
            
            batch.faceCount++;
        }
        
        batch.vertexCount = (int)batch.vertices.size();
        batch.indexCount = (int)batch.indices.size();
        
        // Set material parameters
        if (key.texinfoIndex >= 0 && key.texinfoIndex < (int)materials.size()) {
            const auto& mat = materials[key.texinfoIndex];
            batch.matParams.roughness = mat.roughness;
            batch.matParams.metallic = mat.metallic;
            batch.matParams.hasNormalMap = mat.hasNormalMap;
            batch.matParams.hasRoughnessMap = mat.hasSpecularMap;
            batch.matParams.hasMetallicMap = false;  // Simplified
            batch.matParams.hasEmissiveMap = mat.hasEmissiveMap;
            batch.matParams.hasDetail = mat.hasDetailTexture;
            batch.matParams.detailScale = mat.detailScale;
            batch.matParams.detailBlendFactor = mat.detailBlendFactor;
            batch.matParams.detailBlendMode = mat.detailBlendMode;
            batch.matParams.bumpScale = mat.bumpScale;
        }
        
        batches.push_back(batch);
    }
    
    Logger::Info("[FaceProcessor] Generated " + std::to_string(batches.size()) + " batches from " + 
                 std::to_string(faces.size()) + " faces.");
}

void FaceProcessor::SortFaces(std::vector<FacePolygon>& faces, const glm::vec3& cameraPos)
{
    // Sort faces by distance for proper transparency rendering
    std::sort(faces.begin(), faces.end(), [&cameraPos](const FacePolygon& a, const FacePolygon& b) {
        glm::vec3 aCenter = glm::vec3(0);
        glm::vec3 bCenter = glm::vec3(0);
        
        for (const auto& v : a.vertices) aCenter += v;
        for (const auto& v : b.vertices) bCenter += v;
        
        aCenter /= (float)a.vertices.size();
        bCenter /= (float)b.vertices.size();
        
        float distA = glm::distance2(cameraPos, aCenter);
        float distB = glm::distance2(cameraPos, bCenter);
        
        return distA > distB;  // Sort back-to-front for transparency
    });
}

glm::vec3 FaceProcessor::ComputeFaceNormal(const std::vector<glm::vec3>& vertices)
{
    if (vertices.size() < 3) return glm::vec3(0, 0, 1);
    
    glm::vec3 normal = glm::vec3(0);
    
    // Calculate normal using Newell's method for robustness
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t j = (i + 1) % vertices.size();
        const glm::vec3& v1 = vertices[i];
        const glm::vec3& v2 = vertices[j];
        
        normal.x += (v1.y - v2.y) * (v1.z + v2.z);
        normal.y += (v1.z - v2.z) * (v1.x + v2.x);
        normal.z += (v1.x - v2.x) * (v1.y + v2.y);
    }
    
    return glm::normalize(normal);
}

glm::vec2 FaceProcessor::ComputeTextureCoord(const glm::vec3& position, 
                                           const glm::vec4 texVecs[2])
{
    glm::vec2 uv;
    uv.x = glm::dot(glm::vec3(texVecs[0].x, texVecs[0].y, texVecs[0].z), position) + texVecs[0].w;
    uv.y = glm::dot(glm::vec3(texVecs[1].x, texVecs[1].y, texVecs[1].z), position) + texVecs[1].w;
    return uv;
}

glm::vec2 FaceProcessor::ComputeLightmapCoord(const glm::vec3& position,
                                            const glm::vec4 lightmapVecs[2],
                                            const glm::ivec2& mins,
                                            const glm::ivec2& size,
                                            uint32_t atlasWidth, uint32_t atlasHeight)
{
    float u = glm::dot(glm::vec3(lightmapVecs[0].x, lightmapVecs[0].y, lightmapVecs[0].z), position) + lightmapVecs[0].w;
    float v = glm::dot(glm::vec3(lightmapVecs[1].x, lightmapVecs[1].y, lightmapVecs[1].z), position) + lightmapVecs[1].w;
    
    // Convert to luxel coordinates
    u -= (float)mins.x;
    v -= (float)mins.y;
    
    // Normalize to [0,1] and scale by atlas size
    float atlasU = (u + 0.5f) / (float)size.x;
    float atlasV = (v + 0.5f) / (float)size.y;
    
    // Convert to atlas coordinates
    return glm::vec2(atlasU / (float)atlasWidth, atlasV / (float)atlasHeight);
}

// Hash function for BatchKey
struct BatchKeyHash {
    std::size_t operator()(const FaceBatch::BatchKey& key) const {
        std::size_t h = 0;
        auto hash_combine = [&h](std::size_t value) {
            h ^= value + 0x9e3779b9 + (h << 6) + (h >> 2);
        };
        
        hash_combine(std::hash<uint32_t>{}(key.texinfoIndex));
        hash_combine(std::hash<uint32_t>{}(key.lightmapStyle));
        hash_combine(std::hash<uint32_t>{}(key.albedoID));
        hash_combine(std::hash<uint32_t>{}(key.normalID));
        hash_combine(std::hash<uint32_t>{}(key.specularID));
        hash_combine(std::hash<uint32_t>{}(key.emissiveID));
        hash_combine(std::hash<uint32_t>{}(key.detailID));
        hash_combine(std::hash<uint32_t>{}(key.blendMode));
        hash_combine(std::hash<uint32_t>{}(key.cullMode));
        hash_combine(std::hash<bool>{}(key.hasLightmap));
        hash_combine(std::hash<bool>{}(key.isDisplacement));
        hash_combine(std::hash<bool>{}(key.isWater));
        hash_combine(std::hash<bool>{}(key.isSky));
        hash_combine(std::hash<uint32_t>{}(key.surfaceFlags));
        hash_combine(std::hash<uint32_t>{}(key.contentFlags));
        
        return h;
    }
};

} // namespace veex