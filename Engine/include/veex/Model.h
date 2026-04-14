#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace veex {

class GameInfo;
class MDLModel; // Forward declaration

struct ModelVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

// Skeletal animation bone data
struct BoneTransform {
    glm::mat4 transform;
    std::string name;
    int32_t parent;
};

class Model {
public:
    Model();
    ~Model();

    // Load from various formats
    bool LoadFromFile(const std::string& path);
    bool LoadMDL(const std::string& path, const GameInfo& game);
    
    // Drawing
    void Draw() const;
    void DrawWithAnimation(const std::vector<BoneTransform>& boneTransforms) const;

    // Skeletal animation support
    bool IsSkeletal() const { return m_isSkeletal; }
    const std::vector<BoneTransform>& GetBoneTransforms() const { return m_boneTransforms; }
    void UpdateBones(const std::vector<glm::mat4>& boneMatrices);
    
    // MDL model access
    std::shared_ptr<MDLModel> GetMDLModel() const { return m_mdlModel; }
    void SetMDLModel(std::shared_ptr<MDLModel> model);

    // Animation control
    void SetAnimationSequence(int32_t sequenceIndex);
    int32_t GetCurrentSequence() const { return m_currentSequence; }
    void SetAnimationTime(float time) { m_animationTime = time; }
    float GetAnimationTime() const { return m_animationTime; }
    void UpdateAnimation(float deltaTime);

private:
    void SetupMesh();
    void CalculateBoneMatrices();

    // OpenGL buffers
    unsigned int m_VAO = 0, m_VBO = 0, m_EBO = 0;
    
    // Mesh data
    std::vector<ModelVertex> m_vertices;
    std::vector<unsigned int> m_indices;
    
    // Skeletal animation data
    bool m_isSkeletal = false;
    std::shared_ptr<MDLModel> m_mdlModel;
    std::vector<BoneTransform> m_boneTransforms;
    std::vector<glm::mat4> m_boneMatrices;
    
    // Animation state
    int32_t m_currentSequence = -1;
    float m_animationTime = 0.0f;
    bool m_animating = false;
};

} // namespace veex
