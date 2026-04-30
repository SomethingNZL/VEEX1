#pragma once
#include "veex/Entity.h"
#include <string>

namespace veex {

class AmbientGeneric : public Entity {
public:
    AmbientGeneric(int id, const std::string& classname);

    void OnSpawn() override;
    void AcceptInput(const std::string& inputName, const std::string& param, Entity* activator) override;

private:
    std::string m_soundPath;
    float m_volume = 1.0f;
    bool m_looping = false;
};

} // namespace veex