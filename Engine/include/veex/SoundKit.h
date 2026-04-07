#pragma once
#include "miniaudio.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "veex/GameInfo.h"

namespace veex {

class BSP;

struct SoundInstance {
    ma_sound sound;
    ma_lpf lpf;
    glm::vec3 position; 
};

class SoundKit {
public:
    static SoundKit& Get() {
        static SoundKit instance;
        return instance;
    }

    SoundKit();
    ~SoundKit();

    // Matches Material::Initialize(const GameInfo& game)
    bool Initialize(const GameInfo& game);
    
    void Update(const glm::vec3& listenerPos, const glm::vec3& listenerDir, BSP* world);
    
    // Simple signature so entities can just call it with a path
    void PlayOneShot(const std::string& path, const glm::vec3& position);
    
    void PlayLooping(const std::string& path, const glm::vec3& position, float volume) {
        // Redirect to OneShot for now
        PlayOneShot(path, position); 
    }

private:
    ma_engine m_engine;
    const GameInfo* m_gameInfo = nullptr;
    std::vector<std::unique_ptr<SoundInstance>> m_activeSounds;
    std::mutex m_soundMutex;
};

} // namespace veex