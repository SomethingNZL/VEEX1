#include "veex/Server.h"
#include "veex/Logger.h"
#include "veex/EntityParser.h"
#include "veex/BinfoParser.h"
#include <sstream>

namespace veex {

Server::Server() : m_simTime(0.0) {}

std::string Server::GetKV(const EntityData& ed, const std::string& key) const
{
    auto it = ed.kv.find(key);
    return (it != ed.kv.end()) ? it->second : "";
}

Entity* Server::ResolveTarget(const std::string& targetName, Entity* self) const
{
    if (targetName == "self") return self;

    for (const auto& ent : m_entities) {
        if (ent && ent->GetTargetName() == targetName)
            return ent.get();
    }
    return nullptr;
}

void Server::RouteOutputs()
{
    for (auto& ent : m_entities) {
        if (!ent) continue;

        auto& pending = ent->GetPendingOutputs();
        if (pending.empty()) continue;

        auto itTable = m_binfoTable.find(ent->GetClassname());
        if (itTable == m_binfoTable.end()) {
            pending.clear();
            continue;
        }

        const auto& connections = itTable->second;
        for (const auto& fired : pending) {
            for (const auto& conn : connections) {
                if (conn.outputName == fired.outputName) {
                    Entity* target = ResolveTarget(conn.targetName, ent.get());
                    if (target) {
                        target->AcceptInput(conn.inputName, conn.param, ent.get());
                    }
                }
            }
        }
        pending.clear();
    }
}

bool Server::Init(const std::string& entityData, const std::string& binfoPath, const GameInfo& game)
{
    Logger::Info("Server: Initializing...");

    EntityParser parser;
    m_parsedEntities = parser.Parse(entityData);

    BinfoParser::LoadFromFile(binfoPath, game, m_binfoTable);

    for (const auto& ed : m_parsedEntities) {
        if (GetKV(ed, "classname") == "worldspawn") {
            std::string sky = GetKV(ed, "skyname");
            if (!sky.empty()) m_skyName = sky;
            break;
        }
    }

    int nextID = 1;
    for (const auto& ed : m_parsedEntities)
        SpawnEntity(ed, nextID);

    RouteOutputs();

    // Spawn point is now correctly scaled/swizzled by SpawnEntity
    auto test = std::make_shared<TestEntity>(nextID++);
    test->SetPosition(m_spawnPoint + glm::vec3(0.0f, 2.0f, -5.0f));
    AddEntity(test);

    Logger::Info("Server: Init complete. Live entities: " + std::to_string(m_entities.size()));
    return true;
}

void Server::Tick(double dt)
{
    m_simTime += dt;
    for (auto& ent : m_entities)
        if (ent) ent->Update(static_cast<float>(dt));

    RouteOutputs();
}

void Server::AddEntity(std::shared_ptr<Entity> entity)
{
    m_entities.push_back(std::move(entity));
}

const std::vector<std::shared_ptr<Entity>>& Server::GetEntities() const
{
    return m_entities;
}

void Server::Shutdown()
{
    m_entities.clear();
    m_parsedEntities.clear();
    m_binfoTable.clear();
}

bool Server::SpawnEntity(const EntityData& ed, int& nextID)
{
    std::string classname = GetKV(ed, "classname");
    if (classname.empty()) return false;

    if (classname == "worldspawn") return true;

    // Helper to parse "X Y Z" Source strings into VEEX/GL coordinate space
    auto parseSourcePos = [](const std::string& s) -> glm::vec3 {
        if (s.empty()) return glm::vec3(0.0f);
        std::stringstream ss(s);
        float x, y, z;
        ss >> x >> y >> z;

        // 1. Scale Hammer units to Engine meters
        const float SCALE = 1.0f / 32.0f;
        
        // 2. Swizzle: Source(X, Y, Z-Up) -> GL(X, Z, -Y-Backward)
        // This matches your PlayerController::SourceToGL logic
        return glm::vec3(x * SCALE, z * SCALE, -y * SCALE);
    };

    glm::vec3 origin = parseSourcePos(GetKV(ed, "origin"));
    
    // Parse angles (Pitch Yaw Roll) - no scale needed, just mapping
    std::stringstream ass(GetKV(ed, "angles"));
    float ap, ay, ar;
    ass >> ap >> ay >> ar;
    glm::vec3 angles(ap, ay, ar);

    std::shared_ptr<Entity> newEnt = nullptr;

    // Factory logic for players and generic entities
    if (classname == "info_player_start" || classname == "info_player_counterterrorist" || classname == "info_player_terrorist") {
        newEnt = std::make_shared<Entity>(nextID++, classname);
        m_spawnPoint = origin; 
    } 
    else {
        newEnt = std::make_shared<Entity>(nextID++, classname);
    }

    if (newEnt) {
        newEnt->SetPosition(origin);
        newEnt->SetAngles(angles);
        newEnt->SetTargetName(GetKV(ed, "targetname"));

        AddEntity(newEnt);
        return true;
    }

    return false;
}

} // namespace veex