#pragma once
// PropDynamicEntity.h
// Entity type for animated MDL models (props, characters, etc.)
// ---------------------------------------------------------------
// Supports:
//   - MDL model loading and rendering
//   - Animation sequence selection and playback
//   - Skin selection
//   - Model scaling
//   - Bone-based skeletal animation
// ---------------------------------------------------------------

#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/Model.h"
#include "veex/Logger.h"
#include <memory>
#include <string>

namespace veex {

class GameInfo;

class PropDynamicEntity : public Entity {
public:
    explicit PropDynamicEntity(int id) : Entity(id, "prop_dynamic") {}
    virtual ~PropDynamicEntity() = default;

    bool Spawn(const EntityData& ed) override;
    void Update(float dt) override;

    // Model access
    const std::string& GetModelPath() const { return m_modelPath; }
    Model* GetModel() { return m_model.get(); }
    const Model* GetModel() const { return m_model.get(); }

    // Animation control
    void SetSequence(const std::string& sequenceName);
    void SetSequence(int32_t sequenceIndex);
    int32_t GetSequence() const { return m_currentSequence; }
    float GetAnimationTime() const { return m_animationTime; }
    void SetPlaybackRate(float rate) { m_playbackRate = rate; }
    float GetPlaybackRate() const { return m_playbackRate; }

    // Skin
    int GetSkin() const { return m_skin; }
    void SetSkin(int skin) { m_skin = skin; }

    // Scale
    float GetScale() const { return m_scale; }
    void SetScale(float scale) { m_scale = scale; }

    // Body groups
    int GetBodyGroup(int index) const;
    void SetBodyGroup(int index, int value);

    // Rendering
    void Draw() const;

    // Input handlers (Source Engine style)
    void InputSetAnimation(const std::string& param);
    void InputSetSequence(const std::string& param);
    void InputSetSkin(const std::string& param);
    void InputSetPlaybackRate(const std::string& param);

protected:
    std::string m_modelPath;
    std::unique_ptr<Model> m_model;
    
    int32_t m_currentSequence = 0;
    float m_animationTime = 0.0f;
    float m_playbackRate = 1.0f;
    
    int m_skin = 0;
    float m_scale = 1.0f;
    
    std::vector<int> m_bodyGroups;
    
    // Cached model path for reloading
    std::string m_cachedModelPath;
};

// Entity factory registration
// This will be defined in PropDynamicEntity.cpp

} // namespace veex