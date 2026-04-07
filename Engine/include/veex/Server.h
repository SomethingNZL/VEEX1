#pragma once
#include "veex/Entity.h"
#include "veex/EntityParser.h"
#include "veex/BinfoTypes.h"
#include "veex/GameInfo.h"
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace veex {

class Server {
public:
    Server();
    ~Server();

    bool Init(const std::string& entityData, const std::string& binfoPath, const GameInfo& game);
    void Tick(double dt);
    void Shutdown();

    void AddEntity(std::shared_ptr<Entity> entity);
    void RemoveEntity(int entityID); // Added this declaration

    const std::vector<std::shared_ptr<Entity>>& GetEntities() const;
    const std::vector<EntityData>& GetParsedEntities() const { return m_parsedEntities; }

    // Removed the { return ... } parts here to avoid redefinition errors
    glm::vec3   GetSpawnPoint() const;
    std::string GetSkyName()    const;
    double      GetSimTime()    const; // Added this declaration

private:
    // Logic system internal passes
    void ProcessWorldspawn();
    void InitializeLogicSystem();
    void SpawnEditorTools();
    void CleanupEntities();
    void RouteOutputs();

    // Changed to match the .cpp call (SpawnEntity(ed))
    bool SpawnEntity(const EntityData& ed);

    Entity* ResolveTarget(const std::string& targetName, Entity* self) const;
    std::string GetKV(const EntityData& ed, const std::string& key) const;

    double      m_simTime = 0.0;
    int         m_nextEntityID = 1; 
    glm::vec3   m_spawnPoint{ 0.0f };
    std::string m_skyName = "sky_day01_01";

    std::vector<std::shared_ptr<Entity>> m_entities;
    std::vector<EntityData>              m_parsedEntities;
    BinfoTable                           m_binfoTable;
};

} // namespace veex