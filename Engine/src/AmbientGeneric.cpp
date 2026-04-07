#include "veex/AmbientGeneric.h"
#include "veex/SoundKit.h"

namespace veex {

AmbientGeneric::AmbientGeneric(int id, const std::string& classname) 
    : Entity(id, classname) {}

void AmbientGeneric::OnSpawn() {
    m_soundPath = GetRawKV("message");
    std::string healthStr = GetRawKV("health");
    m_volume = healthStr.empty() ? 1.0f : (std::stof(healthStr) / 10.0f);
    
    int spawnflags = 0;
    try { 
        std::string sf = GetRawKV("spawnflags");
        if(!sf.empty()) spawnflags = std::stoi(sf); 
    } catch(...) {}

    m_looping = !(spawnflags & 32); 
    bool startSilent = (spawnflags & 1);

    if (!m_soundPath.empty() && m_looping && !startSilent) {
        SoundKit::Get().PlayLooping(m_soundPath, GetPosition(), m_volume);
    }
}

void AmbientGeneric::AcceptInput(const std::string& inputName, const std::string& param, Entity* activator) {
    if (inputName == "PlaySound") {
        SoundKit::Get().PlayOneShot(GetRawKV("message"), GetPosition());
    }
    Entity::AcceptInput(inputName, param, activator);
}

} // namespace veex