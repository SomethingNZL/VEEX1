#pragma once
// Entity.h
// Base entity class with Source-style I/O system.
//
// How it mirrors Source:
//   FireOutput("OnTrigger", caller)   — equivalent to m_OnTrigger.FireOutput()
//   RegisterInput("SetOpen", fn)      — equivalent to DEFINE_INPUTFUNC()
//   The Server resolves target names and routes FireOutput to the right entity.

#include "veex/BinfoTypes.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>

namespace veex {

struct EntityData; // defined in EntityParser.h

class Entity;

// InputHandler: called when an input fires on this entity.
// param is the string from the binfo wire (may be empty).
using InputHandler = std::function<void(Entity* self, const std::string& param)>;

// ============================================================
//  Entity
// ============================================================
class Entity {
public:
    Entity(int id, const std::string& classname)
        : m_id(id), m_classname(classname), m_position(0.0f), m_angles(0.0f) {}

    virtual ~Entity() = default;

    // ---- Lifecycle -------------------------------------------------

    // Called once after construction and KV application.
    // Return false to veto spawning (entity is discarded).
    virtual bool Spawn(const EntityData&) { return true; }

    // Server tick.
    virtual void Update(float /*dt*/) {}

    // ---- I/O  (mirrors CBaseEntity::FireOutput / AcceptInput) ------

    // Fire a named output on this entity.
    // Pushes to the pending queue — Server drains and routes each tick.
    // e.g.  FireOutput("OnTrigger", this);
    void FireOutput(const std::string& outputName, Entity* activator = nullptr) {
        m_pendingOutputs.push_back({ outputName, activator });
    }

    // Accept a named input — called by the Server I/O router.
    // Dispatches to the registered InputHandler if found.
    // Mirrors CBaseEntity::AcceptInput.
    void AcceptInput(const std::string& inputName,
                     const std::string& param,
                     Entity* activator)
    {
        auto it = m_inputs.find(inputName);
        if (it != m_inputs.end()) {
            it->second(this, param);
        }
        // Unknown inputs are silently ignored, matching Source behaviour.
    }

    // Register a handler for a named input.
    // Call in derived constructor or Spawn().
    // Mirrors DEFINE_INPUTFUNC in Source.
    void RegisterInput(const std::string& inputName, InputHandler fn) {
        m_inputs[inputName] = std::move(fn);
    }

    // ---- Pending output queue — drained by Server each tick --------
    struct PendingOutput {
        std::string outputName;
        Entity*     activator = nullptr;
    };

    std::vector<PendingOutput>& GetPendingOutputs() { return m_pendingOutputs; }
    void ClearPendingOutputs() { m_pendingOutputs.clear(); }

    // ---- Common accessors ------------------------------------------
    int                GetID()         const { return m_id; }
    const std::string& GetClassname()  const { return m_classname; }
    const std::string& GetTargetName() const { return m_targetName; }

    void      SetPosition(const glm::vec3& p) { m_position = p; }
    glm::vec3 GetPosition()             const { return m_position; }

    void      SetAngles(const glm::vec3& a) { m_angles = a; }
    glm::vec3 GetAngles()              const { return m_angles; }

    void      SetTargetName(const std::string& n) { m_targetName = n; }

protected:
    int         m_id;
    std::string m_classname;
    std::string m_targetName; // "targetname" KV from BSP — used by I/O routing

    glm::vec3   m_position;
    glm::vec3   m_angles;

private:
    std::unordered_map<std::string, InputHandler> m_inputs;
    std::vector<PendingOutput>                    m_pendingOutputs;
};

// ============================================================
//  EntityFactory
// ============================================================
class EntityFactory {
public:
    using FactoryFn = std::function<std::shared_ptr<Entity>(int id)>;

    static void Register(const std::string& classname, FactoryFn fn) {
        Registry()[classname] = std::move(fn);
    }

    static std::shared_ptr<Entity> Create(const std::string& classname, int id) {
        auto& reg = Registry();
        auto  it  = reg.find(classname);
        if (it == reg.end()) return nullptr;
        return it->second(id);
    }

    static bool IsKnown(const std::string& classname) {
        return Registry().count(classname) > 0;
    }

private:
    static std::unordered_map<std::string, FactoryFn>& Registry() {
        static std::unordered_map<std::string, FactoryFn> s_registry;
        return s_registry;
    }
};

template<typename T>
struct EntityRegistrar {
    explicit EntityRegistrar(const std::string& classname) {
        EntityFactory::Register(classname, [](int id) -> std::shared_ptr<Entity> {
            return std::make_shared<T>(id);
        });
    }
};

#define VEEX_REGISTER_ENTITY(classname, Type) \
    static ::veex::EntityRegistrar<Type> s_reg_##Type(classname)

} // namespace veex
