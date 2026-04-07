#include "veex/SoundKit.h"
#include "veex/BSP.h"
#include "veex/Logger.h"
#include "veex/FileSystem.h"

namespace veex {

SoundKit::SoundKit() : m_gameInfo(nullptr) {}

SoundKit::~SoundKit() {
    Logger::Info("SoundKit: Shutting down audio engine.");
    ma_engine_uninit(&m_engine);
}

bool SoundKit::Initialize(const GameInfo& game) {
    m_gameInfo = &game;
    
    ma_result res = ma_engine_init(NULL, &m_engine);
    if (res != MA_SUCCESS) {
        Logger::Error("SoundKit: Failed to initialize miniaudio engine.");
        return false;
    }

    ma_uint32 sampleRate = ma_engine_get_sample_rate(&m_engine);
    Logger::Info("SoundKit: Audio engine started at " + std::to_string(sampleRate) + "Hz");
    return true;
}

void SoundKit::PlayOneShot(const std::string& path, const glm::vec3& position) {
    if (!m_gameInfo) {
        Logger::Error("SoundKit: PlayOneShot called before Initialize!");
        return;
    }

    // Use the explicit veex:: namespace to resolve the ResolveAssetPath call
    // This mirrors the logic in your MaterialSystem's FindMaterialPath
    std::string fullPath = veex::ResolveAssetPath(path, *m_gameInfo);
    
    if (fullPath.empty()) {
        // ResolveAssetPath already logs the specific failure path
        return;
    }

    auto instance = std::make_unique<SoundInstance>();
    instance->position = position;
    
    ma_result res = ma_sound_init_from_file(&m_engine, fullPath.c_str(), 0, NULL, NULL, &instance->sound);
    if (res != MA_SUCCESS) {
        Logger::Error("SoundKit: miniaudio failed to load: " + fullPath);
        return;
    }

    ma_sound_set_position(&instance->sound, position.x, position.y, position.z);

    // Initial LPF setup
    ma_uint32 engineSR = ma_engine_get_sample_rate(&m_engine);
    ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, 2, engineSR, 20000.0f, 2);
    ma_lpf_init(&lpfConfig, NULL, &instance->lpf);

    ma_node_attach_output_bus(&instance->sound, 0, &instance->lpf, 0);
    
    ma_sound_start(&instance->sound);

    std::lock_guard<std::mutex> lock(m_soundMutex);
    m_activeSounds.push_back(std::move(instance));
    
    Logger::Info("SoundKit: Playing " + path);
}

void SoundKit::Update(const glm::vec3& listenerPos, const glm::vec3& listenerDir, BSP* world) {
    if (!world) return;

    ma_engine_listener_set_position(&m_engine, 0, listenerPos.x, listenerPos.y, listenerPos.z);
    ma_engine_listener_set_direction(&m_engine, 0, listenerDir.x, listenerDir.y, listenerDir.z);

    ma_uint32 engineSR = ma_engine_get_sample_rate(&m_engine);

    std::lock_guard<std::mutex> lock(m_soundMutex);
    for (auto it = m_activeSounds.begin(); it != m_activeSounds.end();) {
        auto& instance = *it;

        if (!ma_sound_is_playing(&instance->sound)) {
            ma_sound_uninit(&instance->sound);
            ma_lpf_uninit(&instance->lpf, NULL); 
            it = m_activeSounds.erase(it);
            continue;
        }

        bool occluded = world->IsOccluded(listenerPos, instance->position);

        float targetCutoff = occluded ? 800.0f : 20000.0f;
        float targetVolume = occluded ? 0.6f : 1.0f;

        // Re-calculate filter for current hardware sample rate
        ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, 2, engineSR, targetCutoff, 2);
        ma_lpf_reinit(&lpfConfig, &instance->lpf);
        
        ma_sound_set_volume(&instance->sound, targetVolume);

        it++;
    }
}

} // namespace veex