#pragma once
// PropStaticEntity.h
// Example of a BSP entity that registers itself with the factory.
// ---------------------------------------------------------------
// To add a new entity type to the engine:
//   1. Derive from Entity.
//   2. Override Spawn(const EntityData&) to read your KVs.
//   3. Put VEEX_REGISTER_ENTITY in the matching .cpp file.
//   That is all — no changes to Server, EntityFactory, or any
//   other existing file are required.
// ---------------------------------------------------------------

#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/Logger.h"

namespace veex {

class PropStaticEntity : public Entity {
public:
    explicit PropStaticEntity(int id) : Entity(id, "prop_static") {}

    bool Spawn(const EntityData& ed) override
    {
        m_modelPath = ed.GetString("model");
        m_skin      = ed.GetInt("skin", 0);
        m_solid     = ed.GetInt("solid", 6);

        if (m_modelPath.empty()) {
            Logger::Warn("prop_static id=" + std::to_string(m_id) + " has no model KV — discarding.");
            return false; // tell Server to skip this entity
        }

        Logger::Info("prop_static: model='" + m_modelPath
            + "' skin=" + std::to_string(m_skin)
            + " solid=" + std::to_string(m_solid));
        return true;
    }

    const std::string& GetModelPath() const { return m_modelPath; }
    int                GetSkin()      const { return m_skin; }
    int                GetSolid()     const { return m_solid; }

private:
    std::string m_modelPath;
    int         m_skin  = 0;
    int         m_solid = 6;
};

} // namespace veex


// ---- PropStaticEntity.cpp ----------------------------------------
// (in the real project this lives in PropStaticEntity.cpp, not here)
// VEEX_REGISTER_ENTITY("prop_static", PropStaticEntity);
// ------------------------------------------------------------------
// The macro expands to a static EntityRegistrar<PropStaticEntity>
// whose constructor runs before main() and inserts the factory
// function into EntityFactory's registry.  Server::Init() will then
// find it automatically during the second-pass spawn loop.