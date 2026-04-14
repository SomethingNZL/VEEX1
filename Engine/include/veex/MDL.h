#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace veex {

// Forward declarations
class GameInfo;

// ============================================================================
// MDL File Format Structures (Valve Source Engine SDK2013)
// ============================================================================

// File header
#pragma pack(push, 1)
struct MDLHeader {
    int32_t id;                 // Should be "IDSTUDIO"
    int32_t version;            // File version
    int32_t checksum;           // Same as studiohdr_t, can be used to detect file swaps
    char name[64];              // Model name
    int32_t length;             // File length
    glm::vec3 eyePosition;      // Eye position
    glm::vec3 illuminationPosition; // Illumination position
    glm::vec3 hullMin;          // Hull min
    glm::vec3 hullMax;          // Hull max
    glm::vec3 bbMin;            // Bounding box min
    glm::vec3 bbMax;            // Bounding box max
    uint32_t flags;             // Flags
    int32_t numBones;           // Number of bones
    int32_t boneIndex;          // Offset to bone array
    int32_t numBoneControllers; // Number of bone controllers
    int32_t boneControllerIndex; // Offset to bone controller array
    int32_t numHitboxSets;      // Number of hitbox sets
    int32_t hitboxSetIndex;     // Offset to hitbox set array
    int32_t numLocalAnim;       // Number of animations
    int32_t localAnimIndex;     // Offset to animation array
    int32_t numLocalSeq;        // Number of sequences
    int32_t localSeqIndex;      // Offset to sequence array
    int32_t activityListVersion; // Activity list version
    int32_t eventsIndexed;      // Events indexed
    int32_t numTextures;        // Number of textures
    int32_t textureIndex;       // Offset to texture array
    int32_t numCDTextures;      // Number of CD textures
    int32_t CDTextureIndex;     // Offset to CD texture array
    int32_t numSkinRef;         // Number of skin references
    int32_t numSkinFamilies;    // Number of skin families
    int32_t skinIndex;          // Offset to skin families
    int32_t numBodyParts;       // Number of body parts
    int32_t bodyPartIndex;      // Offset to body part array
    int32_t numLocalAttachments; // Number of attachments
    int32_t localAttachmentIndex; // Offset to attachment array
    int32_t numLocalNodes;      // Number of local nodes
    int32_t localNodeIndex;     // Offset to local node array
    int32_t numLocalNodesName;  // Number of local node names
    int32_t localNodeNameIndex; // Offset to local node name array
    int32_t numFlexDesc;        // Number of flex descriptors
    int32_t flexDescIndex;      // Offset to flex descriptor array
    int32_t numFlexControllers; // Number of flex controllers
    int32_t flexControllerIndex; // Offset to flex controller array
    int32_t numFlexRules;       // Number of flex rules
    int32_t flexRuleIndex;      // Offset to flex rule array
    int32_t numIKChains;        // Number of IK chains
    int32_t IKChainIndex;       // Offset to IK chain array
    int32_t numMouths;          // Number of mouths
    int32_t mouthIndex;         // Offset to mouth array
    int32_t numLocalPoseParameters; // Number of local pose parameters
    int32_t localPoseParameterIndex; // Offset to local pose parameter array
    int32_t surfacePropIndex;   // Offset to surface property name
    int32_t keyValueIndex;      // Offset to key value data
    int32_t keyValueSize;       // Key value data size
    int32_t numLocalIKAutoPlayLocks; // Number of IK autoplay locks
    int32_t localIKAutoPlayLockIndex; // Offset to IK autoplay lock array
    int32_t numIKLocks;         // Number of IK locks
    int32_t IKLockIndex;        // Offset to IK lock array
    int32_t mass;               // Model mass
    int32_t contents;           // Model contents
    int32_t numIncludeModels;   // Number of include models
    int32_t includeModelIndex;  // Offset to include model array
    int32_t serializedStudioHdrIndex; // Offset to serialized studio header
    int32_t numEncodedVertexAnims; // Number of encoded vertex animations
    int32_t encodedVertexAnimIndex; // Offset to encoded vertex animation array
    int32_t numVertexAnims;     // Number of vertex animations
    int32_t vertexAnimIndex;    // Offset to vertex animation array
    uint8_t vertexAnimFixedDescriptionSize; // Vertex animation fixed description size
    uint8_t pad[3];             // Padding
};

// Bone structure
struct MDLBone {
    int32_t nameIndex;          // Offset to bone name
    int32_t parent;             // Parent bone index
    int32_t boneController[6];  // Bone controllers
    glm::vec3 value;            // Default values
    glm::vec3 scale;            // Scale values
    int32_t poseToBoneIndex;    // Offset to pose-to-bone matrix
    glm::quat qAlignment;       // Alignment quaternion
    int32_t flags;              // Bone flags
    int32_t proceduralType;     // Procedural type
    int32_t proceduralRuleOffset; // Offset to procedural rule
    int32_t physicsBoneIndex;   // Physics bone index
    int32_t surfacePropIdx;     // Surface property index
    int32_t contents;           // Bone contents
    uint32_t unused[8];         // Unused
};

// Texture (material) structure
struct MDLTexture {
    int32_t nameIndex;          // Offset to texture name
    uint32_t flags;             // Texture flags
    int32_t used;               // Used
    int32_t unused1;            // Unused
    char* pName;                // Pointer to name (runtime only)
    int32_t material;           // Material index (runtime only)
    int32_t clientMaterial;     // Client material index (runtime only)
    uint32_t unused[2];         // Unused
};

// Body part structure
struct MDLBodyPart {
    int32_t nameIndex;          // Offset to body part name
    int32_t numModels;          // Number of models
    int32_t base;               // Base
    int32_t modelIndex;         // Offset to model array
};

// Model structure (within body part)
struct MDLStudioModel {
    char name[64];              // Model name
    int32_t type;               // Model type
    float boundingRadius;       // Bounding radius
    int32_t numMeshes;          // Number of meshes
    int32_t meshIndex;          // Offset to mesh array
    int32_t numVertices;        // Number of vertices
    int32_t vertexIndex;        // Offset to vertex array
    int32_t tangentsIndex;      // Offset to tangent array
    int32_t numAttachments;     // Number of attachments
    int32_t attachmentIndex;    // Offset to attachment array
    int32_t numEyeballs;        // Number of eyeballs
    int32_t eyeballIndex;       // Offset to eyeball array
    uint32_t unused[8];         // Unused
};

// Mesh structure
struct MDLMesh {
    int32_t numVerts;           // Number of vertices
    int32_t vertexIndex;        // Offset to vertex array
    int32_t flexes;             // Flexes
    int32_t material;           // Material index
    int32_t modelIndex;         // Model index
    int32_t numVertices;        // Number of vertices
    int32_t vertexOffset;       // Vertex offset
    int32_t flexDescCount;      // Flex descriptor count
    int32_t flexDescIndex;      // Flex descriptor index
};

// Vertex structure (strip header)
struct MDLStripHeader {
    int32_t numIndices;         // Number of indices
    int32_t indexOffset;        // Offset to index array
    int32_t numVertices;        // Number of vertices
    int32_t vertexOffset;       // Offset to vertex array
    int16_t numBones;           // Number of bones
    uint8_t state;              // State
    uint8_t reserved;           // Reserved
    int32_t numBoneStateChanges; // Number of bone state changes
    int32_t boneStateChangeIndex; // Offset to bone state change array
};

// Animation header
struct MDLAnimHeader {
    int16_t size;               // Size
    int16_t movementIndex;      // Movement index
    int32_t group;              // Group
    int32_t unused[2];          // Unused
};

// Sequence descriptor
struct MDLSeqDesc {
    int32_t nameIndex;          // Offset to sequence name
    int32_t activityNameIndex;  // Offset to activity name
    int32_t flags;              // Sequence flags
    int32_t activity;           // Activity
    int32_t actWeight;          // Activity weight
    int32_t numEvents;          // Number of events
    int32_t eventIndex;         // Offset to event array
    float boundingBox[6];       // Bounding box
    int32_t motionType;         // Motion type
    int32_t motionBone;         // Motion bone
    glm::vec3 linearMovement;   // Linear movement
    int32_t autoMovePosIndex;   // Auto move position index
    int32_t autoMoveAngleIndex; // Auto move angle index
    float bbMin[3];             // Bounding box min
    float bbMax[3];             // Bounding box max
    int32_t numBlends;          // Number of blends
    int32_t animIndex;          // Offset to animation index
    int32_t blendType;          // Blend type
    float blendParam[2];        // Blend parameters
    int32_t groupSize;          // Group size
    int32_t groupIndex;         // Offset to group index
    int32_t localHierarchy;     // Local hierarchy
    int32_t sectionIndex;       // Section index
    int32_t sectionFrames;      // Section frames
    float frameRate;            // Frame rate
    float numFrames;            // Number of frames
    int32_t poseKeyIndex;       // Pose key index
    int32_t numIKRules;         // Number of IK rules
    int32_t numAutoPlayLocks;   // Number of autoplay locks
    uint32_t unused[8];         // Unused
};

#pragma pack(pop)

// ============================================================================
// Runtime MDL Model Data
// ============================================================================

// Bone data for animation
struct Bone {
    std::string name;
    int32_t parent;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    glm::mat4 poseToBone;
    int32_t flags;
};

// Vertex data
struct MDLVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    uint8_t boneWeights[4];
    uint8_t numBones;
};

// Animation sequence data
struct AnimationSequence {
    std::string name;
    int32_t activity;
    int32_t flags;
    float frameRate;
    float numFrames;
    int32_t numBlends;
    std::vector<int32_t> blendTypes;
    std::vector<float> blendParams;
    int32_t groupSize[2];
    int32_t animIndex;
};

// Loaded MDL model
class MDLModel {
public:
    MDLModel();
    ~MDLModel();

    // Load from file path (will use FileSystem to resolve)
    bool LoadFromFile(const std::string& path, const GameInfo& game);
    
    // Load from buffer (for VPK support)
    bool LoadFromBuffer(const std::vector<char>& buffer);

    // Accessors
    const std::vector<Bone>& GetBones() const { return m_bones; }
    const std::vector<MDLVertex>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    const std::vector<std::string>& GetMaterialNames() const { return m_materialNames; }
    const std::vector<AnimationSequence>& GetSequences() const { return m_sequences; }
    
    int32_t GetNumBones() const { return static_cast<int32_t>(m_bones.size()); }
    int32_t GetNumVertices() const { return static_cast<int32_t>(m_vertices.size()); }
    int32_t GetNumIndices() const { return static_cast<int32_t>(m_indices.size()); }
    int32_t GetNumMaterials() const { return static_cast<int32_t>(m_materialNames.size()); }
    int32_t GetNumSequences() const { return static_cast<int32_t>(m_sequences.size()); }

    // Animation
    bool GetSequenceIndex(const std::string& name) const;
    int32_t FindSequence(const std::string& name) const;

    // Model info
    const std::string& GetName() const { return m_name; }
    float GetMass() const { return m_mass; }
    const glm::vec3& GetEyePosition() const { return m_eyePosition; }
    const glm::vec3& GetBoundingBoxMin() const { return m_bbMin; }
    const glm::vec3& GetBoundingBoxMax() const { return m_bbMax; }

private:
    bool ParseHeader(const uint8_t* data, size_t size);
    bool ParseBones(const uint8_t* data, size_t size);
    bool ParseTextures(const uint8_t* data, size_t size);
    bool ParseBodyParts(const uint8_t* data, size_t size);
    bool ParseSequences(const uint8_t* data, size_t size);
    
    std::string ResolveString(const uint8_t* base, int32_t offset) const;
    
    MDLHeader m_header;
    std::string m_name;
    float m_mass;
    glm::vec3 m_eyePosition;
    glm::vec3 m_bbMin, m_bbMax;
    
    std::vector<Bone> m_bones;
    std::vector<MDLVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<std::string> m_materialNames;
    std::vector<AnimationSequence> m_sequences;
    
    // Temporary storage during parsing
    const uint8_t* m_baseData = nullptr;
};

// MDL Model cache for efficient loading
class MDLCache {
public:
    static MDLCache& Get();
    
    std::shared_ptr<MDLModel> LoadModel(const std::string& path, const GameInfo& game);
    void UnloadModel(const std::string& path);
    void Clear();
    
private:
    MDLCache() = default;
    std::unordered_map<std::string, std::shared_ptr<MDLModel>> m_models;
};

} // namespace veex