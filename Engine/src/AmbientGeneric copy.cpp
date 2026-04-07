#include "veex/Entity.h"
#include "veex/SoundKit.h"

// NO NAMESPACE BLOCK HERE
class AmbientGeneric : public Entity {
public:
    AmbientGeneric(int id, const std::string& classname) : Entity(id, classname) {}

    void OnSpawn() override {
        std::string m_soundPath = GetRawKV("message");
        std::string healthStr = GetRawKV("health");
        float m_volume = healthStr.empty() ? 1.0f : (std::stof(healthStr) / 10.0f);
        
        int spawnflags = 0;
        try { 
            std::string sf = GetRawKV("spawnflags");
            if(!sf.empty()) spawnflags = std::stoi(sf); 
        } catch(...) {}

        bool m_looping = !(spawnflags & 32); 
        bool startSilent = (spawnflags & 1);

        if (!m_soundPath.empty() && m_looping && !startSilent) {
            // Use the exact path the compiler is suggesting in the error message
            ::veex::veex::SoundKit::Get().PlayLooping(m_soundPath, GetPosition(), m_volume);
        }
    }

    void AcceptInput(const std::string& inputName, const std::string& param, Entity* activator) override {
        if (inputName == "PlaySound") {
            // Use the exact path the compiler is suggesting
            ::veex::veex::SoundKit::Get().PlayOneShot(GetRawKV("message"), GetPosition());
        }
        Entity::AcceptInput(inputName, param, activator);
    }
};