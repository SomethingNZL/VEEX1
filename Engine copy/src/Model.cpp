#include "veex/Model.h"
#include "veex/MDL.h"
#include "veex/Logger.h"
#include <glad/gl.h>
#include <cstring>

namespace veex {

Model::Model() = default;

Model::~Model() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_EBO) glDeleteBuffers(1, &m_EBO);
}

bool Model::LoadFromFile(const std::string& path) {
    // TODO: Use tinygltf for GLB loading
    // Stub: simple quad
    m_vertices = {
        {{-0.5f, -0.5f, 0.0f}, {0,0,1}, {0,0}},
        {{ 0.5f, -0.5f, 0.0f}, {0,0,1}, {1,0}},
        {{ 0.5f,  0.5f, 0.0f}, {0,0,1}, {1,1}},
        {{-0.5f,  0.5f, 0.0f}, {0,0,1}, {0,1}}
    };
    m_indices = {0,1,2, 2,3,0};

    SetupMesh();
    Logger::Info("Model loaded (stub): " + path);
    return true;
}

bool Model::LoadMDL(const std::string& path, const GameInfo& game) {
    // Use MDLCache for efficient loading
    auto mdlModel = MDLCache::Get().LoadModel(path, game);
    if (!mdlModel) {
        Logger::Error("Model: Failed to load MDL: " + path);
        return false;
    }

    SetMDLModel(mdlModel);

    // Convert MDL vertices to our format
    const auto& mdlVertices = mdlModel->GetVertices();
    const auto& mdlIndices = mdlModel->GetIndices();

    m_vertices.reserve(mdlVertices.size());
    for (const auto& v : mdlVertices) {
        ModelVertex vert;
        vert.position = v.position;
        vert.normal = v.normal;
        vert.texCoord = v.texCoord;
        m_vertices.push_back(vert);
    }

    m_indices = mdlIndices;

    // Setup bone transforms for skeletal animation
    const auto& bones = mdlModel->GetBones();
    m_boneTransforms.reserve(bones.size());
    for (const auto& bone : bones) {
        BoneTransform bt;
        bt.name = bone.name;
        bt.parent = bone.parent;
        bt.transform = bone.poseToBone;
        m_boneTransforms.push_back(bt);
    }

    m_isSkeletal = !bones.empty();

    SetupMesh();

    Logger::Info("Model: Loaded MDL '" + path + "' - " + 
                 std::to_string(m_vertices.size()) + " vertices, " +
                 std::to_string(m_indices.size()) + " indices, " +
                 std::to_string(m_boneTransforms.size()) + " bones, " +
                 std::to_string(mdlModel->GetNumMaterials()) + " materials");

    return true;
}

void Model::SetupMesh() {
    if (m_VAO) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_EBO) {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }

    if (m_vertices.empty()) {
        Logger::Warn("Model::SetupMesh: No vertices to upload");
        return;
    }

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(ModelVertex), m_vertices.data(), GL_STATIC_DRAW);

    if (!m_indices.empty()) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    }

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
    glEnableVertexAttribArray(1);
    
    // TexCoord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, texCoord));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Model::Draw() const {
    if (!m_VAO) return;
    
    glBindVertexArray(m_VAO);
    if (!m_indices.empty()) {
        glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());
    }
    glBindVertexArray(0);
}

void Model::DrawWithAnimation(const std::vector<BoneTransform>& boneTransforms) const {
    // This would be used with a skeletal animation shader
    // For now, just draw normally - the animation is applied in the shader
    Draw();
}

void Model::UpdateBones(const std::vector<glm::mat4>& boneMatrices) {
    m_boneMatrices = boneMatrices;
}

void Model::SetMDLModel(std::shared_ptr<MDLModel> model) {
    m_mdlModel = model;
}

void Model::SetAnimationSequence(int32_t sequenceIndex) {
    if (m_mdlModel && sequenceIndex >= 0 && sequenceIndex < m_mdlModel->GetNumSequences()) {
        m_currentSequence = sequenceIndex;
        m_animationTime = 0.0f;
        m_animating = true;
    }
}

void Model::UpdateAnimation(float deltaTime) {
    if (!m_animating || !m_mdlModel || m_currentSequence < 0) return;

    const auto& seq = m_mdlModel->GetSequences()[m_currentSequence];
    if (seq.numFrames <= 0 || seq.frameRate <= 0.0f) return;

    m_animationTime += deltaTime * seq.frameRate;
    
    // Loop animation
    if (m_animationTime >= seq.numFrames) {
        m_animationTime = fmodf(m_animationTime, seq.numFrames);
    }

    // Calculate bone matrices for current frame
    CalculateBoneMatrices();
}

void Model::CalculateBoneMatrices() {
    if (!m_mdlModel || m_currentSequence < 0) return;

    const auto& bones = m_mdlModel->GetBones();
    if (bones.empty()) return;
    
    // Simple bone matrix calculation in bind pose
    // Full implementation would read animation data and interpolate
    m_boneMatrices.resize(bones.size());
    
    for (size_t i = 0; i < bones.size(); ++i) {
        // Start with pose-to-bone matrix
        glm::mat4 boneMatrix = bones[i].poseToBone;
        
        // Apply parent transforms
        if (bones[i].parent >= 0 && static_cast<size_t>(bones[i].parent) < m_boneMatrices.size()) {
            boneMatrix = m_boneMatrices[bones[i].parent] * boneMatrix;
        }
        
        m_boneMatrices[i] = boneMatrix;
    }
}

} // namespace veex
