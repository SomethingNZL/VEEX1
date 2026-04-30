#include "veex/PropDynamicEntity.h"
#include "veex/MDL.h"
#include "veex/FileSystem.h"
#include "veex/GameInfo.h"
#include "veex/Logger.h"
#include "veex/Entity.h"

namespace veex {

// Entity factory registration
VEEX_REGISTER_ENTITY("prop_dynamic", PropDynamicEntity);

bool PropDynamicEntity::Spawn(const EntityData& ed) {
    // Get model path
    m_modelPath = ed.GetString("model");
    if (m_modelPath.empty()) {
        Logger::Warn("PropDynamicEntity id=" + std::to_string(m_id) + " has no model KV — discarding.");
        return false;
    }

    // Get other properties
    m_skin = ed.GetInt("skin", 0);
    m_scale = ed.GetFloat("scale", 1.0f);
    m_currentSequence = ed.GetInt("sequence", 0);
    m_playbackRate = ed.GetFloat("playbackrate", 1.0f);

    // Cache the model path for potential reloading
    m_cachedModelPath = m_modelPath;

    Logger::Info("PropDynamicEntity: Spawning '" + m_modelPath + 
                 "' at " + std::to_string(m_position.x) + "," + 
                 std::to_string(m_position.y) + "," + 
                 std::to_string(m_position.z));

    // Create and load the model
    m_model = std::make_unique<Model>();
    
    // Try to load as MDL (check extension)
    if (m_modelPath.size() > 4 && 
        m_modelPath.substr(m_modelPath.size() - 4) == ".mdl") {
        // Use global GameInfo set by Server::Init
        const GameInfo* gameInfo = Entity::GetGameInfo();
        if (gameInfo) {
            if (!m_model->LoadMDL(m_modelPath, *gameInfo)) {
                Logger::Warn("PropDynamicEntity: Failed to load MDL model: " + m_modelPath);
                // Continue anyway - model will be a stub
            }
        } else {
            Logger::Warn("PropDynamicEntity: No GameInfo available for MDL loading");
        }
    } else {
        // Try loading as a simple model
        if (!m_model->LoadFromFile(m_modelPath)) {
            Logger::Warn("PropDynamicEntity: Failed to load model: " + m_modelPath);
            return false;
        }
    }

    // Register input handlers for Source Engine style I/O
    RegisterInput("SetAnimation", [this](Entity* self, const std::string& param) {
        InputSetAnimation(param);
    });
    
    RegisterInput("SetSequence", [this](Entity* self, const std::string& param) {
        InputSetSequence(param);
    });
    
    RegisterInput("SetSkin", [this](Entity* self, const std::string& param) {
        InputSetSkin(param);
    });
    
    RegisterInput("SetPlaybackRate", [this](Entity* self, const std::string& param) {
        InputSetPlaybackRate(param);
    });

    // Set initial animation if specified
    if (m_model && m_model->GetMDLModel()) {
        auto mdlModel = m_model->GetMDLModel();
        if (m_currentSequence >= 0 && m_currentSequence < mdlModel->GetNumSequences()) {
            m_model->SetAnimationSequence(m_currentSequence);
        }
    }

    return true;
}

void PropDynamicEntity::Update(float dt) {
    if (!m_model) return;

    // Update animation
    m_animationTime += dt * m_playbackRate;
    m_model->UpdateAnimation(dt * m_playbackRate);
}

void PropDynamicEntity::SetSequence(const std::string& sequenceName) {
    if (!m_model || !m_model->GetMDLModel()) return;
    
    auto mdlModel = m_model->GetMDLModel();
    int32_t seqIndex = mdlModel->FindSequence(sequenceName);
    if (seqIndex >= 0) {
        m_currentSequence = seqIndex;
        m_model->SetAnimationSequence(seqIndex);
        m_animationTime = 0.0f;
        Logger::Info("PropDynamicEntity: Set sequence to '" + sequenceName + "'");
    } else {
        Logger::Warn("PropDynamicEntity: Unknown sequence '" + sequenceName + "'");
    }
}

void PropDynamicEntity::SetSequence(int32_t sequenceIndex) {
    if (!m_model || !m_model->GetMDLModel()) return;
    
    auto mdlModel = m_model->GetMDLModel();
    if (sequenceIndex >= 0 && sequenceIndex < mdlModel->GetNumSequences()) {
        m_currentSequence = sequenceIndex;
        m_model->SetAnimationSequence(sequenceIndex);
        m_animationTime = 0.0f;
    } else {
        Logger::Warn("PropDynamicEntity: Invalid sequence index " + std::to_string(sequenceIndex));
    }
}

int PropDynamicEntity::GetBodyGroup(int index) const {
    if (index >= 0 && index < static_cast<int>(m_bodyGroups.size())) {
        return m_bodyGroups[index];
    }
    return 0;
}

void PropDynamicEntity::SetBodyGroup(int index, int value) {
    // Resize if needed
    if (index >= static_cast<int>(m_bodyGroups.size())) {
        m_bodyGroups.resize(index + 1, 0);
    }
    m_bodyGroups[index] = value;
}

void PropDynamicEntity::Draw() const {
    // Rendering is handled by the Renderer class
    // This method is a placeholder for future implementation
    // The actual drawing will use the Model class and proper transformation matrices
}

void PropDynamicEntity::InputSetAnimation(const std::string& param) {
    // param is the animation/sequence name
    if (!param.empty()) {
        SetSequence(param);
    }
}

void PropDynamicEntity::InputSetSequence(const std::string& param) {
    // param is the sequence name or index
    if (!param.empty()) {
        // Try parsing as integer first
        try {
            int seqIndex = std::stoi(param);
            SetSequence(seqIndex);
        } catch (...) {
            // Not a number, treat as sequence name
            SetSequence(param);
        }
    }
}

void PropDynamicEntity::InputSetSkin(const std::string& param) {
    if (!param.empty()) {
        try {
            int skin = std::stoi(param);
            SetSkin(skin);
        } catch (...) {
            Logger::Warn("PropDynamicEntity: Invalid skin value: " + param);
        }
    }
}

void PropDynamicEntity::InputSetPlaybackRate(const std::string& param) {
    if (!param.empty()) {
        try {
            float rate = std::stof(param);
            SetPlaybackRate(rate);
        } catch (...) {
            Logger::Warn("PropDynamicEntity: Invalid playback rate: " + param);
        }
    }
}

} // namespace veex
