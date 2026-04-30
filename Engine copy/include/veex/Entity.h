#pragma once
#include "veex/BinfoTypes.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <map>

namespace veex {

struct EntityData; 
class Entity;
class GameInfo;

// InputHandler: called when an input fires on this entity.
using InputHandler = std::function<void(Entity* self, const std::string& param)>;

// ============================================================
//  Entity
// ============================================================
class Entity {
public:
    Entity(int id, const std::string& classname)
        : m_id(id), m_classname(classname), m_position(0.0f), m_angles(0.0f), 
          m_active(true), m_pendingDeletion(false) {}

    virtual ~Entity() = default;

    // Global GameInfo access (set by Server during Init)
    static void SetGameInfo(const GameInfo* gameInfo) { s_gameInfo = gameInfo; }
    static const GameInfo* GetGameInfo() { return s_gameInfo; }

    // ---- Lifecycle -------------------------------------------------
    virtual void OnSpawn() {} 
    virtual bool Spawn(const EntityData&) { return true; }
    virtual void Update(float /*dt*/) {}

    // ---- I/O System (Mirrors Source Engine logic) ------------------
    struct PendingOutput {
        std::string outputName;
        Entity* activator = nullptr;
    };

    void FireOutput(const std::string& outputName, Entity* activator = nullptr) {
        m_pendingOutputs.push_back({ outputName, activator });
    }

    virtual void AcceptInput(const std::string& inputName, const std::string& param, Entity* activator) {
        auto it = m_inputs.find(inputName);
        if (it != m_inputs.end()) it->second(this, param);
    }

    void RegisterInput(const std::string& inputName, InputHandler fn) {
        m_inputs[inputName] = std::move(fn);
    }

    std::vector<PendingOutput>& GetPendingOutputs() { return m_pendingOutputs; }
    void ClearPendingOutputs() { m_pendingOutputs.clear(); }

    // ---- Lifecycle Glue (Server State Tracking) --------------------
    bool IsActive() const { return m_active; }
    void SetActive(bool active) { m_active = active; }
    
    bool IsPendingDeletion() const { return m_pendingDeletion; }
    void MarkForDeletion() { m_pendingDeletion = true; }

    // ---- KV Storage ------------------------------------------------
    void SetRawKV(const std::string& k, const std::string& v) { m_kvs[k] = v; }
    std::string GetRawKV(const std::string& k) { 
        auto it = m_kvs.find(k);
        return (it != m_kvs.end()) ? it->second : ""; 
    }

    // ---- Accessors -------------------------------------------------
    int                GetID()         const { return m_id; }
    const std::string& GetClassname()  const { return m_classname; }
    const std::string& GetTargetName() const { return m_targetName; }
    void               SetTargetName(const std::string& n) { m_targetName = n; }

    void      SetPosition(const glm::vec3& p) { m_position = p; }
    glm::vec3 GetPosition()             const { return m_position; }
    void      SetAngles(const glm::vec3& a)   { m_angles = a; }
    glm::vec3 GetAngles()              const { return m_angles; }

protected:
    int         m_id;
    std::string m_classname;
    std::string m_targetName;
    glm::vec3   m_position;
    glm::vec3   m_angles;

    bool m_active;
    bool m_pendingDeletion;
    std::map<std::string, std::string> m_kvs;

private:
    std::unordered_map<std::string, InputHandler> m_inputs;
    std::vector<PendingOutput>                    m_pendingOutputs;

    static inline const GameInfo* s_gameInfo = nullptr;
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
        auto it = reg.find(classname);
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

// Helper for static registration
template<typename T>
struct EntityRegistrar {
    explicit EntityRegistrar(const std::string& classname) {
        EntityFactory::Register(classname, [](int id) -> std::shared_ptr<Entity> {
            return std::make_shared<T>(id);
        });
    }
};

// Use this in .cpp files to hook into the factory
#define VEEX_REGISTER_ENTITY(classname, Type) \
    static ::veex::EntityRegistrar<Type> s_reg_##Type(classname)

} // namespace veex