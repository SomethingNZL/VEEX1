#include "veex/MDL.h"
#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace veex {

// ============================================================================
// Helper: Change file extension
// ============================================================================
static std::string ChangeExtension(const std::string& path, const std::string& newExt) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        return path.substr(0, dotPos) + newExt;
    }
    return path + newExt;
}

// ============================================================================
// MDLModel Implementation
// ============================================================================

MDLModel::MDLModel()
    : m_mass(0.0f)
    , m_flags(0)
    , m_checksum(0)
    , m_version(0)
    , m_eyePosition(0.0f)
    , m_bbMin(0.0f)
    , m_bbMax(0.0f)
    , m_hullMin(0.0f)
    , m_hullMax(0.0f) {
    std::memset(&m_header, 0, sizeof(m_header));
}

MDLModel::~MDLModel() = default;

bool MDLModel::LoadFromFile(const std::string& path, const GameInfo& game) {
    // Load MDL file
    if (!ReadFile(path, game, m_mdlData)) {
        Logger::Error("MDLModel: Failed to read MDL file: " + path);
        return false;
    }

    // Load VVD file (vertex data)
    std::string vvdPath = ChangeExtension(path, ".vvd");
    if (!ReadFile(vvdPath, game, m_vvdData)) {
        Logger::Warn("MDLModel: Failed to read VVD file: " + vvdPath);
        // Some models may not have VVD (very rare), but we can't render without it
    }

    // Load VTX file (index/strip data)
    // VTX files have platform-specific prefixes in some versions
    // Try common variants: .dx80.vtx, .dx90.vtx, .sw.vtx, or just .vtx
    std::string vtxPath = ChangeExtension(path, ".dx90.vtx");
    if (!ReadFile(vtxPath, game, m_vtxData)) {
        vtxPath = ChangeExtension(path, ".dx80.vtx");
        if (!ReadFile(vtxPath, game, m_vtxData)) {
            vtxPath = ChangeExtension(path, ".sw.vtx");
            if (!ReadFile(vtxPath, game, m_vtxData)) {
                vtxPath = ChangeExtension(path, ".vtx");
                if (!ReadFile(vtxPath, game, m_vtxData)) {
                    Logger::Warn("MDLModel: Failed to read VTX file for: " + path);
                }
            }
        }
    }

    return LoadFromBuffer(m_mdlData);
}

bool MDLModel::LoadFromBuffer(const std::vector<char>& buffer) {
    if (buffer.empty()) {
        Logger::Error("MDLModel: Empty buffer");
        return false;
    }

    m_mdlData = buffer;
    m_baseData = reinterpret_cast<const uint8_t*>(m_mdlData.data());
    size_t size = m_mdlData.size();

    // Parse MDL data
    if (!ParseMDL(m_baseData, size)) {
        return false;
    }

    // Parse VVD data if available
    if (!m_vvdData.empty()) {
        if (!ParseVVD(m_vvdData)) {
            Logger::Warn("MDLModel: Failed to parse VVD data");
        }
    }

    // Parse VTX data if available
    if (!m_vtxData.empty()) {
        if (!ParseVTX(m_vtxData)) {
            Logger::Warn("MDLModel: Failed to parse VTX data");
        }
    }

    // Build combined geometry
    if (!BuildGeometry()) {
        Logger::Warn("MDLModel: Failed to build geometry");
    }

    Logger::Info("MDLModel: Loaded '" + m_name + "' (v" + std::to_string(m_version) +
                 ") - " + std::to_string(m_vertices.size()) + " vertices, " +
                 std::to_string(m_indices.size()) + " indices, " +
                 std::to_string(m_bones.size()) + " bones, " +
                 std::to_string(m_materialNames.size()) + " materials, " +
                 std::to_string(m_meshes.size()) + " meshes");

    return true;
}

// ============================================================================
// MDL Parsing
// ============================================================================

bool MDLModel::ParseMDL(const uint8_t* data, size_t size) {
    // Parse main header
    if (!ParseHeader(data, size)) {
        return false;
    }

    // Parse secondary header if present
    if (m_header.studiohdr2index > 0) {
        ParseStudioHdr2(data, size);
    }

    // Parse bones
    if (!ParseBones(data, size)) {
        Logger::Warn("MDLModel: Failed to parse bones");
    }

    // Parse textures/materials
    if (!ParseTextures(data, size)) {
        Logger::Warn("MDLModel: Failed to parse textures");
    }

    // Parse texture directories
    ParseTextureDirs(data, size);

    // Parse skin families
    ParseSkinFamilies(data, size);

    // Parse body parts (mesh metadata)
    if (!ParseBodyParts(data, size)) {
        Logger::Warn("MDLModel: Failed to parse body parts");
    }

    // Parse animation sequences
    ParseSequences(data, size);

    // Parse hitbox sets
    ParseHitboxSets(data, size);

    // Parse surface property
    ParseSurfaceProp(data, size);

    // Parse attachments
    ParseAttachments(data, size);

    return true;
}

bool MDLModel::ParseHeader(const uint8_t* data, size_t size) {
    if (size < sizeof(studiohdr_t)) {
        Logger::Error("MDLModel: File too small for header (" + std::to_string(size) +
                      " < " + std::to_string(sizeof(studiohdr_t)) + ")");
        return false;
    }

    std::memcpy(&m_header, data, sizeof(studiohdr_t));

    // Check magic number - "IDST" = 0x54534449 in little-endian
    if (m_header.id != MDL_IDST) {
        // Also try big-endian just in case
        if (m_header.id == 0x49445354) {
            Logger::Warn("MDLModel: File has big-endian ID (might need byte-swapping)");
        } else {
            Logger::Error("MDLModel: Invalid file ID 0x" +
                          std::to_string(m_header.id) +
                          " (expected IDST = 0x" + std::to_string(MDL_IDST) + ")");
            return false;
        }
    }

    // Check version
    m_version = m_header.version;
    if (m_version < MDL_VERSION_44 || m_version > MDL_VERSION_49) {
        Logger::Warn("MDLModel: Unexpected version " + std::to_string(m_version) +
                     " (expected " + std::to_string(MDL_VERSION_44) + "-" +
                     std::to_string(MDL_VERSION_49) + ")");
    }

    // Store checksum for VVD/VTX validation
    m_checksum = m_header.checksum;

    // Extract model name
    m_name = std::string(m_header.name, strnlen(m_header.name, 64));

    // Extract basic properties
    m_mass = m_header.mass;
    m_flags = m_header.flags;
    m_eyePosition = m_header.eyeposition;
    m_bbMin = m_header.view_bbmin;
    m_bbMax = m_header.view_bbmax;
    m_hullMin = m_header.hull_min;
    m_hullMax = m_header.hull_max;

    Logger::Info("MDLModel: Header parsed - name='" + m_name +
                 "', version=" + std::to_string(m_version) +
                 ", checksum=" + std::to_string(m_checksum) +
                 ", bones=" + std::to_string(m_header.bone_count) +
                 ", textures=" + std::to_string(m_header.texture_count) +
                 ", bodyparts=" + std::to_string(m_header.bodypart_count) +
                 ", sequences=" + std::to_string(m_header.localseq_count));

    return true;
}

bool MDLModel::ParseStudioHdr2(const uint8_t* data, size_t size) {
    if (!BoundsCheck(m_header.studiohdr2index, sizeof(studiohdr2_t), size)) {
        Logger::Warn("MDLModel: studiohdr2_t offset out of bounds");
        return false;
    }

    m_header2 = std::make_unique<studiohdr2_t>();
    std::memcpy(m_header2.get(), data + m_header.studiohdr2index, sizeof(studiohdr2_t));

    Logger::Info("MDLModel: Parsed studiohdr2_t (maxEyeDeflection=" +
                 std::to_string(m_header2->flMaxEyeDeflection) + ")");
    return true;
}

bool MDLModel::ParseBones(const uint8_t* data, size_t size) {
    if (m_header.bone_count <= 0) {
        return true; // Some models have no bones
    }

    if (!BoundsCheck(m_header.bone_offset, m_header.bone_count * sizeof(mstudiobone_t), size)) {
        Logger::Error("MDLModel: Bone data out of bounds");
        return false;
    }

    m_bones.reserve(m_header.bone_count);

    for (int32_t i = 0; i < m_header.bone_count; ++i) {
        size_t boneOffset = m_header.bone_offset + i * sizeof(mstudiobone_t);
        if (boneOffset + sizeof(mstudiobone_t) > size) {
            Logger::Error("MDLModel: Bone " + std::to_string(i) + " out of bounds");
            continue;
        }

        mstudiobone_t boneHeader;
        std::memcpy(&boneHeader, data + boneOffset, sizeof(mstudiobone_t));

        Bone bone;
        bone.name = ResolveString(data, boneOffset + boneHeader.sznameindex);
        bone.parent = boneHeader.parent;
        bone.position = boneHeader.pos;
        bone.rotation = boneHeader.quat;
        bone.flags = boneHeader.flags;

        // Convert pose-to-bone 3x4 matrix to 4x4
        // poseToBone is stored as 3 rows of 4 floats (mat4x3 in column-major from glm)
        // Actually glm::mat4x3 is 4 columns, 3 rows
        bone.poseToBone = glm::mat4(1.0f);
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 3; ++row) {
                bone.poseToBone[col][row] = boneHeader.poseToBone[col][row];
            }
        }

        m_bones.push_back(bone);
    }

    return true;
}

bool MDLModel::ParseTextures(const uint8_t* data, size_t size) {
    if (m_header.texture_count <= 0) {
        return true;
    }

    if (!BoundsCheck(m_header.texture_offset, m_header.texture_count * sizeof(mstudiotexture_t), size)) {
        Logger::Error("MDLModel: Texture data out of bounds");
        return false;
    }

    m_materialNames.reserve(m_header.texture_count);

    for (int32_t i = 0; i < m_header.texture_count; ++i) {
        size_t textureOffset = m_header.texture_offset + i * sizeof(mstudiotexture_t);
        if (textureOffset + sizeof(mstudiotexture_t) > size) {
            Logger::Error("MDLModel: Texture " + std::to_string(i) + " out of bounds");
            continue;
        }

        mstudiotexture_t textureHeader;
        std::memcpy(&textureHeader, data + textureOffset, sizeof(mstudiotexture_t));

        // name_offset is relative to the start of this texture structure
        std::string materialName = ResolveString(data, static_cast<int32_t>(textureOffset + textureHeader.name_offset));
        m_materialNames.push_back(materialName);
    }

    return true;
}

bool MDLModel::ParseTextureDirs(const uint8_t* data, size_t size) {
    if (m_header.texturedir_count <= 0) {
        return true;
    }

    if (!BoundsCheck(m_header.texturedir_offset, m_header.texturedir_count * sizeof(int32_t), size)) {
        return false;
    }

    m_textureDirs.reserve(m_header.texturedir_count);

    for (int32_t i = 0; i < m_header.texturedir_count; ++i) {
        int32_t dirOffset;
        size_t offset = m_header.texturedir_offset + i * sizeof(int32_t);
        if (offset + sizeof(int32_t) > size) continue;

        std::memcpy(&dirOffset, data + offset, sizeof(int32_t));
        std::string dirPath = ResolveString(data, dirOffset);
        if (!dirPath.empty()) {
            m_textureDirs.push_back(dirPath);
        }
    }

    return true;
}

bool MDLModel::ParseSkinFamilies(const uint8_t* data, size_t size) {
    if (m_header.skinfamily_count <= 0 || m_header.skinreference_count <= 0) {
        return true;
    }

    int32_t tableSize = m_header.skinfamily_count * m_header.skinreference_count * sizeof(int16_t);
    if (!BoundsCheck(m_header.skinreference_index, tableSize, size)) {
        Logger::Warn("MDLModel: Skin replacement table out of bounds");
        return false;
    }

    m_skinFamilies.reserve(m_header.skinfamily_count);

    for (int32_t i = 0; i < m_header.skinfamily_count; ++i) {
        SkinFamily family;
        family.textures.reserve(m_header.skinreference_count);

        for (int32_t j = 0; j < m_header.skinreference_count; ++j) {
            size_t offset = m_header.skinreference_index +
                           (i * m_header.skinreference_count + j) * sizeof(int16_t);
            if (offset + sizeof(int16_t) > size) {
                family.textures.push_back(0);
                continue;
            }

            int16_t textureIndex;
            std::memcpy(&textureIndex, data + offset, sizeof(int16_t));
            family.textures.push_back(textureIndex);
        }

        m_skinFamilies.push_back(family);
    }

    return true;
}

bool MDLModel::ParseBodyParts(const uint8_t* data, size_t size) {
    if (m_header.bodypart_count <= 0) {
        return true;
    }

    if (!BoundsCheck(m_header.bodypart_offset, m_header.bodypart_count * sizeof(mstudiobodyparts_t), size)) {
        Logger::Error("MDLModel: Body part data out of bounds");
        return false;
    }

    // Store body part metadata - actual geometry comes from VVD/VTX
    for (int32_t i = 0; i < m_header.bodypart_count; ++i) {
        size_t bodyPartOffset = m_header.bodypart_offset + i * sizeof(mstudiobodyparts_t);
        if (bodyPartOffset + sizeof(mstudiobodyparts_t) > size) {
            Logger::Error("MDLModel: Body part " + std::to_string(i) + " out of bounds");
            continue;
        }

        mstudiobodyparts_t bodyPart;
        std::memcpy(&bodyPart, data + bodyPartOffset, sizeof(mstudiobodyparts_t));

        // Parse models within this body part
        for (int32_t j = 0; j < bodyPart.nummodels; ++j) {
            size_t modelOffset = bodyPart.modelindex + j * sizeof(mstudiomodel_t);
            if (modelOffset + sizeof(mstudiomodel_t) > size) {
                Logger::Error("MDLModel: Model " + std::to_string(j) + " out of bounds");
                continue;
            }

            mstudiomodel_t modelHeader;
            std::memcpy(&modelHeader, data + modelOffset, sizeof(mstudiomodel_t));

            // Parse meshes within this model
            for (int32_t k = 0; k < modelHeader.nummeshes; ++k) {
                size_t meshOffset = modelHeader.meshindex + k * sizeof(mstudiomesh_t);
                if (meshOffset + sizeof(mstudiomesh_t) > size) {
                    Logger::Error("MDLModel: Mesh " + std::to_string(k) + " out of bounds");
                    continue;
                }

                mstudiomesh_t meshHeader;
                std::memcpy(&meshHeader, data + meshOffset, sizeof(mstudiomesh_t));

                // Create mesh data entry (geometry will be filled from VVD/VTX)
                MeshData meshData;
                meshData.materialIndex = meshHeader.material;
                meshData.bodyPart = i;
                meshData.model = j;
                meshData.mesh = k;
                m_meshes.push_back(meshData);
            }
        }
    }

    return true;
}

bool MDLModel::ParseSequences(const uint8_t* data, size_t size) {
    if (m_header.localseq_count <= 0) {
        return true;
    }

    if (!BoundsCheck(m_header.localseq_offset, m_header.localseq_count * sizeof(mstudioseqdesc_t), size)) {
        Logger::Warn("MDLModel: Sequence data out of bounds");
        return false;
    }

    m_sequences.reserve(m_header.localseq_count);

    for (int32_t i = 0; i < m_header.localseq_count; ++i) {
        size_t seqOffset = m_header.localseq_offset + i * sizeof(mstudioseqdesc_t);
        if (seqOffset + sizeof(mstudioseqdesc_t) > size) {
            Logger::Error("MDLModel: Sequence " + std::to_string(i) + " out of bounds");
            continue;
        }

        mstudioseqdesc_t seqHeader;
        std::memcpy(&seqHeader, data + seqOffset, sizeof(mstudioseqdesc_t));

        AnimationSequence seq;
        seq.name = ResolveString(data, static_cast<int32_t>(seqOffset + seqHeader.szlabelindex));
        seq.activityName = ResolveString(data, static_cast<int32_t>(seqOffset + seqHeader.szactivitynameindex));
        seq.activity = seqHeader.activity;
        seq.flags = seqHeader.flags;
        seq.numFrames = 0; // Will be filled from animdesc if available
        seq.frameRate = 30.0f; // Default
        seq.numBlends = seqHeader.numblends;
        seq.animIndex = seqHeader.animindexindex;

        // Try to get frame count from first animation
        if (seqHeader.animindexindex >= 0 && m_header.localanim_count > 0) {
            // The animindexindex points to an array of anim indices
            // For now, just use defaults
        }

        m_sequences.push_back(seq);
    }

    // Now parse animation descriptions to get frame counts
    if (m_header.localanim_count > 0 &&
        BoundsCheck(m_header.localanim_offset, m_header.localanim_count * sizeof(mstudioanimdesc_t), size)) {
        for (int32_t i = 0; i < m_header.localanim_count && i < static_cast<int32_t>(m_sequences.size()); ++i) {
            size_t animOffset = m_header.localanim_offset + i * sizeof(mstudioanimdesc_t);
            if (animOffset + sizeof(mstudioanimdesc_t) > size) continue;

            mstudioanimdesc_t animHeader;
            std::memcpy(&animHeader, data + animOffset, sizeof(mstudioanimdesc_t));

            if (i < static_cast<int32_t>(m_sequences.size())) {
                m_sequences[i].numFrames = animHeader.numframes;
                m_sequences[i].frameRate = animHeader.fps;
            }
        }
    }

    return true;
}

bool MDLModel::ParseHitboxSets(const uint8_t* data, size_t size) {
    if (m_header.hitbox_count <= 0) {
        return true;
    }

    if (!BoundsCheck(m_header.hitbox_offset, m_header.hitbox_count * sizeof(mstudiohitboxset_t), size)) {
        return false;
    }

    // Parse hitbox sets (metadata - could store if needed)
    for (int32_t i = 0; i < m_header.hitbox_count; ++i) {
        size_t setOffset = m_header.hitbox_offset + i * sizeof(mstudiohitboxset_t);
        if (setOffset + sizeof(mstudiohitboxset_t) > size) continue;

        mstudiohitboxset_t hitboxSet;
        std::memcpy(&hitboxSet, data + setOffset, sizeof(mstudiohitboxset_t));

        std::string setName = ResolveString(data, static_cast<int32_t>(setOffset + hitboxSet.sznameindex));

        // Parse individual hitboxes
        for (int32_t j = 0; j < hitboxSet.numhitboxes; ++j) {
            size_t hitboxOffset = hitboxSet.hitboxindex + j * sizeof(mstudiobbox_t);
            if (hitboxOffset + sizeof(mstudiobbox_t) > size) continue;

            mstudiobbox_t bbox;
            std::memcpy(&bbox, data + hitboxOffset, sizeof(mstudiobbox_t));
            // Could store hitbox data if needed for physics/tracing
        }
    }

    return true;
}

bool MDLModel::ParseSurfaceProp(const uint8_t* data, size_t size) {
    if (m_header.surfaceprop_index > 0 && static_cast<size_t>(m_header.surfaceprop_index) < size) {
        m_surfaceProp = ResolveString(data, m_header.surfaceprop_index);
    }
    return true;
}

bool MDLModel::ParseAttachments(const uint8_t* data, size_t size) {
    if (m_header.attachment_count <= 0) {
        return true;
    }

    // Attachments are stored as mstudioattachment_t structures
    // For now, we just acknowledge they exist
    // Full implementation would parse attachment names and transforms

    return true;
}

// ============================================================================
// VVD (Vertex Data) Parsing
// ============================================================================

bool MDLModel::ParseVVD(const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(vertexFileHeader_t)) {
        Logger::Error("MDLModel: VVD file too small for header");
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
    size_t size = buffer.size();

    vertexFileHeader_t header;
    std::memcpy(&header, data, sizeof(vertexFileHeader_t));

    // Validate VVD header
    if (header.id != VVD_IDSV) {
        Logger::Error("MDLModel: Invalid VVD magic number");
        return false;
    }

    if (header.version != VVD_VERSION) {
        Logger::Warn("MDLModel: Unexpected VVD version " + std::to_string(header.version));
    }

    if (header.checksum != m_checksum) {
        Logger::Warn("MDLModel: VVD checksum mismatch (MDL=" + std::to_string(m_checksum) +
                     ", VVD=" + std::to_string(header.checksum) + ")");
    }

    // Read vertex data
    size_t vertexDataStart = sizeof(vertexFileHeader_t);
    size_t vertexDataSize = header.vertexDataSize;

    if (vertexDataStart + vertexDataSize > size) {
        Logger::Error("MDLModel: VVD vertex data out of bounds");
        return false;
    }

    // Calculate number of vertices
    int32_t numVertices = vertexDataSize / sizeof(mstudiovertex_t);
    if (numVertices > 65536) {
        Logger::Warn("MDLModel: Suspicious VVD vertex count: " + std::to_string(numVertices));
    }

    m_vertices.clear();
    m_vertices.reserve(numVertices);

    for (int32_t i = 0; i < numVertices; ++i) {
        size_t vertOffset = vertexDataStart + i * sizeof(mstudiovertex_t);
        if (vertOffset + sizeof(mstudiovertex_t) > size) break;

        mstudiovertex_t vert;
        std::memcpy(&vert, data + vertOffset, sizeof(mstudiovertex_t));

        MDLVertex runtimeVert;
        runtimeVert.position = vert.m_vecPosition;
        runtimeVert.normal = vert.m_vecNormal;
        runtimeVert.texCoord = vert.m_vecTexCoord;
        runtimeVert.tangent = glm::vec3(0.0f); // Will be read from tangent data
        runtimeVert.numBones = vert.m_BoneWeights.numbones;
        for (int b = 0; b < 3; ++b) {
            runtimeVert.boneWeights[b] = vert.m_BoneWeights.weight[b];
            runtimeVert.boneIds[b] = vert.m_BoneWeights.bone[b];
        }

        m_vertices.push_back(runtimeVert);
    }

    // Read tangent data (follows vertex data)
    size_t tangentDataStart = vertexDataStart + vertexDataSize;
    if (tangentDataStart + sizeof(glm::vec4) * numVertices <= size) {
        for (int32_t i = 0; i < numVertices && i < static_cast<int32_t>(m_vertices.size()); ++i) {
            size_t tanOffset = tangentDataStart + i * sizeof(glm::vec4);
            glm::vec4 tangent;
            std::memcpy(&tangent, data + tanOffset, sizeof(glm::vec4));
            m_vertices[i].tangent = glm::vec3(tangent);
        }
    }

    Logger::Info("MDLModel: Parsed VVD - " + std::to_string(m_vertices.size()) + " vertices");
    return true;
}

// ============================================================================
// VTX (Index/Strip Data) Parsing
// ============================================================================

bool MDLModel::ParseVTX(const std::vector<char>& buffer) {
    if (buffer.size() < sizeof(vtxFileHeader_t)) {
        Logger::Error("MDLModel: VTX file too small for header");
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
    size_t size = buffer.size();

    vtxFileHeader_t header;
    std::memcpy(&header, data, sizeof(vtxFileHeader_t));

    // Validate VTX header
    if (header.version != VTX_VERSION_7) {
        Logger::Warn("MDLModel: Unexpected VTX version " + std::to_string(header.version) +
                     " (expected " + std::to_string(VTX_VERSION_7) + ")");
    }

    if (header.checkSum != m_checksum) {
        Logger::Warn("MDLModel: VTX checksum mismatch (MDL=" + std::to_string(m_checksum) +
                     ", VTX=" + std::to_string(header.checkSum) + ")");
    }

    // Clear existing indices
    m_indices.clear();

    // Parse body parts
    for (int32_t i = 0; i < header.numBodyParts && i < m_header.bodypart_count; ++i) {
        size_t bodyPartOffset = header.bodyPartOffset + i * sizeof(vtxBodyPart_t);
        if (bodyPartOffset + sizeof(vtxBodyPart_t) > size) continue;

        vtxBodyPart_t bodyPart;
        std::memcpy(&bodyPart, data + bodyPartOffset, sizeof(vtxBodyPart_t));

        // Parse models
        for (int32_t j = 0; j < bodyPart.numModels; ++j) {
            size_t modelOffset = bodyPart.modelOffset + j * sizeof(vtxModel_t);
            if (modelOffset + sizeof(vtxModel_t) > size) continue;

            vtxModel_t model;
            std::memcpy(&model, data + modelOffset, sizeof(vtxModel_t));

            // Parse LODs (use LOD 0 for now)
            if (model.numLODs <= 0) continue;

            size_t lodOffset = model.lodOffset + 0 * sizeof(vtxModelLOD_t);
            if (lodOffset + sizeof(vtxModelLOD_t) > size) continue;

            vtxModelLOD_t lod;
            std::memcpy(&lod, data + lodOffset, sizeof(vtxModelLOD_t));

            // Parse meshes
            for (int32_t k = 0; k < lod.numMeshes; ++k) {
                size_t meshOffset = lod.meshOffset + k * sizeof(vtxMesh_t);
                if (meshOffset + sizeof(vtxMesh_t) > size) continue;

                vtxMesh_t mesh;
                std::memcpy(&mesh, data + meshOffset, sizeof(vtxMesh_t));

                // Parse strip groups
                for (int32_t sg = 0; sg < mesh.numStripGroups; ++sg) {
                    size_t sgOffset = mesh.stripGroupHeaderOffset + sg * sizeof(vtxStripGroup_t);
                    if (sgOffset + sizeof(vtxStripGroup_t) > size) continue;

                    vtxStripGroup_t stripGroup;
                    std::memcpy(&stripGroup, data + sgOffset, sizeof(vtxStripGroup_t));

                    // Read strip group vertices
                    std::vector<vtxVertex_t> sgVertices;
                    sgVertices.reserve(stripGroup.numVerts);
                    for (int32_t v = 0; v < stripGroup.numVerts; ++v) {
                        size_t vOffset = sgOffset + stripGroup.vertOffset + v * sizeof(vtxVertex_t);
                        if (vOffset + sizeof(vtxVertex_t) > size) break;

                        vtxVertex_t vtx;
                        std::memcpy(&vtx, data + vOffset, sizeof(vtxVertex_t));
                        sgVertices.push_back(vtx);
                    }

                    // Read indices
                    std::vector<uint16_t> sgIndices;
                    sgIndices.reserve(stripGroup.numIndices);
                    for (int32_t idx = 0; idx < stripGroup.numIndices; ++idx) {
                        size_t idxOffset = sgOffset + stripGroup.indexOffset + idx * sizeof(uint16_t);
                        if (idxOffset + sizeof(uint16_t) > size) break;

                        uint16_t index;
                        std::memcpy(&index, data + idxOffset, sizeof(uint16_t));
                        sgIndices.push_back(index);
                    }

                    // Parse strips
                    for (int32_t s = 0; s < stripGroup.numStrips; ++s) {
                        size_t stripOffset = sgOffset + stripGroup.stripOffset + s * sizeof(vtxStrip_t);
                        if (stripOffset + sizeof(vtxStrip_t) > size) continue;

                        vtxStrip_t strip;
                        std::memcpy(&strip, data + stripOffset, sizeof(vtxStrip_t));

                        // Build indices for this strip
                        size_t idxStart = strip.indexOffset;
                        size_t idxCount = strip.numIndices;

                        if (strip.flags & STRIP_IS_TRILIST) {
                            // Triangle list - indices are already in order
                            for (size_t idx = 0; idx < idxCount; ++idx) {
                                size_t srcIdx = idxStart + idx;
                                if (srcIdx >= sgIndices.size()) break;

                                uint16_t vertIdx = sgIndices[srcIdx];
                                if (vertIdx >= sgVertices.size()) continue;

                                // Map to VVD vertex index
                                uint16_t vvdIndex = sgVertices[vertIdx].origMeshVertId;
                                m_indices.push_back(vvdIndex);
                            }
                        } else {
                            // Triangle strip - convert to triangle list
                            for (size_t idx = 2; idx < idxCount; ++idx) {
                                size_t i0 = idxStart + ((idx % 2 == 0) ? idx - 2 : idx - 1);
                                size_t i1 = idxStart + ((idx % 2 == 0) ? idx - 1 : idx - 2);
                                size_t i2 = idxStart + idx;

                                if (i0 >= sgIndices.size() || i1 >= sgIndices.size() || i2 >= sgIndices.size()) break;

                                uint16_t v0 = sgVertices[sgIndices[i0]].origMeshVertId;
                                uint16_t v1 = sgVertices[sgIndices[i1]].origMeshVertId;
                                uint16_t v2 = sgVertices[sgIndices[i2]].origMeshVertId;

                                m_indices.push_back(v0);
                                m_indices.push_back(v1);
                                m_indices.push_back(v2);
                            }
                        }
                    }
                }
            }
        }
    }

    Logger::Info("MDLModel: Parsed VTX - " + std::to_string(m_indices.size()) + " indices");
    return true;
}

// ============================================================================
// Geometry Building
// ============================================================================

bool MDLModel::BuildGeometry() {
    // If we have VVD and VTX data, the vertices and indices are already built
    // If we only have MDL (no VVD/VTX), we can't build renderable geometry

    if (m_vertices.empty()) {
        Logger::Warn("MDLModel: No vertex data available (missing VVD file)");
        return false;
    }

    if (m_indices.empty()) {
        Logger::Warn("MDLModel: No index data available (missing VTX file)");
        // Create a simple sequential index buffer as fallback
        m_indices.reserve(m_vertices.size());
        for (size_t i = 0; i < m_vertices.size(); ++i) {
            m_indices.push_back(static_cast<uint32_t>(i));
        }
    }

    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string MDLModel::ResolveString(const uint8_t* base, int32_t offset) const {
    if (offset < 0 || static_cast<size_t>(offset) >= 16777216) { // Reasonable upper limit (16MB)
        return "";
    }
    // Ensure we don't read past the MDL data
    if (offset >= static_cast<int32_t>(m_mdlData.size())) {
        return "";
    }
    return std::string(reinterpret_cast<const char*>(base + offset));
}

bool MDLModel::BoundsCheck(int32_t offset, size_t sizeNeeded, size_t fileSize) const {
    if (offset < 0) return false;
    if (static_cast<size_t>(offset) >= fileSize) return false;
    if (static_cast<size_t>(offset) + sizeNeeded > fileSize) return false;
    return true;
}

int32_t MDLModel::FindSequence(const std::string& name) const {
    for (size_t i = 0; i < m_sequences.size(); ++i) {
        if (m_sequences[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

std::string MDLModel::GetSequenceName(int32_t index) const {
    if (index >= 0 && index < static_cast<int32_t>(m_sequences.size())) {
        return m_sequences[index].name;
    }
    return "";
}

// ============================================================================
// MDLCache Implementation
// ============================================================================

MDLCache& MDLCache::Get() {
    static MDLCache instance;
    return instance;
}

std::shared_ptr<MDLModel> MDLCache::LoadModel(const std::string& path, const GameInfo& game) {
    // Check cache first
    auto it = m_models.find(path);
    if (it != m_models.end()) {
        return it->second;
    }

    // Load new model
    auto model = std::make_shared<MDLModel>();
    if (model->LoadFromFile(path, game)) {
        m_models[path] = model;
        return model;
    }

    Logger::Error("MDLCache: Failed to load model: " + path);
    return nullptr;
}

void MDLCache::UnloadModel(const std::string& path) {
    m_models.erase(path);
}

void MDLCache::Clear() {
    m_models.clear();
}

} // namespace veex
