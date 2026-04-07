#pragma once
// Server.h
#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/BinfoTypes.h"
#include "veex/GameInfo.h"
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace veex {

class TestEntity : public Entity {
public:
    explicit TestEntity(int id) : Entity(id, "test_moving_cube") {}
    void Update(float dt) override { m_position.x += 50.0f * dt; }
};

class Server {
public:
    Server();

    // entityData — raw BSP entity lump string
    // binfoPath  — path to Game/entities.binfo
    // game       — GameInfo reference for path resolution
    bool Init(const std::string& entityData, const std::string& binfoPath, const GameInfo& game);

    void Tick(double dt);
    void Shutdown();

    void AddEntity(std::shared_ptr<Entity> entity);
    const std::vector<std::shared_ptr<Entity>>& GetEntities() const;
    const std::vector<EntityData>& GetParsedEntities() const { return m_parsedEntities; }

    glm::vec3   GetSpawnPoint() const { return m_spawnPoint; }
    std::string GetSkyName()    const { return m_skyName;    }

private:
    bool SpawnEntity(const EntityData& ed, int& nextID);

    // Helper to safely extract a string from EntityData's KV map
    std::string GetKV(const EntityData& ed, const std::string& key) const;

    void RouteOutputs();
    Entity* ResolveTarget(const std::string& targetName, Entity* self) const;

    double      m_simTime = 0.0;
    glm::vec3   m_spawnPoint{ 0.0f };
    std::string m_skyName = "sky_day01_01";

    std::vector<std::shared_ptr<Entity>> m_entities;
    std::vector<EntityData>              m_parsedEntities;
    BinfoTable                           m_binfoTable;
};

} // namespace veex