#include "veex/MDL.h"
#include "veex/FileSystem.h"
#include "veex/Logger.h"
#include <cstring>
#include <algorithm>

namespace veex {

// ============================================================================
// MDLModel Implementation
// ============================================================================

MDLModel::MDLModel() 
    : m_mass(0.0f), m_eyePosition(0.0f), m_bbMin(0.0f), m_bbMax(0.0f), m_baseData(nullptr) {
    std::memset(&m_header, 0, sizeof(m_header));
}

MDLModel::~MDLModel() = default;

bool MDLModel::LoadFromFile(const std::string& path, const GameInfo& game) {
    std::vector<char> buffer;
    if (!ReadFile(path, game, buffer)) {
        Logger::Error("MDLModel: Failed to read MDL file: " + path);
        return false;
    }
    return LoadFromBuffer(buffer);
}

bool MDLModel::LoadFromBuffer(const std::vector<char>& buffer) {
    if (buffer.empty()) {
        Logger::Error("MDLModel: Empty buffer");
        return false;
    }

    m_baseData = reinterpret_cast<const uint8_t*>(buffer.data());
    size_t size = buffer.size();

    // Parse header first
    if (!ParseHeader(m_baseData, size)) {
        return false;
    }

    // Parse bones
    if (!ParseBones(m_baseData, size)) {
        Logger::Warn("MDLModel: Failed to parse bones (may be okay for some models)");
    }

    // Parse textures/materials
    if (!ParseTextures(m_baseData, size)) {
        Logger::Warn("MDLModel: Failed to parse textures");
    }

    // Parse body parts (meshes and vertices)
    if (!ParseBodyParts(m_baseData, size)) {
        Logger::Error("MDLModel: Failed to parse body parts");
        return false;
    }

    // Parse sequences (animations)
    ParseSequences(m_baseData, size);

    Logger::Info("MDLModel: Loaded '" + m_name + "' - " + 
                 std::to_string(m_vertices.size()) + " vertices, " +
                 std::to_string(m_indices.size()) + " indices, " +
                 std::to_string(m_bones.size()) + " bones, " +
                 std::to_string(m_materialNames.size()) + " materials");

    return true;
}

bool MDLModel::ParseHeader(const uint8_t* data, size_t size) {
    if (size < sizeof(MDLHeader)) {
        Logger::Error("MDLModel: File too small for header");
        return false;
    }

    std::memcpy(&m_header, data, sizeof(MDLHeader));

    // Check magic number - "IDSTUDIO" = 0x49445354
    if (m_header.id != 0x49445354) {
        Logger::Error("MDLModel: Invalid file ID (expected IDSTUDIO)");
        return false;
    }

    // Check version (SDK2013 uses version 49)
    if (m_header.version < 44 || m_header.version > 49) {
        Logger::Warn("MDLModel: Unexpected version " + std::to_string(m_header.version) + 
                     " (expected 44-49 for Source SDK 2013)");
    }

    // Extract model name
    m_name = std::string(m_header.name, strnlen(m_header.name, 64));

    m_mass = static_cast<float>(m_header.mass);
    m_eyePosition = m_header.eyePosition;
    m_bbMin = m_header.bbMin;
    m_bbMax = m_header.bbMax;

    Logger::Info("MDLModel: Header parsed - name='" + m_name + 
                 "', version=" + std::to_string(m_header.version) +
                 ", bones=" + std::to_string(m_header.numBones) +
                 ", textures=" + std::to_string(m_header.numTextures) +
                 ", bodyparts=" + std::to_string(m_header.numBodyParts));

    return true;
}

bool MDLModel::ParseBones(const uint8_t* data, size_t size) {
    if (m_header.numBones <= 0) {
        return true; // Some models have no bones
    }

    if (m_header.boneIndex < 0 || static_cast<size_t>(m_header.boneIndex) >= size) {
        Logger::Error("MDLModel: Invalid bone index offset");
        return false;
    }

    m_bones.reserve(m_header.numBones);

    for (int32_t i = 0; i < m_header.numBones; ++i) {
        size_t boneOffset = m_header.boneIndex + i * sizeof(MDLBone);
        if (boneOffset + sizeof(MDLBone) > size) {
            Logger::Error("MDLModel: Bone " + std::to_string(i) + " out of bounds");
            return false;
        }

        MDLBone boneHeader;
        std::memcpy(&boneHeader, data + boneOffset, sizeof(MDLBone));

        Bone bone;
        bone.name = ResolveString(data, boneHeader.nameIndex);
        bone.parent = boneHeader.parent;
        bone.position = boneHeader.value;
        bone.rotation = boneHeader.qAlignment;
        bone.scale = boneHeader.scale;
        bone.flags = boneHeader.flags;

        // Read pose-to-bone matrix if available
        if (boneHeader.poseToBoneIndex >= 0 && 
            static_cast<size_t>(boneHeader.poseToBoneIndex) < size) {
            float matrix[3][4];
            std::memcpy(matrix, data + boneHeader.poseToBoneIndex, sizeof(matrix));
            bone.poseToBone = glm::mat4(
                matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
                matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
                matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
                0.0f, 0.0f, 0.0f, 1.0f
            );
        } else {
            bone.poseToBone = glm::mat4(1.0f);
        }

        m_bones.push_back(bone);
    }

    return true;
}

bool MDLModel::ParseTextures(const uint8_t* data, size_t size) {
    if (m_header.numTextures <= 0) {
        return true;
    }

    if (m_header.textureIndex < 0 || static_cast<size_t>(m_header.textureIndex) >= size) {
        Logger::Error("MDLModel: Invalid texture index offset");
        return false;
    }

    m_materialNames.reserve(m_header.numTextures);

    for (int32_t i = 0; i < m_header.numTextures; ++i) {
        size_t textureOffset = m_header.textureIndex + i * sizeof(MDLTexture);
        if (textureOffset + sizeof(MDLTexture) > size) {
            Logger::Error("MDLModel: Texture " + std::to_string(i) + " out of bounds");
            return false;
        }

        MDLTexture textureHeader;
        std::memcpy(&textureHeader, data + textureOffset, sizeof(MDLTexture));

        std::string materialName = ResolveString(data, textureHeader.nameIndex);
        m_materialNames.push_back(materialName);
    }

    return true;
}

bool MDLModel::ParseBodyParts(const uint8_t* data, size_t size) {
    if (m_header.numBodyParts <= 0) {
        Logger::Error("MDLModel: No body parts found");
        return false;
    }

    if (m_header.bodyPartIndex < 0 || static_cast<size_t>(m_header.bodyPartIndex) >= size) {
        Logger::Error("MDLModel: Invalid body part index offset");
        return false;
    }

    for (int32_t i = 0; i < m_header.numBodyParts; ++i) {
        size_t bodyPartOffset = m_header.bodyPartIndex + i * sizeof(MDLBodyPart);
        if (bodyPartOffset + sizeof(MDLBodyPart) > size) {
            Logger::Error("MDLModel: Body part " + std::to_string(i) + " out of bounds");
            return false;
        }

        MDLBodyPart bodyPart;
        std::memcpy(&bodyPart, data + bodyPartOffset, sizeof(MDLBodyPart));

        // Parse models within this body part
        for (int32_t j = 0; j < bodyPart.numModels; ++j) {
            size_t modelOffset = bodyPart.modelIndex + j * sizeof(MDLStudioModel);
            if (modelOffset + sizeof(MDLStudioModel) > size) {
                Logger::Error("MDLModel: Model " + std::to_string(j) + " out of bounds");
                continue;
            }

            MDLStudioModel modelHeader;
            std::memcpy(&modelHeader, data + modelOffset, sizeof(MDLStudioModel));

            // Parse meshes within this model
            for (int32_t k = 0; k < modelHeader.numMeshes; ++k) {
                size_t meshOffset = modelHeader.meshIndex + k * sizeof(MDLMesh);
                if (meshOffset + sizeof(MDLMesh) > size) {
                    Logger::Error("MDLModel: Mesh " + std::to_string(k) + " out of bounds");
                    continue;
                }

                MDLMesh meshHeader;
                std::memcpy(&meshHeader, data + meshOffset, sizeof(MDLMesh));

                // Parse vertices
                if (meshHeader.numVertices > 0 && meshHeader.vertexIndex >= 0) {
                    size_t vertexOffset = meshHeader.vertexIndex + meshHeader.vertexOffset;
                    if (vertexOffset + sizeof(MDLVertex) * meshHeader.numVertices <= size) {
                        size_t vertexStart = m_vertices.size();
                        m_vertices.reserve(m_vertices.size() + meshHeader.numVertices);

                        for (int32_t v = 0; v < meshHeader.numVertices; ++v) {
                            MDLVertex vert;
                            std::memcpy(&vert, data + vertexOffset + v * sizeof(MDLVertex), sizeof(MDLVertex));
                            
                            // Convert to runtime vertex format
                            MDLVertex runtimeVert;
                            runtimeVert.position = vert.position;
                            runtimeVert.normal = vert.normal;
                            runtimeVert.texCoord = vert.texCoord;
                            runtimeVert.tangent = vert.tangent;
                            std::memcpy(runtimeVert.boneWeights, vert.boneWeights, 4);
                            runtimeVert.numBones = vert.numBones;
                            
                            m_vertices.push_back(runtimeVert);
                        }
                    }
                }

                // Parse indices (via strips)
                // For now, we'll create a simple triangle list from the mesh
                // A full implementation would parse strips and groups
                if (meshHeader.numVerts > 0) {
                    // Generate indices based on vertex count
                    // This is a simplified approach - full MDL parsing would read strip data
                    size_t vertexStart = m_vertices.size() - meshHeader.numVerts; // Vertices were added above
                    m_indices.reserve(m_indices.size() + meshHeader.numVerts);
                    
                    // For now, create a simple triangle fan or just store vertex count
                    // The actual index data would come from strip headers
                    for (int32_t idx = 0; idx < meshHeader.numVerts; ++idx) {
                        m_indices.push_back(static_cast<uint32_t>(vertexStart + idx));
                    }
                }
            }
        }
    }

    return true;
}

bool MDLModel::ParseSequences(const uint8_t* data, size_t size) {
    if (m_header.numLocalSeq <= 0) {
        return true; // No animations
    }

    if (m_header.localSeqIndex < 0 || static_cast<size_t>(m_header.localSeqIndex) >= size) {
        Logger::Error("MDLModel: Invalid sequence index offset");
        return false;
    }

    m_sequences.reserve(m_header.numLocalSeq);

    for (int32_t i = 0; i < m_header.numLocalSeq; ++i) {
        size_t seqOffset = m_header.localSeqIndex + i * sizeof(MDLSeqDesc);
        if (seqOffset + sizeof(MDLSeqDesc) > size) {
            Logger::Error("MDLModel: Sequence " + std::to_string(i) + " out of bounds");
            continue;
        }

        MDLSeqDesc seqHeader;
        std::memcpy(&seqHeader, data + seqOffset, sizeof(MDLSeqDesc));

        AnimationSequence seq;
        seq.name = ResolveString(data, seqHeader.nameIndex);
        seq.activity = seqHeader.activity;
        seq.flags = seqHeader.flags;
        seq.frameRate = seqHeader.frameRate;
        seq.numFrames = seqHeader.numFrames;
        seq.numBlends = seqHeader.numBlends;
        seq.animIndex = seqHeader.animIndex;

        if (seqHeader.numBlends > 0) {
            seq.blendTypes.resize(seqHeader.numBlends);
            seq.blendParams.resize(seqHeader.numBlends);
            std::memcpy(seq.blendTypes.data(), &seqHeader.blendType, 
                       seqHeader.numBlends * sizeof(int32_t));
            std::memcpy(seq.blendParams.data(), seqHeader.blendParam, 
                       seqHeader.numBlends * sizeof(float));
        }

        if (seqHeader.groupSize > 0) {
            seq.groupSize[0] = seqHeader.groupSize;
        }

        m_sequences.push_back(seq);
    }

    return true;
}

std::string MDLModel::ResolveString(const uint8_t* base, int32_t offset) const {
    if (offset < 0 || static_cast<size_t>(offset) >= 65536) { // Reasonable limit
        return "";
    }
    return std::string(reinterpret_cast<const char*>(base + offset));
}

int32_t MDLModel::FindSequence(const std::string& name) const {
    for (size_t i = 0; i < m_sequences.size(); ++i) {
        if (m_sequences[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
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