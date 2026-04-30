#pragma once
// FuncDoor.h
// Example concrete entity showing the full VEEX I/O pattern.
// Registers inputs that the binfo wiring can call, and fires
// outputs that the binfo wiring routes to other entities.

#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/Logger.h"

namespace veex {

class FuncDoor : public Entity {
public:
    explicit FuncDoor(int id) : Entity(id, "func_door")
    {
        // Register all inputs this entity accepts.
        // These names must match what appears in the binfo InputName column.
        // Mirrors DEFINE_INPUTFUNC in Source.

        RegisterInput("SetOpen", [](Entity* self, const std::string& param) {
            auto* door = static_cast<FuncDoor*>(self);
            bool open = (param == "true" || param == "1");
            door->SetDoorOpen(open);
        });

        RegisterInput("PlaySound", [](Entity* self, const std::string& param) {
            // TODO: hook into audio system
            Logger::Info("FuncDoor id=" + std::to_string(self->GetID())
                + " PlaySound: " + param);
        });

        RegisterInput("Lock", [](Entity* self, const std::string&) {
            static_cast<FuncDoor*>(self)->m_locked = true;
            Logger::Info("FuncDoor id=" + std::to_string(self->GetID()) + " Locked.");
        });
    }

    bool Spawn(const EntityData& ed) override
    {
        // Read KVs from the BSP entity block.
        m_moveDir  = ed.GetFloat("movedir", 0.0f);
        m_speed    = ed.GetFloat("speed", 100.0f);
        m_openPos  = m_position + glm::vec3(0, ed.GetFloat("lip", 8.0f), 0);
        m_closePos = m_position;

        Logger::Info("FuncDoor id=" + std::to_string(m_id)
            + " spawned. speed=" + std::to_string(m_speed));
        return true;
    }

    void Update(float dt) override
    {
        if (m_isOpen) {
            // Slide toward open position
            glm::vec3 diff = m_openPos - m_position;
            float dist = glm::length(diff);
            if (dist > 0.01f) {
                m_position += glm::normalize(diff) * m_speed * dt;
            }
        } else {
            // Slide toward closed position
            glm::vec3 diff = m_closePos - m_position;
            float dist = glm::length(diff);
            if (dist > 0.01f) {
                m_position += glm::normalize(diff) * m_speed * dt;
            } else if (m_wasOpen) {
                // Just finished closing — fire OnClose so binfo can react
                FireOutput("OnClose", this);
                m_wasOpen = false;
            }
        }
    }

    // Called by a player/trigger system when something uses this door.
    void OnTrigger(Entity* activator) {
        if (m_locked) return;
        FireOutput("OnTrigger", activator); // binfo routes this
    }

private:
    void SetDoorOpen(bool open) {
        if (m_isOpen && !open) m_wasOpen = true;
        m_isOpen = open;
        Logger::Info("FuncDoor id=" + std::to_string(m_id)
            + (open ? " Opening." : " Closing."));
    }

    float     m_speed    = 100.0f;
    float     m_moveDir  = 0.0f;
    glm::vec3 m_openPos  { 0.0f };
    glm::vec3 m_closePos { 0.0f };
    bool      m_isOpen   = false;
    bool      m_wasOpen  = false;
    bool      m_locked   = false;
};

} // namespace veex


// ---- FuncDoor.cpp (put this content in FuncDoor.cpp) ----
// #include "veex/FuncDoor.h"
// namespace veex {
// VEEX_REGISTER_ENTITY("func_door", FuncDoor);
// } // namespace veex
